
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <shlobj.h>
#include "notebook.h"
#include "appearance.h"
#include "editor.h"

constexpr wchar_t appClass[] = L"Defter.Notebook.1";
constexpr int sidebarWidth = 238, topHeight = 45, listTop = 144, rowHeight = 36, footerHeight = 25;
constexpr int studyHeader = 24, splitWidth = 7;
constexpr UINT_PTR saveTimer = 1, styleTimer = 2;
constexpr UINT blockMenuMessage = WM_APP + 1;

int wheelSteps(int& remainder, int delta) {
    const int total = remainder + delta;
    remainder = total % WHEEL_DELTA;
    return total / WHEEL_DELTA;
}

struct EditorScroll {
    SIZE extent{};
    int horizontal = 0, vertical = 0;
    bool measured = false;
};

struct Box {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int a, int b) const { return a >= x && a < x + w && b >= y && b < y + h; }
};

enum class Glyph { Note, Code, Study, Search, Plus, Chevron, More, Sidebar, Sun, Moon, Blocks, Close };
enum class Action { Add, Blocks, Language, More, Row, Clear, Split, ZoomOut, ZoomReset, ZoomIn, None };
struct Hit {
    Action action = Action::None;
    uint32_t item = 0;
    bool operator==(const Hit&) const = default;
};

struct App;
LRESULT CALLBACK windowProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK editProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);


Glyph kindGlyph(Kind kind) {
    constexpr Glyph icons[] = {Glyph::Note, Glyph::Code, Glyph::Study};
    return icons[static_cast<int>(kind)];
}

struct Canvas {
    HDC dc;
    UINT dpi;
    int s(int n) const { return MulDiv(n, dpi, 96); }
    RECT rect(Box b) const { return {s(b.x), s(b.y), s(b.x + b.w), s(b.y + b.h)}; }

