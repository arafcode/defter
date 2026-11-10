
bool pressed(int key) { return (GetKeyState(key) & 0x8000) != 0; }

bool keyboardSymbol(WPARAM key, wchar_t symbol, bool shift) {
    const SHORT mapping = VkKeyScanExW(symbol, GetKeyboardLayout(0));
    if (mapping == -1 || LOBYTE(mapping) != key) return false;
    return HIBYTE(mapping) == (shift ? 1 : 0);
}

bool handleZoomWheel(App& app, WPARAM wheel) {
    if (!(GET_KEYSTATE_WPARAM(wheel) & MK_CONTROL)) return false;
    app.zoomWheel(GET_WHEEL_DELTA_WPARAM(wheel));
    return true;
}

bool shortcut(App& app, HWND edit, WPARAM key) {
    const bool ctrl = pressed(VK_CONTROL), shift = pressed(VK_SHIFT), alt = pressed(VK_MENU);
    if (ctrl && !alt) {
        if (!shift && (key == '0' || key == VK_NUMPAD0)) {
            app.setZoom(defaultZoom);
            return true;
        }
        if (key == VK_ADD || key == VK_OEM_PLUS || keyboardSymbol(key, L'+', shift) || keyboardSymbol(key, L'=', shift)) {
            app.zoomBy(1);
            return true;
        }
        if (key == VK_SUBTRACT || key == VK_OEM_MINUS || keyboardSymbol(key, L'-', shift)) {
            app.zoomBy(-1);
            return true;
        }
    }
    if (key == VK_F11 && !ctrl && !alt && !shift) {
        app.toggleFullScreen();
        return true;
    }
    if (ctrl && key == 'N' && !(shift && alt) && !pressed(VK_RMENU)) {
        app.add(alt ? Kind::Study : shift ? Kind::Code : Kind::Note);
        return true;
    }
    if (ctrl && !alt && shift && key == 'B') {
        app.toggleSidebar();
        return true;
    }
    if (ctrl && !alt && shift && key == 'L') {
        app.toggleTheme();
        return true;
    }
    if (ctrl && !alt && !shift && key == 'K') {
        if (!app.sidebarOpen()) app.toggleSidebar();
        SetFocus(app.query);
        SendMessageW(app.query, EM_SETSEL, 0, -1);
        return true;
    }
    if (ctrl && !alt && !shift && key == 'S') {
        app.save();
        return true;
    }
    if (key == VK_F2 && app.note()) {
        SetFocus(app.title);
        SendMessageW(app.title, EM_SETSEL, 0, -1);
        return true;
    }
    if (key == VK_ESCAPE && edit == app.query) {
        SetWindowTextW(app.query, L"");
        SetFocus(app.mainEditor());
        return true;
    }
    if (edit == app.query && !ctrl && !alt && (key == VK_RETURN || key == VK_UP || key == VK_DOWN)) {
        if (app.visible.empty()) return true;
        size_t index = 0;
        if (key != VK_RETURN) {
            auto& values = app.visible;
            auto selected = std::find_if(values.begin(), values.end(), [&](size_t i) {
                return app.book.notes[i].id == app.book.active;
            });
            if (selected != values.end()) {
                index = static_cast<size_t>(selected - values.begin());
                if (key == VK_DOWN && index + 1 < values.size()) ++index;
                if (key == VK_UP && index > 0) --index;
            }
        }
        app.choose(app.book.notes[app.visible[index]].id);
        if (key != VK_RETURN) SetFocus(app.query);
        else SetFocus(app.mainEditor());
        return true;
    }
    if (edit == app.title && key == VK_RETURN && !ctrl && !alt) {
        SetFocus(app.mainEditor());
        return true;
    }

    Note* item = app.note();
    if (!item) return false;
    if (ctrl && !alt && !shift && key == 'L') {
        app.languageMenu();
        return true;
    }
    if (ctrl && !alt && shift && key == 'P' && item->kind == Kind::Note) {
        app.blocksMenu(false);
        return true;
    }
    if (!ctrl && !alt && key == VK_F10) {
        app.moreMenu(app.moreBox, item->id);
        return true;
    }
    const bool rich = edit == app.prose || edit == app.code || edit == app.reference || edit == app.attempt;
    if (rich && ctrl && !alt && (key == 'V' || (shift && key == VK_INSERT))) {
        SendMessageW(edit, WM_PASTE, 0, 0);
        return true;
    }
    if (rich && !ctrl && !alt && shift && key == VK_INSERT) {
        SendMessageW(edit, WM_PASTE, 0, 0);
        return true;
    }
    if (key == VK_TAB && !alt) {
        const bool codeBlock = edit == app.prose && fenceAt(item->body, selection(edit).cpMin).opening != View::npos;
        if (!ctrl && rich && (edit != app.prose || codeBlock)) {
            const View value = edit == app.attempt ? item->practice : item->body;
            indentSelection(edit, value, shift);
        } else {
            std::array<HWND, 6> stops{};
            int count = 0;
            stops[count++] = app.sidebarButton;
            if (app.sidebarOpen()) stops[count++] = app.query;
            stops[count++] = app.title;
            if (item->kind == Kind::Study) stops[count++] = app.reference;
            stops[count++] = app.mainEditor();
            stops[count++] = app.themeButton;
            int index = static_cast<int>(std::find(stops.begin(), stops.begin() + count, edit) - stops.begin());
            SetFocus(stops[index == count ? (shift ? count - 1 : 0) : (index + (shift ? count - 1 : 1)) % count]);
        }
        return true;
    }
    if (edit == app.prose && ctrl && !alt && (key == 'B' || key == VK_RETURN)) {
        const auto sel = selection(edit);
        const size_t start = lineStart(item->body, sel.cpMin);
        if (fenceAt(item->body, sel.cpMin).opening != View::npos) return false;
        if (key == 'B') {
            Text styled = L"**" + item->body.substr(sel.cpMin, sel.cpMax - sel.cpMin) + L"**";
            insertText(edit, styled);
            selectText(edit, sel.cpMin + 2, sel.cpMax + 2);
        } else {
            const View rest = View(item->body).substr(start);
            if (rest.starts_with(L"- [ ] ") || rest.starts_with(L"- [x] ")) {
                const bool done = rest[3] == L'x';
                selectText(edit, start + 3, start + 4);
                insertText(edit, done ? L" " : L"x");
                selectText(edit, sel.cpMin, sel.cpMax);
            }
        }
        return true;
    }
    if (rich && key == VK_RETURN && !ctrl && !alt && !shift) {
        const auto sel = selection(edit);
        const View value = edit == app.attempt ? item->practice : item->body;
        const size_t start = lineStart(value, sel.cpMin);
        const size_t end = lineEnd(value, sel.cpMin);
        const View line = value.substr(start, end - start);
        const auto fence = edit == app.prose ? fenceAt(value, sel.cpMin) : Fence{};
        if (edit != app.prose || (fence.opening != View::npos && start > fence.opening && !line.starts_with(fenceMark))) {
            size_t indent = 0;
            while (indent < line.size() && start + indent < static_cast<size_t>(sel.cpMin)
                && (line[indent] == L' ' || line[indent] == L'\t')) ++indent;
            Text next = L"\r";
            next += line.substr(0, indent);
            const View before = trimRight(line.substr(0, sel.cpMin - start));
            const int language = edit == app.prose ? fence.language : item->language;
            if (sel.cpMax == end && language && !before.empty()
                && (before.back() == L'{' || (language == 1 && before.back() == L':')))
                next += L"    ";
            insertText(edit, next);
            return true;
        }
        if (edit == app.prose && fence.opening == View::npos && sel.cpMin == sel.cpMax && sel.cpMin == end) {
            const View command = trimRight(line);
            if (command == L"/") {
                PostMessageW(app.hwnd, blockMenuMessage, 0, 0);
                return true;
            }
            if (command == L"/code" || command.starts_with(L"/code ")) {
                const int lang = command.size() > 6 ? languageIndex(command.substr(6)) : -1;
                app.insertBlock(1, true, lang);
                return true;
            }
            if (command == L"/h1" || command == L"/h2" || command == L"/list" || command == L"/todo") {
                const UINT index = command == L"/h1" ? 2 : command == L"/h2" ? 3 : command == L"/list" ? 4 : 5;
                app.insertBlock(index, true);
                return true;
            }
            constexpr View bullet = L"• ";
            const View prefix = line.starts_with(bullet) ? bullet
                : (line.starts_with(L"- [ ] ") || line.starts_with(L"- [x] ")) ? View(L"- [ ] ") : View{};
            if (!prefix.empty()) {
                if (trimRight(line).size() < prefix.size()) {
                    selectText(edit, start, end);
                    insertText(edit, L"");
                } else {
                    Text next = L"\r";
                    next += prefix;
                    insertText(edit, next);
                }
                return true;
            }
        }
    }
    return false;
}

