#include "appearance_ui.inl"

#include "menu_theme.inl"

    void addMenu() {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, 1, L"Metin notu\tCtrl+N");
        AppendMenuW(menu, MF_STRING, 2, L"Kod notu\tCtrl+Shift+N");
        AppendMenuW(menu, MF_STRING, 3, L"Kod çalışması\tCtrl+Alt+N");
        const UINT choice = popup(menu, addBox);
        if (choice) add(static_cast<Kind>(choice - 1));
    }
    void languageMenu() {
        Note* current = note();
        if (!current) return;
        const auto caret = selection(prose);
        Fence block;
        if (current->kind == Kind::Note) block = fenceAt(current->body, caret.cpMin);
        const int active = block.opening == View::npos ? current->language : block.language;
        HMENU menu = CreatePopupMenu();
        auto append = [&](UINT index, UINT flags) {
            AppendMenuW(menu, MF_STRING | flags | (index == active ? MF_CHECKED : 0),
                index + 1, languages[index].name);
        };
        for (UINT i = 0; i < std::size(languages); ++i) {
            if (i == jqueryLanguage) continue;
            append(i, i == 10 ? MF_MENUBARBREAK : 0);
            if (i == javascriptLanguage) append(jqueryLanguage, 0);
        }
        const UINT result = popup(menu, menuBox);
        if (!result) return;
        const UINT lang = result - 1;
        current->language = lang;
        if (current->kind == Kind::Note && block.opening != View::npos) {
            Text tag(fenceMark);
            tag += languages[lang].tag;
            const LONG shift = static_cast<LONG>(tag.size()) - static_cast<LONG>(block.end - block.opening);
            selectText(prose, block.opening, block.end);
            insertText(prose, tag);
            const size_t restored = caret.cpMin > static_cast<LONG>(block.end)
                ? caret.cpMin + shift : block.opening + tag.size();
            selectText(prose, restored, restored);
            SetFocus(prose);
        } else if (current->kind != Kind::Note) {
            scheduleStyle(current->kind == Kind::Code ? 2 : 12);
        }
        invalidate({workspaceLeft(), 0, w - workspaceLeft(), topHeight});
        markDirty();
    }

    void insertBlock(UINT choice, bool typed = false, int pickedLanguage = -1) {
        auto* current = note();
        if (!current || current->kind != Kind::Note) return;
        const auto caret = selection(prose);
        const auto& value = current->body;
        const size_t start = lineStart(value, caret.cpMin);
        const size_t end = lineEnd(value, caret.cpMin);
        const auto block = fenceAt(value, caret.cpMin);
        if (choice == 1 && block.opening != View::npos) return;
        const bool selected = caret.cpMin != caret.cpMax;
        const bool emptyLine = trimRight(View(value).substr(start, end - start)).empty();
        size_t left = start, right = typed ? end : start;
        Text out;
        size_t cursor = left;
        if (choice == 1) {
            if (!typed && selected) {
                left = caret.cpMin;
                right = caret.cpMax;
            } else if (!typed && !emptyLine) {
                left = right = end;
            } else right = end;
            if (left > start) out += L'\r';
            out += fenceMark;
            out += languages[pickedLanguage >= 0 ? pickedLanguage : current->language].tag;
            out += L'\r';
            cursor = left + out.size();
            if (!typed && selected) {
                out.append(value, left, right - left);
                if (out.back() != L'\r') out += L'\r';
            } else out += L'\r';
            out += fenceMark;
            if (right >= value.size() || value[right] != L'\r') out += L'\r';
        } else {
            constexpr const wchar_t* prefixes[] = {L"# ", L"## ", L"• ", L"- [ ] "};
            out = prefixes[choice - 2];
            cursor = left + out.size();
        }
        SetFocus(prose);
        selectText(prose, left, right);
        insertText(prose, out);
        selectText(prose, cursor, cursor);
        SendMessageW(prose, EM_SCROLLCARET, 0, 0);
    }

    void blocksMenu(bool inlineMenu) {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, 1, L"Kod bloğu\t/code");
        AppendMenuW(menu, MF_STRING, 2, L"Büyük başlık\t/h1");
        AppendMenuW(menu, MF_STRING, 3, L"Başlık\t/h2");
        AppendMenuW(menu, MF_STRING, 4, L"Madde işareti\t/list");
        AppendMenuW(menu, MF_STRING, 5, L"Yapılacak\t/todo");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 6, L"Kod dilini seç…");
        Box anchor = menuBox;
        if (inlineMenu) {
            POINT p{};
            SendMessageW(prose, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&p), selection(prose).cpMin);
            MapWindowPoints(prose, hwnd, &p, 1);
            anchor = {logical(p.x), logical(p.y), 0, 21};
        }
        const UINT choice = popup(menu, anchor);
        if (choice == 6) languageMenu();
        else if (choice) insertBlock(choice, inlineMenu);
    }

    static Text newlines(View value) {
        Text result(value);
        std::replace(result.begin(), result.end(), L'\r', L'\n');
        return result;
    }

    void exportNote(const Note& current) {
        Text name = label(current);
        for (auto& ch : name) if (ch < 32 || View(L"<>:\"/\\|?*").find(ch) != View::npos) ch = L'_';
        while (!name.empty() && (name.back() == L' ' || name.back() == L'.')) name.pop_back();
        if (name.empty()) name = L"Not";
        wchar_t path[32768]{};
        const wchar_t* ext = current.kind == Kind::Code ? languages[current.language].extension : L"md";
        name += L".";
        name += ext;
        lstrcpynW(path, name.c_str(), static_cast<int>(std::size(path)));
        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = hwnd;
        dialog.lpstrFilter = current.kind == Kind::Code ? L"Kod dosyası\0*.*\0\0" : L"Markdown\0*.md\0Tüm dosyalar\0*.*\0\0";
        dialog.lpstrFile = path;
        dialog.nMaxFile = static_cast<DWORD>(std::size(path));
        dialog.lpstrDefExt = ext;
        dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        dialog.lpstrTitle = L"Notu dışa aktar";
        if (!GetSaveFileNameW(&dialog)) return;
        Text content;
        if (current.kind == Kind::Code) content = newlines(current.body);
        else {
            content = L"# " + label(current) + L"\n\n";
            if (current.kind == Kind::Note) {
                View body = current.body;
                bool inCode = false;
                for (size_t start = 0; start < body.size();) {
                    const size_t end = lineEnd(body, start);
                    View line = body.substr(start, end - start);
                    if (line.starts_with(fenceMark)) inCode = !inCode;
                    if (!inCode && line.starts_with(L"\u2022 ")) {
                        content += L"- ";
                        line.remove_prefix(2);
                    }
                    content += line;
                    if (end < body.size()) content += L'\n';
                    start = end + 1;
                }
            }
            else {
                auto pane = [&](View label, View body) {
                    size_t longest = 2, run = 0;
                    for (wchar_t ch : body) {
                        run = ch == 0x60 ? run + 1 : 0;
                        longest = std::max(longest, run);
                    }
                    const Text delimiter(longest + 1, 0x60);
                    content += label;
                    content += delimiter + languages[current.language].tag + L"\n" + newlines(body) + L"\n" + delimiter + L"\n\n";
                };
                pane(L"## Referans kod\n\n", current.body);
                pane(L"## Senin kodun\n\n", current.practice);
            }
        }
        if (!writeFile(path, utf8(content))) MessageBoxW(hwnd, L"Dosya yazılamadı.", appName, MB_OK | MB_ICONERROR);
    }
    void moreMenu(Box anchor, uint32_t id) {
        auto pos = std::find_if(book.notes.begin(), book.notes.end(), [id](const Note& item) { return item.id == id; });
        if (pos == book.notes.end()) return;
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, 1, L"Yeniden adlandır\tF2");
        AppendMenuW(menu, MF_STRING, 2, L"Çoğalt");
        AppendMenuW(menu, MF_STRING, 3, L"Dışa aktar…");
        AppendMenuW(menu, MF_STRING, 5, fullScreen ? L"Tam ekrandan çık\tF11" : L"Tam ekran\tF11");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 4, L"Notu sil…");
        const UINT action = popup(menu, anchor);
        if (action == 1) {
            choose(id);
            SetFocus(title);
            SendMessageW(title, EM_SETSEL, 0, -1);
        } else if (action == 2) {
            add(pos->kind, &*pos);
        } else if (action == 3) {
            exportNote(*pos);
        } else if (action == 5) {
            toggleFullScreen();
        } else if (action == 4) {
            const Text question = L"“" + label(*pos) + L"” silinsin mi?";
            if (MessageBoxW(hwnd, question.c_str(), L"Notu sil", MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) return;
            const bool wasActive = book.active == id;
            const auto next = book.notes.erase(pos);
            if (wasActive) {
                book.active = next != book.notes.end() ? next->id : book.notes.empty() ? 0 : book.notes.back().id;
                loadActive();
                SetFocus(mainEditor());
            } else filter();
            markDirty();
        }
    }
    void editorMenu(HWND edit, POINT p) {
        if (p.x == -1 && p.y == -1) {
            p = {px(8), px(16)};
            ClientToScreen(edit, &p);
        }
        const bool rich = edit != title && edit != query;
        const auto range = rich ? selection(edit) : CHARRANGE{};
        DWORD start = range.cpMin, end = range.cpMax;
        if (!rich) SendMessageW(edit, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
        const UINT enabled = start != end ? 0 : MF_GRAYED;
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING | (SendMessageW(edit, EM_CANUNDO, 0, 0) ? 0 : MF_GRAYED), 1, L"Geri al\tCtrl+Z");
        if (rich) AppendMenuW(menu, MF_STRING | (SendMessageW(edit, EM_CANREDO, 0, 0) ? 0 : MF_GRAYED), 2, L"Yinele\tCtrl+Y");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | enabled, 3, L"Kes\tCtrl+X");
        AppendMenuW(menu, MF_STRING | enabled, 4, L"Kopyala\tCtrl+C");
        AppendMenuW(menu, MF_STRING | (IsClipboardFormatAvailable(CF_UNICODETEXT) ? 0 : MF_GRAYED), 5, L"Yapıştır\tCtrl+V");
        AppendMenuW(menu, MF_STRING, 6, L"Tümünü seç\tCtrl+A");
        ScreenToClient(hwnd, &p);
        const UINT choice = popup(menu, {logical(p.x), logical(p.y) - 4, 0, 0});
        constexpr UINT messages[] = {0, WM_UNDO, EM_REDO, WM_CUT, WM_COPY, WM_PASTE, EM_SETSEL};
        if (choice) SendMessageW(edit, messages[choice], 0, choice == 6 ? -1 : 0);
    }

    void click(Hit target) {
        switch (target.action) {
        case Action::Add: addMenu(); break;
        case Action::Blocks: blocksMenu(false); break;
        case Action::Language: languageMenu(); break;
        case Action::More: moreMenu(moreBox, book.active); break;
        case Action::Row: choose(target.item); break;
        case Action::Clear: SetWindowTextW(query, L""); SetFocus(query); break;
        case Action::Split: dragging = true; SetCapture(hwnd); break;
        case Action::ZoomOut: zoomBy(-1); break;
        case Action::ZoomReset: setZoom(defaultZoom); break;
        case Action::ZoomIn: zoomBy(1); break;
        default: break;
        }
    }