    void fill(Box b, COLORREF color) const {
        RECT r = rect(b);
        SetDCBrushColor(dc, color);
        FillRect(dc, &r, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    }
    void round(Box b, int radius, COLORREF color, COLORREF border = CLR_INVALID) const {
        auto oldBrush = SelectObject(dc, GetStockObject(DC_BRUSH));
        auto oldPen = SelectObject(dc, GetStockObject(border == CLR_INVALID ? NULL_PEN : DC_PEN));
        SetDCBrushColor(dc, color);
        SetDCPenColor(dc, border == CLR_INVALID ? color : border);
        const RECT r = rect(b);
        RoundRect(dc, r.left, r.top, r.right, r.bottom, s(radius * 2), s(radius * 2));
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
    }
    void text(View value, Box b, HFONT font, COLORREF color, UINT extra = 0) const {
        if (value.empty() || b.w <= 0 || b.h <= 0) return;
        RECT r = rect(b);
        auto old = SelectObject(dc, font);
        SetTextColor(dc, color);
        SetBkMode(dc, TRANSPARENT);
        DrawTextW(dc, value.data(), static_cast<int>(value.size()), &r,
            DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX | extra);
        SelectObject(dc, old);
    }
    void glyph(Glyph glyph, int x, int y, COLORREF color) const {
        HPEN pen = CreatePen(PS_SOLID, std::max(1, s(1)), color);
        auto oldPen = SelectObject(dc, pen);
        auto oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        auto path = [&](std::initializer_list<POINT> vertices) {
            POINT points[12]{};
            int i = 0;
            for (POINT v : vertices) points[i++] = {s(x + v.x), s(y + v.y)};
            Polyline(dc, points, i);
        };
        auto ring = [&](int l, int t, int r, int b) { Ellipse(dc, s(x + l), s(y + t), s(x + r), s(y + b)); };
        auto frame = [&](int l, int t, int r, int b) { RoundRect(dc, s(x + l), s(y + t), s(x + r), s(y + b), s(3), s(3)); };
        switch (glyph) {
        case Glyph::Note:
            path({{4,2},{10,2},{14,6},{14,15},{4,15},{4,2}});
            path({{10,2},{10,6},{14,6}}); path({{6,9},{12,9}}); path({{6,12},{11,12}});
            break;
        case Glyph::Code:
            path({{6,5},{2,9},{6,13}}); path({{12,5},{16,9},{12,13}}); path({{10,4},{8,14}});
            break;
        case Glyph::Study:
            frame(1,3,17,15); path({{9,3},{9,15}}); path({{3,6},{7,6}}); path({{11,6},{15,6}});
            break;
        case Glyph::Search:
            ring(2,2,12,12); path({{10,10},{15,15}});
            break;
        case Glyph::Plus:
            path({{3,9},{15,9}}); path({{9,3},{9,15}});
            break;
        case Glyph::Chevron:
            path({{5,7},{9,11},{13,7}});
            break;
        case Glyph::More:
            SelectObject(dc, GetStockObject(DC_BRUSH));
            SetDCBrushColor(dc, color);
            ring(3,8,5,10); ring(8,8,10,10); ring(13,8,15,10);
            break;
        case Glyph::Sidebar:
            frame(1, 3, 17, 15);
            path({{7, 3}, {7, 15}});
            path({{3, 6}, {5, 6}}); path({{3, 9}, {5, 9}});
            break;
        case Glyph::Sun:
            ring(5, 5, 13, 13);
            path({{9, 1}, {9, 3}}); path({{9, 15}, {9, 17}});
            path({{1, 9}, {3, 9}}); path({{15, 9}, {17, 9}});
            path({{3, 3}, {4, 4}}); path({{14, 14}, {15, 15}});
            path({{14, 4}, {15, 3}}); path({{3, 15}, {4, 14}});
            break;
        case Glyph::Moon:
            path({{10, 2}, {6, 3}, {3, 6}, {2, 10}, {4, 14}, {8, 16}, {12, 15}, {15, 12}, {16, 9},
                  {13, 11}, {9, 9}, {8, 5}});
            path({{8, 5}, {10, 2}});
            break;
        case Glyph::Blocks:
            frame(2,2,8,8); frame(10,2,16,8); frame(2,10,8,16);
            path({{10,13},{16,13}}); path({{13,10},{13,16}});
            break;
        case Glyph::Close:
            path({{5,5},{13,13}}); path({{13,5},{5,13}});
            break;
        }
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
    }
};

struct App {
    HWND hwnd{}, query{}, title{}, prose{}, code{}, reference{}, attempt{}, sidebarButton{}, themeButton{};
    HINSTANCE instance{};
    Notebook book;
    Preferences preferences;
    Text search;
    std::vector<size_t> visible;
    HFONT captionFont{}, regular{}, medium{}, section{}, logo{}, headline{}, mono{}, bodyFont{};
    HBRUSH paperBrush{}, sidebarBrush{};
    UINT dpi = 96;
    int w = 1100, h = 700, scroll = 0, splitRatio = 50;
    bool fullScreen = false, fullScreenSidebar = false;
    LONG_PTR windowStyle = 0, windowExStyle = 0;
    WINDOWPLACEMENT windowPlacement{sizeof(windowPlacement)};
    int wheelDelta = 0;
    std::array<EditorScroll, 4> editorScroll;
    bool changing = false, dirty = false, savingFailed = false, dragging = false, blockedStorage = false;
    int pendingStyle = 0; ULONGLONG dirtySince = 0;
    Hit hovered;
    std::array<int, 3> match{};
    Box sidebarButtonBox, themeBox, searchBox, clearBox, addBox, listBox, textBox, titleBox, menuBox, moreBox, splitBox, codeBox, leftBox, rightBox, zoomOutBox, zoomResetBox, zoomInBox;