void pastePlain(HWND edit) {
    if (!OpenClipboard(edit)) return;
    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    Text text;
    if (handle) {
        const auto* source = static_cast<const wchar_t*>(GlobalLock(handle));
        if (source) {
            const size_t capacity = GlobalSize(handle) / sizeof(wchar_t);
            const size_t count = wcsnlen(source, capacity);
            text.reserve(std::min<size_t>(count, maxEditSize));
            for (size_t i = 0; i < count && text.size() < maxEditSize; ++i) {
                if (source[i] == L'\r' && i + 1 < count && source[i + 1] == L'\n') ++i;
                text += source[i] == L'\n' ? L'\r' : source[i];
            }
            GlobalUnlock(handle);
        }
    }
    CloseClipboard();
    if (!text.empty()) insertText(edit, text);
}

LRESULT CALLBACK editProc(HWND edit, UINT message, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR data) {
    auto& app = *reinterpret_cast<App*>(data);
    if (message == WM_MOUSEWHEEL && handleZoomWheel(app, w)) return 0;
    if (id >= 3 && (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)) {
        HWND target = app.editorAt({GET_X_LPARAM(l), GET_Y_LPARAM(l)});
        app.scrollEditor(target ? target : edit, message, w);
        return 0;
    }
    if (message == WM_PASTE && id >= 3) {
        pastePlain(edit);
        return 0;
    }
    if (message == WM_CONTEXTMENU) {
        app.editorMenu(edit, {GET_X_LPARAM(l), GET_Y_LPARAM(l)});
        return 0;
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(edit, editProc, id);
    const bool keyboardInput = message >= WM_KEYFIRST && message <= WM_KEYLAST;
    const bool compositionInput = message == WM_IME_CHAR || message == WM_IME_COMPOSITION;
    const HCURSOR pointer = keyboardInput || compositionInput ? GetCursor() : nullptr;
    const LRESULT result = DefSubclassProc(edit, message, w, l);
    // Windows edit controls can clear the mouse pointer while processing keys, including Backspace.
    if (pointer && !GetCursor()) SetCursor(pointer);
    if (id >= 3 && message == WM_PAINT && !app.changing && GetWindowTextLengthW(edit) == 0) {
        const wchar_t* hint = id == 3 ? L"Yazmaya başla veya / yazıp Enter’a bas…"
            : id == 4 ? L"Kodunu yaz…" : id == 5 ? L"Referans kodu yapıştır…" : L"Buraya kendin yaz…";
        HDC dc = GetDC(edit);
        RECT rect{};
        GetClientRect(edit, &rect);
        Canvas canvas{dc, app.dpi};
        canvas.text(hint, {2, 0, app.logical(rect.right) - 7, app.zoomUnits(23)}, id == 3 ? app.bodyFont : app.mono, app.colors().quiet);
        ReleaseDC(edit, dc);
    }
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS) {
        if (id == 1) app.invalidate(app.searchBox);

        InvalidateRect(edit, nullptr, FALSE);
    }
    if (id >= 4 && !app.changing && (message == WM_VSCROLL || message == WM_KEYDOWN || message == WM_CHAR || message == WM_PASTE || message == WM_LBUTTONUP)) {
        app.invalidateGutters();
    }
    return result;
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        app->hwnd = hwnd;
        app->dpi = GetDpiForWindow(hwnd);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(hwnd, message, w, l);

    switch (message) {
    case WM_CREATE: {
        if (!app->createControls()) return -1;
        app->loadActive();
        if (app->dirty) app->markDirty();
        return 0;
    }
    case WM_SIZE:
        if (w != SIZE_MINIMIZED && app->query) app->layout();
        return 0;
    case WM_DPICHANGED: {
        app->dpi = LOWORD(w);
        app->fonts();
        const auto& r = *reinterpret_cast<RECT*>(l);
        SetWindowPos(hwnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(l);
        limits->ptMinTrackSize = {app->px(775), app->px(485)};
        return 0;
    }
    case WM_PAINT:
        app->paint();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MEASUREITEM: {
        auto& item = *reinterpret_cast<MEASUREITEMSTRUCT*>(l);
        if (item.CtlType == ODT_MENU && app->measureMenu(item)) return TRUE;
        break;
    }
    case WM_DRAWITEM: {
        const auto& item = *reinterpret_cast<const DRAWITEMSTRUCT*>(l);
        if (item.CtlType == ODT_MENU) return app->drawMenu(item);
        if (item.CtlType != ODT_BUTTON) break;
        app->drawButton(item);
        return TRUE;
    }
    case WM_CTLCOLORBTN:
        return reinterpret_cast<LRESULT>(app->paperBrush);
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(w);
        const bool search = reinterpret_cast<HWND>(l) == app->query;
        const auto& c = app->colors();
        SetTextColor(dc, c.ink);
        SetBkColor(dc, search ? c.side : c.paper);
        return reinterpret_cast<LRESULT>(search ? app->sidebarBrush : app->paperBrush);
    }
    case WM_NOTIFY: {
        const auto& header = *reinterpret_cast<const NMHDR*>(l);
        if (header.code == EN_REQUESTRESIZE)
            app->rememberExtent(*reinterpret_cast<const REQRESIZE*>(l));
        return 0;
    }
    case WM_COMMAND:
        if (HIWORD(w) == BN_CLICKED && l) {
            if (LOWORD(w) == 7) app->toggleSidebar();
            if (LOWORD(w) == 8) app->toggleTheme();
        }
        if (HIWORD(w) == EN_CHANGE && l) app->changed(LOWORD(w));
        if (HIWORD(w) == EN_VSCROLL) app->invalidateGutters();
        return 0;
    case WM_TIMER:
        if (w == saveTimer) app->save();
        else if (w == styleTimer) app->style();
        return 0;
    case blockMenuMessage:
        app->blocksMenu(true);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(l) == HTCLIENT) {
            POINT p{};
            GetCursorPos(&p);
            ScreenToClient(hwnd, &p);
            const auto target = app->hit(app->logical(p.x), app->logical(p.y)).action;
            if (target != Action::None || app->dragging) {
                SetCursor(LoadCursorW(nullptr, target == Action::Split || app->dragging ? IDC_SIZEWE : IDC_HAND));
                return TRUE;
            }
        }
        break;
    case WM_MOUSEMOVE: {
        const int x = app->logical(GET_X_LPARAM(l)), y = app->logical(GET_Y_LPARAM(l));
        if (app->dragging) {
            const int ratio = std::clamp((x - app->codeBox.x - splitWidth / 2) * 100 / std::max(1, app->codeBox.w - splitWidth), 30, 70);
            if (ratio != app->splitRatio) { app->splitRatio = ratio; app->layout(); }
        } else {
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&track);
            app->hover(app->hit(x, y));
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        app->hover({});
        return 0;
    case WM_LBUTTONDOWN:
        app->click(app->hit(app->logical(GET_X_LPARAM(l)), app->logical(GET_Y_LPARAM(l))));
        return 0;
    case WM_LBUTTONUP:
        if (app->dragging) ReleaseCapture();
        return 0;
    case WM_CAPTURECHANGED:
        app->dragging = false;
        app->invalidate(app->splitBox);
        return 0;
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL: {
        if (message == WM_MOUSEWHEEL && handleZoomWheel(*app, w)) return 0;
        POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        if (HWND target = app->editorAt(p)) {
            app->scrollEditor(target, message, w);
        } else if (message == WM_MOUSEWHEEL) {
            ScreenToClient(hwnd, &p);
            if (app->logical(p.x) < app->workspaceLeft()) {
                app->scroll = std::clamp(app->scroll - GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA * 3, 0, app->maxScroll());
                app->hover({});
                app->invalidate({0, listTop, sidebarWidth, app->h - listTop});
            }
        }
        return 0;
    }
    case WM_CONTEXTMENU: {
        POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        ScreenToClient(hwnd, &p);
        const int x = app->logical(p.x), y = app->logical(p.y);
        const auto target = app->hit(x, y);
        if (target.action == Action::Row) app->moreMenu({x, y - 4, 0, 0}, target.item);
        return 0;
    }
    case WM_QUERYENDSESSION:
        app->save();
        return !app->dirty;
    case WM_CLOSE:
        app->save();
        if (app->dirty && MessageBoxW(hwnd, L"Son değişiklikler kaydedilemedi. Yine de kapatılsın mı?",
            appName, MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) return 0;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        app->releaseFonts();
        DeleteObject(app->sidebarBrush);
        DeleteObject(app->paperBrush);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, w, l);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR args, int show) {
    const bool test = View(args) == L"--test-profile";
    HANDLE single = CreateMutexW(nullptr, FALSE, test ? L"Local\\Defter.Notebook.Test.1" : L"Local\\Defter.Notebook.1");
    if (!single) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(appClass, test ? testName : appName)) {
            if (IsIconic(existing)) ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        CloseHandle(single);
        return 0;
    }
    const HRESULT ole = OleInitialize(nullptr);
    HMODULE rich = LoadLibraryExW(L"msftedit.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!rich) {
        MessageBoxW(nullptr, L"Windows metin düzenleyicisi başlatılamadı.", appName, MB_OK | MB_ICONERROR);
        return 1;
    }
    InitCommonControls();
    App app;
    app.instance = instance;
    app.initStorage(test);
    app.preferences.load(app.book.path);
    WNDCLASSEXW type{sizeof(type)};
    type.lpfnWndProc = windowProc;
    type.hInstance = instance;
    type.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    type.lpszClassName = appClass;
    if (!RegisterClassExW(&type)) return 1;
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const UINT dpi = GetDpiForSystem();
    const int width = std::min(MulDiv(1110, dpi, 96), static_cast<int>(work.right - work.left));
    const int height = std::min(MulDiv(750, dpi, 96), static_cast<int>(work.bottom - work.top));
    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, appClass, test ? testName : appName, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        work.left + (work.right - work.left - width) / 2, work.top + (work.bottom - work.top - height) / 2,
        width, height, nullptr, nullptr, instance, &app);
    if (!hwnd) return 1;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if ((msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) && shortcut(app, msg.hwnd, msg.wParam)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    FreeLibrary(rich);
    if (SUCCEEDED(ole)) OleUninitialize();
    CloseHandle(single);
    return static_cast<int>(msg.wParam);
}
