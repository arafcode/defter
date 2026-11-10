
    struct MenuRow {
        Text label, shortcut;
    };

    bool measureMenu(MEASUREITEMSTRUCT& item) const {
        const auto* row = reinterpret_cast<const MenuRow*>(item.itemData);
        if (!row) return false;
        HDC dc = GetDC(hwnd);
        const auto previous = SelectObject(dc, regular);
        SIZE label{}, shortcut{};
        GetTextExtentPoint32W(dc, row->label.data(), static_cast<int>(row->label.size()), &label);
        GetTextExtentPoint32W(dc, row->shortcut.data(), static_cast<int>(row->shortcut.size()), &shortcut);
        SelectObject(dc, previous);
        ReleaseDC(hwnd, dc);
        item.itemHeight = px(28);
        item.itemWidth = px(row->shortcut.empty() ? 44 : 72) + label.cx + shortcut.cx;
        return true;
    }

    bool drawMenu(const DRAWITEMSTRUCT& item) const {
        const auto* row = reinterpret_cast<const MenuRow*>(item.itemData);
        if (!row) return false;
        const auto& p = colors();
        const RECT r = item.rcItem;
        const Box b{logical(r.left), logical(r.top), logical(r.right) - logical(r.left), logical(r.bottom) - logical(r.top)};
        const Canvas c{item.hDC, dpi};
        const bool disabled = (item.itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;
        const bool selected = (item.itemState & ODS_SELECTED) != 0;
        const auto old = SaveDC(item.hDC);
        IntersectClipRect(item.hDC, r.left, r.top, r.right, r.bottom);
        SetDCBrushColor(item.hDC, selected && !disabled ? p.selected : p.side);
        FillRect(item.hDC, &r, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        if (item.itemState & ODS_CHECKED)
            c.text(L"✓", {b.x + 9, b.y, 17, b.h}, regular, p.ink);
        c.text(row->label, {b.x + 29, b.y, b.w - 37, b.h}, regular, disabled ? p.quiet : p.ink);
        c.text(row->shortcut, {b.x + 29, b.y, b.w - 39, b.h}, captionFont, p.quiet, DT_RIGHT);
        RestoreDC(item.hDC, old);
        return true;
    }

    UINT popup(HMENU menu, Box box) const {
        MENUINFO appearance{sizeof(appearance)};
        appearance.fMask = MIM_BACKGROUND;
        appearance.hbrBack = sidebarBrush;
        SetMenuInfo(menu, &appearance);
        const int count = GetMenuItemCount(menu);
        std::vector<MenuRow> rows(std::max(0, count));
        for (int index = 0; index < count; ++index) {
            MENUITEMINFOW item{sizeof(item)};
            wchar_t label[160]{};
            item.fMask = MIIM_FTYPE | MIIM_STRING;
            item.dwTypeData = label;
            item.cch = static_cast<UINT>(std::size(label) - 1);
            if (!GetMenuItemInfoW(menu, index, TRUE, &item) || (item.fType & MFT_SEPARATOR)) continue;
            View value(label);
            const auto tab = value.find(L'\t');
            auto& row = rows[index];
            row.label = value.substr(0, tab);
            if (tab != View::npos) row.shortcut = value.substr(tab + 1);
            item.fMask = MIIM_FTYPE | MIIM_DATA;
            item.fType |= MFT_OWNERDRAW;
            item.dwItemData = reinterpret_cast<ULONG_PTR>(&row);
            SetMenuItemInfoW(menu, index, TRUE, &item);
        }

        POINT point{px(box.x), px(box.y + box.h + 4)};
        ClientToScreen(hwnd, &point);
        const UINT result = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, hwnd, nullptr);
        DestroyMenu(menu);
        return result;
    }