    const Palette& colors() const { return preferences.dark ? darkPalette : lightPalette; }
    bool sidebarOpen() const { return fullScreen ? fullScreenSidebar : preferences.sidebar; }
    int workspaceLeft() const { return sidebarOpen() ? sidebarWidth : 0; }
    int zoomUnits(int size) const { return MulDiv(size, preferences.zoom, defaultZoom); }
    int px(int n) const { return MulDiv(n, dpi, 96); }
    int logical(int n) const { return MulDiv(n, 96, dpi); }
    Note* note() {
        for (auto& item : book.notes) if (item.id == book.active) return &item;
        return nullptr;
    }

    void initStorage(bool test) {
        Text legacy;
        if (test) {
            wchar_t exe[32768];
            const DWORD length = GetModuleFileNameW(nullptr, exe, static_cast<DWORD>(std::size(exe)));
            Text folder(exe, length);
            book.path = folder.substr(0, folder.find_last_of(L'\\') + 1) + L"test-notes.ypr";
        } else {
            PWSTR folder = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &folder))) {
                Text directory(folder);
                CoTaskMemFree(folder);
                legacy = directory + L"\\Yaprak\\defter.ypr";
                directory += L"\\Defter";
                if (CreateDirectoryW(directory.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS)
                    book.path = directory + L"\\defter.ypr";
            }
        }
        if (!book.path.empty()) {
            if (book.read(book.path)) return;
            if (book.read(book.path + L".bak")) {
                book.preserveBackup = true;
                dirty = true;
                return;
            }
            blockedStorage = GetFileAttributesW(book.path.c_str()) != INVALID_FILE_ATTRIBUTES
                || GetFileAttributesW((book.path + L".bak").c_str()) != INVALID_FILE_ATTRIBUTES;
            if (!blockedStorage && !legacy.empty() && (book.read(legacy) || book.read(legacy + L".bak"))) {
                dirty = true;
                return;
            }
        }
        savingFailed = book.path.empty() || blockedStorage;
        Note welcome;
        welcome.id = book.nextId++;
        welcome.title = L"Başlangıç";
        welcome.language = 1;
        welcome.body = L"Fikirlerin, günlük notların ve kod parçaların için küçük bir alan.\r\r"
            L"## Hızlı başlangıç\r"
            L"• Yeni not menüsünden metin, kod veya çalışma notu aç.\r"
            L"• Boş satıra /code yazıp Enter’a basarak bir kod bloğu ekle.\r"
            L"• Kod çalışmasında referansı sola, kendi yazdığını sağa koy.\r\r";
        welcome.body += fenceMark;
        welcome.body += L"python\rdef merhaba(isim):\r    return f\"Merhaba, {isim}!\"\r";
        welcome.body += fenceMark;
        welcome.body += L'\r';
        book.active = welcome.id;
        book.notes.push_back(std::move(welcome));
    }

    HFONT font(int size, int weight = FW_NORMAL, const wchar_t* face = uiFace) const {
        return CreateFontW(-px(size), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    }
    void releaseFonts() {
        for (auto handle : {captionFont, regular, medium, section, logo, headline, mono, bodyFont})
            if (handle) DeleteObject(handle);
    }
    void fonts() {
        auto previous = std::array{captionFont, regular, medium, section, logo, headline, mono, bodyFont};
        captionFont = font(12); regular = font(14); medium = font(14, FW_SEMIBOLD);
        section = font(11, FW_BOLD); logo = font(21, FW_BOLD); headline = font(19, FW_SEMIBOLD);
        mono = font(zoomUnits(14), FW_NORMAL, codeFace); bodyFont = font(zoomUnits(17));
        SendMessageW(query, WM_SETFONT, reinterpret_cast<WPARAM>(regular), FALSE);
        SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(headline), FALSE);
        SendMessageW(title, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(0, 0));
        for (HFONT old : previous) if (old) DeleteObject(old);
    }
    HWND createEdit(bool rich, DWORD style, int id) {
        HWND edit = CreateWindowExW(0, rich ? MSFTEDIT_CLASS : L"EDIT", L"",
            WS_CHILD | WS_TABSTOP | style, 0, 0, 0, 0, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
        if (edit) SetWindowSubclass(edit, editProc, id, reinterpret_cast<DWORD_PTR>(this));
        return edit;
    }
    bool createControls() {
        query = createEdit(false, ES_AUTOHSCROLL, 1);
        title = createEdit(false, ES_AUTOHSCROLL, 2);
        prose = createEdit(true, ES_MULTILINE | ES_AUTOVSCROLL | ES_NOHIDESEL, 3);
        code = createEdit(true, ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL, 4);
        reference = createEdit(true, ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL, 5);
        attempt = createEdit(true, ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL, 6);
        sidebarButton = createButton(7);
        themeButton = createButton(8);
        if (!query || !title || !prose || !code || !reference || !attempt || !sidebarButton || !themeButton) return false;
        SendMessageW(query, EM_LIMITTEXT, 200, 0);
        SendMessageW(query, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Notlarda ara"));
        SendMessageW(title, EM_LIMITTEXT, 140, 0);
        SendMessageW(title, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"Başlıksız"));
        for (auto edit : {prose, code, reference, attempt}) {
            SendMessageW(edit, EM_EXLIMITTEXT, 0, maxEditSize);
            SendMessageW(edit, EM_SETUNDOLIMIT, 100, 0);
            SendMessageW(edit, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SCROLL);
            SendMessageW(edit, EM_SETEDITSTYLE, SES_EXTENDBACKCOLOR, SES_EXTENDBACKCOLOR);
            SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(0, px(9)));
        }
        applyTheme();
        fonts();
        return true;
    }
    HWND mainEditor() {
        const auto* current = note();
        if (!current) return sidebarOpen() ? query : sidebarButton;
        if (current->kind == Kind::Study) return attempt;
        return current->kind == Kind::Code ? code : prose;
    }
    static Text plain(HWND edit) {
        const int length = GetWindowTextLengthW(edit);
        Text value(static_cast<size_t>(length) + 1, L'\0');
        value.resize(GetWindowTextW(edit, value.data(), length + 1));
        return value;
    }
    void invalidate(Box b) const {
        RECT r{px(b.x), px(b.y), px(b.x + b.w), px(b.y + b.h)};
        InvalidateRect(hwnd, &r, FALSE);
    }
    int maxScroll() const {
        return std::max(0, static_cast<int>(visible.size()) - std::max(1, listBox.h / rowHeight));
    }
    void filter(bool reveal = false) {
        search = lower(plain(query));
        visible.clear();
        for (size_t i = 0; i < book.notes.size(); ++i) {
            const auto& item = book.notes[i];
            if (search.empty() || lower(label(item)).find(search) != Text::npos
                || lower(item.body).find(search) != Text::npos || lower(item.practice).find(search) != Text::npos)
                visible.push_back(i);
        }
        if (reveal) {
            const int rows = std::max(1, listBox.h / rowHeight);
            for (int i = 0; i < static_cast<int>(visible.size()); ++i) {
                if (book.notes[visible[i]].id != book.active) continue;
                if (i < scroll) scroll = i;
                if (i >= scroll + rows) scroll = i - rows + 1;
                break;
            }
        }
        scroll = std::clamp(scroll, 0, maxScroll());
        if (sidebarOpen()) {
            invalidate({0, 105, sidebarWidth, h - 105});
            invalidate(searchBox);
        }
    }
    void move(HWND control, Box b, bool show = true) const {
        if (!show) {
            ShowWindow(control, SW_HIDE);
            return;
        }
        SetWindowPos(control, nullptr, px(b.x), px(b.y), std::max(1, px(b.x + b.w) - px(b.x)),
            std::max(1, px(b.y + b.h) - px(b.y)), SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    Box panelEditor(Box panel, int header = 0) const {
        const int numbers = zoomUnits(39);
        return {panel.x + numbers + 8, panel.y + header + 5, panel.w - numbers - 13, panel.h - header - 7};
    }

    Box gutter(Box panel, int header = 0) const {
        return {panel.x, panel.y + header + 5, zoomUnits(39), panel.h - header - 7};
    }

    void invalidateEditorExtents() {
        for (auto& state : editorScroll) state.measured = false;
    }

    void layout() {
        invalidateEditorExtents();
        RECT client{};
        GetClientRect(hwnd, &client);
        w = logical(client.right);
        h = logical(client.bottom);
        const int left = workspaceLeft();
        sidebarButtonBox = {left + 8, 6, 34, 33};
        themeBox = {w - 118, 6, 110, 33};
        searchBox = {16, 13, sidebarWidth - 32, 34};
        clearBox = {sidebarWidth - 41, 16, 21, 27};
        addBox = sidebarOpen() ? Box{16, 58, sidebarWidth - 32, 36} : Box{47, 6, 34, 33};
        listBox = {10, listTop, sidebarWidth - 20, std::max(rowHeight, h - listTop - 12)};
        scroll = std::clamp(scroll, 0, maxScroll());
        move(sidebarButton, sidebarButtonBox);
        move(themeButton, themeBox);
        move(query, {searchBox.x + 31, searchBox.y + 7, searchBox.w - 71, 21}, sidebarOpen());
        moreBox = {themeBox.x - 36, 6, 33, 33};
        menuBox = {moreBox.x - 140, 6, 134, 33};
        auto* current = note();
        const bool normal = current && current->kind == Kind::Note;
        const bool coding = current && current->kind == Kind::Code;
        const bool studying = current && current->kind == Kind::Study;
        const int width = w - left;
        const int titleLeft = left + (sidebarOpen() ? 53 : 91);
        titleBox = {titleLeft, 10, std::max(1, menuBox.x - titleLeft - 9), 26};
        textBox = {left + 13, topHeight + 8, width - 24, std::max(1, h - topHeight - footerHeight - 11)};
        codeBox = {left, topHeight, width, std::max(1, h - topHeight - footerHeight)};
        const int leftWidth = (width - splitWidth) * splitRatio / 100;
        leftBox = {left, codeBox.y, leftWidth, codeBox.h};
        rightBox = {left + leftWidth + splitWidth, codeBox.y, width - splitWidth - leftWidth, codeBox.h};
        splitBox = {left + leftWidth, codeBox.y, splitWidth, codeBox.h};
        zoomOutBox = {w - 114, h - footerHeight + 1, 24, footerHeight - 1};
        zoomResetBox = {w - 90, h - footerHeight + 1, 58, footerHeight - 1};
        zoomInBox = {w - 32, h - footerHeight + 1, 24, footerHeight - 1};
        move(title, titleBox, current != nullptr);
        move(prose, textBox, normal);
        move(code, panelEditor(codeBox), coding);
        move(reference, panelEditor(leftBox, studyHeader), studying);
        move(attempt, panelEditor(rightBox, studyHeader), studying);
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    void loadEditor(HWND edit, const Text& value, bool normal, int lang) {
        SetWindowTextW(edit, value.c_str());
        setupEditor(edit, normal, colors());
        SendMessageW(edit, EM_SETZOOM, preferences.zoom, defaultZoom);
        formatEditor(edit, value, normal, lang, colors());
        SendMessageW(edit, EM_EMPTYUNDOBUFFER, 0, 0);
    }
    void loadActive(bool focusTitle = false) {
        KillTimer(hwnd, styleTimer);
        pendingStyle = 0;
        editorScroll = {};
        changing = true;
        Note* current = note();
        for (auto edit : {prose, code, reference, attempt}) SetWindowTextW(edit, L"");
        SetWindowTextW(title, current ? current->title.c_str() : L"");
        match = {};
        layout();
        if (current) {
            if (current->kind == Kind::Note) loadEditor(prose, current->body, true, current->language);
            else if (current->kind == Kind::Code) loadEditor(code, current->body, false, current->language);
            else {
                loadEditor(reference, current->body, false, current->language);
                loadEditor(attempt, current->practice, false, current->language);
                match = compareLines(current->body, current->practice);
            }
        }
        changing = false;
        filter(true);
        if (focusTitle) {
            SetFocus(title);
            SendMessageW(title, EM_SETSEL, 0, -1);
        }
    }
    void markDirty() {
        dirty = true;
        const ULONGLONG now = GetTickCount64();
        if (!dirtySince) dirtySince = now;
        const ULONGLONG elapsed = now - dirtySince;
        SetTimer(hwnd, saveTimer, elapsed >= 2800 ? 10 : static_cast<UINT>(std::min<ULONGLONG>(700, 2800 - elapsed)), nullptr);
        invalidate({workspaceLeft(), h - footerHeight, w - workspaceLeft(), footerHeight});
    }
    void save() {
        KillTimer(hwnd, saveTimer);
        if (!dirty) return;
        savingFailed = blockedStorage || !book.save();
        dirty = savingFailed;
        dirtySince = 0;
        invalidate({workspaceLeft(), h - footerHeight, w - workspaceLeft(), footerHeight});
    }
    void scheduleStyle(int mask) {
        pendingStyle |= mask;
        SetTimer(hwnd, styleTimer, 210, nullptr);
    }
    void invalidateGutters() const {
        if (IsWindowVisible(code)) invalidate(gutter(codeBox));
        if (IsWindowVisible(reference)) invalidate(gutter(leftBox, studyHeader));
        if (IsWindowVisible(attempt)) invalidate(gutter(rightBox, studyHeader));
    }
    void style() {
        KillTimer(hwnd, styleTimer);
        const int mask = pendingStyle;
        pendingStyle = 0;
        const auto* current = note();
        if (!current) return;
        changing = true;
        if (mask & 1) formatEditor(prose, current->body, true, current->language, colors());
        if (mask & 2) formatEditor(code, current->body, false, current->language, colors());
        if (mask & 4) formatEditor(reference, current->body, false, current->language, colors());
        if (mask & 8) formatEditor(attempt, current->practice, false, current->language, colors());
        changing = false;
        invalidateEditorExtents();
        invalidateGutters();
    }
    void changed(int id) {
        if (changing) return;
        if (id == 1) {
            scroll = 0;
            filter();
            return;
        }
        Note* current = note();
        if (!current) return;
        if (id == 2) {
            current->title = plain(title);
            filter();
            invalidate({workspaceLeft(), 0, w - workspaceLeft(), topHeight});
        } else {
            HWND control = id == 3 ? prose : id == 4 ? code : id == 5 ? reference : attempt;
            auto& value = id == 6 ? current->practice : current->body;
            value = editText(control);
            editorScroll[id - 3].measured = false;
            scheduleStyle(1 << (id - 3));
            if (current->kind == Kind::Study) match = compareLines(current->body, current->practice);
            if (!search.empty()) filter();
        }
        markDirty();
    }
    void choose(uint32_t id) {
        if (!id || id == book.active) return;
        book.active = id;
        loadActive();
        SetFocus(mainEditor());
        markDirty();
    }
    void add(Kind kind, const Note* original = nullptr) {
        if (book.notes.size() >= 10000 || book.nextId == UINT32_MAX) {
            MessageBoxW(hwnd, L"Bu defterin not sınırına ulaşıldı.", appName, MB_OK | MB_ICONINFORMATION);
            return;
        }
        Note created = original ? *original : Note{};
        created.id = book.nextId++;
        created.kind = kind;
        if (original) {
            created.title = label(*original);
            if (created.title.size() > 130) created.title.resize(130);
            created.title += L" (kopya)";
        } else created.title = kind == Kind::Note ? L"Yeni not" : kind == Kind::Code ? L"Yeni kod notu" : L"Yeni kod çalışması";
        book.active = created.id;
        book.notes.insert(book.notes.begin(), std::move(created));
        SetWindowTextW(query, L"");
        scroll = 0;
        loadActive(true);
        markDirty();
    }

    HWND createButton(int id) const {
        return CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    }

    void applyTheme() {
        const auto& c = colors();
        const HBRUSH nextPaper = CreateSolidBrush(c.paper);
        const HBRUSH nextSidebar = CreateSolidBrush(c.side);
        if (paperBrush) DeleteObject(paperBrush);
        if (sidebarBrush) DeleteObject(sidebarBrush);
        paperBrush = nextPaper;
        sidebarBrush = nextSidebar;

        const BOOL dark = preferences.dark;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &c.side, sizeof(c.side));
        DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &c.ink, sizeof(c.ink));
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &c.border, sizeof(c.border));
        SetWindowTextW(themeButton, preferences.dark ? L"Açık mod" : L"Koyu mod");
        updateSidebarButton();

        SendMessageW(prose, EM_SETBKGNDCOLOR, 0, c.paper);
        for (HWND edit : {code, reference, attempt})
            SendMessageW(edit, EM_SETBKGNDCOLOR, 0, c.codePaper);
    }

    void toggleTheme() {
        preferences.dark = !preferences.dark;
        applyTheme();
        if (const auto* current = note()) {
            pendingStyle |= current->kind == Kind::Note ? 1 : current->kind == Kind::Code ? 2 : 12;
            style();
        }
        RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
        preferences.save(book.path);
    }

    void updateSidebarButton() const {
        SetWindowTextW(sidebarButton, sidebarOpen() ? L"Sol menüyü kapat" : L"Sol menüyü aç");
    }

    void toggleSidebar() {
        const bool moveFocus = GetFocus() == query;
        if (fullScreen) fullScreenSidebar = !fullScreenSidebar;
        else {
            preferences.sidebar = !preferences.sidebar;
            preferences.save(book.path);
        }
        updateSidebarButton();
        hovered = {};
        layout();
        if (moveFocus && !sidebarOpen()) SetFocus(mainEditor());
    }

    void setZoom(int zoom) {
        if (preferences.zoom == zoom) return;
        preferences.zoom = zoom;
        const HFONT oldMono = mono, oldBody = bodyFont;
        mono = font(zoomUnits(14), FW_NORMAL, codeFace);
        bodyFont = font(zoomUnits(17));
        DeleteObject(oldMono);
        DeleteObject(oldBody);
        for (HWND edit : {prose, code, reference, attempt}) {
            SendMessageW(edit, EM_SETZOOM, zoom, defaultZoom);
            InvalidateRect(edit, nullptr, FALSE);
        }
        layout();
        preferences.save(book.path);
    }

    void zoomBy(int steps) {
        const auto current = std::lower_bound(std::begin(zoomLevels), std::end(zoomLevels), preferences.zoom);
        const int index = static_cast<int>(current - std::begin(zoomLevels));
        setZoom(zoomLevels[std::clamp(index + steps, 0, static_cast<int>(std::size(zoomLevels)) - 1)]);
    }

    void zoomWheel(int delta) {
        if (const int steps = wheelSteps(wheelDelta, delta)) zoomBy(steps);
    }

    HWND editorAt(POINT screen) {
        ScreenToClient(hwnd, &screen);
        const int x = logical(screen.x), y = logical(screen.y);
        const auto* current = note();
        if (!current || !codeBox.contains(x, y)) return nullptr;
        if (current->kind == Kind::Note) return prose;
        if (current->kind == Kind::Code) return code;
        if (leftBox.contains(x, y)) return reference;
        if (rightBox.contains(x, y)) return attempt;
        return nullptr;
    }

    void rememberExtent(const REQRESIZE& resize) {
        const UINT_PTR id = resize.nmhdr.idFrom;
        if (id < 3 || id > 6) return;
        auto& state = editorScroll[id - 3];
        state.extent = {resize.rc.right - resize.rc.left, resize.rc.bottom - resize.rc.top};
        state.measured = true;
    }

    void scrollEditor(HWND edit, UINT message, WPARAM wheel) {
        const bool tilt = message == WM_MOUSEHWHEEL;
        const bool horizontal = tilt || (GET_KEYSTATE_WPARAM(wheel) & MK_SHIFT);
        if (horizontal && edit == prose) return;
        auto& state = editorScroll[GetDlgCtrlID(edit) - 3];
        int& remainder = horizontal ? state.horizontal : state.vertical;
        const int delta = GET_WHEEL_DELTA_WPARAM(wheel);
        const int steps = wheelSteps(remainder, tilt ? delta : -delta);
        if (!steps) return;

        UINT units = 3;
        SystemParametersInfoW(horizontal ? SPI_GETWHEELSCROLLCHARS : SPI_GETWHEELSCROLLLINES, 0, &units, 0);
        if (!units) return;
        if (!state.measured) {
            const auto events = SendMessageW(edit, EM_GETEVENTMASK, 0, 0);
            SendMessageW(edit, EM_SETEVENTMASK, 0, events | ENM_REQUESTRESIZE);
            SendMessageW(edit, EM_REQUESTRESIZE, 0, 0);
            SendMessageW(edit, EM_SETEVENTMASK, 0, events);
        }

        RECT client{};
        GetClientRect(edit, &client);
        const LONG viewport = horizontal ? client.right : client.bottom;
        const LONG extent = horizontal ? state.extent.cx : state.extent.cy;
        const LONG limit = std::max<LONG>(0, extent - viewport);
        if (!limit) return;

        POINT position{};
        SendMessageW(edit, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&position));
        LONG& axis = horizontal ? position.x : position.y;
        const int pixelsPerUnit = px(zoomUnits(horizontal ? 8 : edit == prose ? 24 : 20));
        const int64_t amount = units == WHEEL_PAGESCROLL ? viewport : int64_t(units) * pixelsPerUnit;
        const LONG next = static_cast<LONG>(std::clamp<int64_t>(int64_t(axis) + steps * amount, 0, limit));
        if (axis == next) return;
        axis = next;
        SendMessageW(edit, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&position));
        if (!horizontal && edit != prose) invalidateGutters();
    }

    void toggleFullScreen() {
        constexpr UINT placementFlags = SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_NOZORDER;
        const bool focusWasSearch = GetFocus() == query;
        if (!fullScreen) {
            MONITORINFO monitor{sizeof(monitor)};
            if (!GetWindowPlacement(hwnd, &windowPlacement)
                || !GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor)) return;
            windowStyle = GetWindowLongPtrW(hwnd, GWL_STYLE);
            windowExStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            fullScreen = true;
            fullScreenSidebar = false;
            SetWindowLongPtrW(hwnd, GWL_STYLE, windowStyle & ~(WS_OVERLAPPEDWINDOW | WS_MAXIMIZE));
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, windowExStyle & ~WS_EX_DLGMODALFRAME);
            const RECT r = monitor.rcMonitor;
            SetWindowPos(hwnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, placementFlags);
        } else {
            fullScreen = false;
            SetWindowLongPtrW(hwnd, GWL_STYLE, windowStyle);
            SetWindowLongPtrW(hwnd, GWL_EXSTYLE, windowExStyle);
            SetWindowPlacement(hwnd, &windowPlacement);
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, placementFlags | SWP_NOMOVE | SWP_NOSIZE);
        }
        hovered = {};
        updateSidebarButton();
        layout();
        if (focusWasSearch && !sidebarOpen()) SetFocus(mainEditor());
    }

#include "app_ui.inl"
};
#include "app_events.inl"
