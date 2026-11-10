
    Box rowBox(int index) const { return {listBox.x + 4, listTop + (index - scroll) * rowHeight, listBox.w - 9, rowHeight - 3}; }

    Hit hit(int x, int y) {
        if (addBox.contains(x, y)) return {Action::Add};
        if (sidebarOpen() && !search.empty() && clearBox.contains(x, y)) return {Action::Clear};
        if (auto* current = note()) {
            if (zoomOutBox.contains(x, y)) return {Action::ZoomOut};
            if (zoomResetBox.contains(x, y)) return {Action::ZoomReset};
            if (zoomInBox.contains(x, y)) return {Action::ZoomIn};
            if (menuBox.contains(x, y)) return {current->kind == Kind::Note ? Action::Blocks : Action::Language};
            if (moreBox.contains(x, y)) return {Action::More};
            if (current->kind == Kind::Study && splitBox.contains(x, y)) return {Action::Split};
        }
        if (sidebarOpen() && listBox.contains(x, y)) {
            const int index = scroll + (y - listTop) / rowHeight;
            if (index < static_cast<int>(visible.size()) && rowBox(index).contains(x, y))
                return {Action::Row, book.notes[visible[index]].id};
        }
        return {};
    }

    void repaintHit(Hit target) const {
        switch (target.action) {
        case Action::Add: invalidate(addBox); break;
        case Action::Clear: invalidate(clearBox); break;
        case Action::Blocks:
        case Action::Language: invalidate(menuBox); break;
        case Action::More: invalidate(moreBox); break;
        case Action::Split: invalidate(splitBox); break;
        case Action::Row: invalidate(listBox); break;
        case Action::ZoomOut: invalidate(zoomOutBox); break;
        case Action::ZoomReset: invalidate(zoomResetBox); break;
        case Action::ZoomIn: invalidate(zoomInBox); break;
        default: break;
        }
    }

    void hover(Hit next) {
        if (next == hovered) return;
        repaintHit(hovered);
        hovered = next;
        repaintHit(hovered);
    }

    bool hot(Action action) const { return hovered.action == action; }

    void drawButton(const DRAWITEMSTRUCT& item) const {
        const Canvas c{item.hDC, dpi};
        const auto& p = colors();
        const int width = logical(item.rcItem.right), height = logical(item.rcItem.bottom);
        const bool highlighted = (item.itemState & (ODS_SELECTED | ODS_HOTLIGHT)) != 0;
        const bool focused = (item.itemState & ODS_FOCUS) != 0;
        c.fill({0, 0, width, height}, p.paper);
        c.round({0, 0, width, height}, 6, highlighted ? p.hover : p.paper, focused ? p.thumb : p.line);
        if (item.CtlID == 7) {
            c.glyph(Glyph::Sidebar, (width - 18) / 2, (height - 18) / 2, p.ink);
        } else {
            c.glyph(preferences.dark ? Glyph::Sun : Glyph::Moon, 9, (height - 18) / 2, p.ink);
            c.text(preferences.dark ? L"Açık mod" : L"Koyu mod", {33, 0, width - 38, height}, regular, p.ink);
        }
    }

    void drawSidebar(const Canvas& c) {
        const auto& p = colors();
        c.fill({0, 0, sidebarWidth, h}, p.side);
        c.fill({sidebarWidth - 1, 0, 1, h}, p.line);
        c.round(searchBox, 6, p.side, GetFocus() == query ? p.thumb : p.border);
        c.glyph(Glyph::Search, 25, 21, p.muted);
        if (search.empty()) {
            c.text(L"Ctrl K", {sidebarWidth - 62, 13, 38, 34}, captionFont, p.quiet, DT_CENTER);
        } else {
            if (hot(Action::Clear)) c.round(clearBox, 4, p.hover);
            c.glyph(Glyph::Close, clearBox.x + 2, clearBox.y + 4, p.quiet);
        }

        c.round(addBox, 6, hot(Action::Add) ? p.hover : p.paper, p.border);
        c.glyph(Glyph::Plus, addBox.x + 9, addBox.y + 9, p.ink);
        c.text(L"Yeni not", {addBox.x + 34, addBox.y, 110, addBox.h}, medium, p.ink);
        c.glyph(Glyph::Chevron, sidebarWidth - 45, addBox.y + 9, p.muted);

        c.text(L"NOTLAR", {20, 111, 136, 24}, section, p.quiet);
        c.text(std::to_wstring(visible.size()), {167, 111, 49, 24}, captionFont, p.quiet, DT_RIGHT);
        const RECT clip = c.rect(listBox);
        const int state = SaveDC(c.dc);
        IntersectClipRect(c.dc, clip.left, clip.top, clip.right, clip.bottom);
        if (visible.empty()) c.text(search.empty() ? L"Henüz not yok" : L"Sonuç bulunamadı",
            {22, listTop, sidebarWidth - 42, 34}, regular, p.quiet);
        const int count = std::min(static_cast<int>(visible.size()), scroll + listBox.h / rowHeight + 1);
        for (int index = scroll; index < count; ++index) {
            const auto& item = book.notes[visible[index]];
            const Box row = rowBox(index);
            const bool active = item.id == book.active;
            if (active || hovered == Hit{Action::Row, item.id}) c.round(row, 5, active ? p.selected : p.hover);
            c.glyph(kindGlyph(item.kind), row.x + 9, row.y + 7, p.muted);
            c.text(label(item), {row.x + 34, row.y, row.w - 45, row.h}, active ? medium : regular, p.ink);
        }
        RestoreDC(c.dc, state);
        if (maxScroll() > 0) {
            const int track = std::max(16, listBox.h - 6);
            const int thumb = std::max(22, track * std::max(1, listBox.h / rowHeight) / static_cast<int>(visible.size()));
            c.round({sidebarWidth - 9, listTop + 3 + (track - thumb) * scroll / maxScroll(), 3, thumb}, 2, p.thumb);
        }
    }

    void drawPanel(const Canvas& c, Box panel, HWND edit, View label = {}) {
        const auto& p = colors();
        const int header = label.empty() ? 0 : studyHeader;
        c.fill(panel, p.codePaper);
        if (header) {
            c.text(label, {panel.x + 12, panel.y, panel.w - 24, header}, captionFont, p.muted);
            c.fill({panel.x, panel.y + header - 1, panel.w, 1}, p.line);
        }

        const Box numbers = gutter(panel, header);
        const RECT clip = c.rect(numbers);
        if (!RectVisible(c.dc, &clip)) return;
        c.fill({numbers.x + numbers.w - 1, numbers.y, 1, numbers.h}, p.line);
        const int state = SaveDC(c.dc);
        IntersectClipRect(c.dc, clip.left, clip.top, clip.right, clip.bottom);
        int i = static_cast<int>(SendMessageW(edit, EM_GETFIRSTVISIBLELINE, 0, 0));
        const int total = static_cast<int>(SendMessageW(edit, EM_GETLINECOUNT, 0, 0));
        for (; i < total; ++i) {
            const auto cp = SendMessageW(edit, EM_LINEINDEX, i, 0);
            if (cp < 0) break;
            POINT point{};
            SendMessageW(edit, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&point), cp);
            const int y = logical(point.y);
            if (y >= numbers.h) break;
            c.text(std::to_wstring(i + 1), {numbers.x + 3, numbers.y + y, numbers.w - 9, zoomUnits(18)},
                mono, p.quiet, DT_RIGHT);
        }
        RestoreDC(c.dc, state);
    }

    void drawMain(const Canvas& c) {
        const auto& p = colors();
        const int left = workspaceLeft(), width = w - left;
        c.fill({left, 0, width, h}, p.paper);
        c.fill({left, topHeight - 1, width, 1}, p.line);
        if (!sidebarOpen()) {
            c.round(addBox, 6, hot(Action::Add) ? p.hover : p.paper, p.line);
            c.glyph(Glyph::Plus, addBox.x + 8, addBox.y + 7, p.ink);
        }
        const Note* current = note();
        if (!current) {
            c.text(L"Henüz not yok", {left, h / 2 - 29, width, 36}, logo, p.ink, DT_CENTER);
            c.text(L"Yeni not menüsünden başlayabilirsin.", {left, h / 2 + 11, width, 24}, regular, p.muted, DT_CENTER);
            return;
        }

        const bool normal = current->kind == Kind::Note;
        c.round(menuBox, 6, hot(normal ? Action::Blocks : Action::Language) ? p.hover : p.paper, p.line);
        c.glyph(normal ? Glyph::Blocks : Glyph::Code, menuBox.x + 9, menuBox.y + 7, p.muted);
        c.text(normal ? L"Blok ekle" : languages[current->language].name,
            {menuBox.x + 33, menuBox.y, 78, menuBox.h}, regular, p.ink);
        c.glyph(Glyph::Chevron, menuBox.x + 111, menuBox.y + 7, p.muted);
        if (hot(Action::More)) c.round(moreBox, 6, p.hover);
        c.glyph(Glyph::More, moreBox.x + 8, moreBox.y + 7, p.muted);

        if (current->kind == Kind::Study) {
            drawPanel(c, leftBox, reference, L"Referans kod");
            drawPanel(c, rightBox, attempt, L"Senin kodun");
            c.fill(splitBox, p.codePaper);
            c.fill({splitBox.x + splitWidth / 2, splitBox.y, 1, splitBox.h}, p.border);
            if (hot(Action::Split) || dragging)
                c.round({splitBox.x + splitWidth / 2 - 1, splitBox.y + splitBox.h / 2 - 18, 3, 36}, 2, p.thumb);
        } else if (current->kind == Kind::Code) {
            drawPanel(c, codeBox, code);
        }
        drawFooter(c, current->kind);
    }

    void drawFooter(const Canvas& c, Kind kind) {
        const auto& p = colors();
        const int y = h - footerHeight, left = workspaceLeft();
        c.fill({left, y, w - left, footerHeight}, p.paper);
        c.fill({left, y, w - left, 1}, p.line);
        c.text(savingFailed ? L"Kayıt başarısız" : dirty ? L"Kaydediliyor…" : L"Kaydedildi",
            {left + 10, y, 121, footerHeight}, captionFont, savingFailed ? p.ink : p.muted);
        const int hintRight = zoomOutBox.x - 13;
        const Box hint{left + 136, y, hintRight - left - 136, footerHeight};
        if (kind == Kind::Study && match[1]) {
            const Text result = match[2] ? L"Kod eşleşti"
                : std::to_wstring(match[0]) + L" / " + std::to_wstring(match[1]) + L" satır eşleşti";
            c.text(result, hint, captionFont, p.ink, DT_RIGHT);
        } else if (kind == Kind::Note) {
            c.text(L"/code + Enter", hint, captionFont, p.muted, DT_RIGHT);
        }

        const auto zoomButton = [&](Box b, Action action, View text) {
            if (hot(action)) c.fill(b, p.hover);
            c.text(text, b, captionFont, p.ink, DT_CENTER);
        };
        c.fill({zoomOutBox.x - 4, y + 5, 1, footerHeight - 9}, p.line);
        zoomButton(zoomOutBox, Action::ZoomOut, L"−");
        zoomButton(zoomResetBox, Action::ZoomReset, std::to_wstring(preferences.zoom) + L"%");
        zoomButton(zoomInBox, Action::ZoomIn, L"+");
    }

    void paint() {
        PAINTSTRUCT ps{};
        HDC target = BeginPaint(hwnd, &ps);
        const int width = ps.rcPaint.right - ps.rcPaint.left, height = ps.rcPaint.bottom - ps.rcPaint.top;
        if (width > 0 && height > 0) {
            HDC mem = CreateCompatibleDC(target);
            HBITMAP bitmap = mem ? CreateCompatibleBitmap(target, width, height) : nullptr;
            HGDIOBJ old = bitmap ? SelectObject(mem, bitmap) : nullptr;
            if (bitmap) SetViewportOrgEx(mem, -ps.rcPaint.left, -ps.rcPaint.top, nullptr);
            Canvas c{bitmap ? mem : target, dpi};
            FillRect(c.dc, &ps.rcPaint, paperBrush);
            const int divider = px(workspaceLeft());
            if (ps.rcPaint.left < divider) drawSidebar(c);
            if (ps.rcPaint.right > divider) drawMain(c);
            if (bitmap) {
                BitBlt(target, ps.rcPaint.left, ps.rcPaint.top, width, height, mem, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
                SelectObject(mem, old);
                DeleteObject(bitmap);
            }
            if (mem) DeleteDC(mem);
        }
        EndPaint(hwnd, &ps);
    }
