
#pragma once

constexpr wchar_t uiFace[] = L"Segoe UI";
constexpr wchar_t codeFace[] = L"Consolas";
constexpr int proseSize = 255, codeSize = 225;

Text editText(HWND edit) {
    GETTEXTLENGTHEX len{GTL_PRECISE | GTL_NUMCHARS, 1200};
    const LRESULT count = SendMessageW(edit, EM_GETTEXTLENGTHEX, reinterpret_cast<WPARAM>(&len), 0);
    if (count <= 0) return {};
    Text value(static_cast<size_t>(count) + 1, L'\0');
    GETTEXTEX info{static_cast<DWORD>(value.size() * sizeof(wchar_t)), GT_DEFAULT, 1200, nullptr, nullptr};
    LRESULT got = SendMessageW(edit, EM_GETTEXTEX, reinterpret_cast<WPARAM>(&info), reinterpret_cast<LPARAM>(value.data()));
    value.resize(static_cast<size_t>(std::max<LRESULT>(0, got)));
    return value;
}

CHARRANGE selection(HWND edit) {
    CHARRANGE range{};
    SendMessageW(edit, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&range));
    return range;
}

void selectText(HWND edit, size_t start, size_t end) {
    CHARRANGE range{static_cast<LONG>(start), static_cast<LONG>(end)};
    SendMessageW(edit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
}

void insertText(HWND edit, const Text& text) {
    SendMessageW(edit, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(text.c_str()));
}

struct Style {
    COLORREF foreground;
    COLORREF background = CLR_INVALID;
    const wchar_t* face = uiFace;
    int size = proseSize;
    bool bold = false;
    bool strike = false;
};

void setStyle(HWND edit, const Style& style, WPARAM target) {
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR | CFM_BACKCOLOR | CFM_FACE | CFM_SIZE | CFM_BOLD | CFM_STRIKEOUT;
    format.dwEffects = (style.background == CLR_INVALID ? CFE_AUTOBACKCOLOR : 0)
        | (style.bold ? CFE_BOLD : 0) | (style.strike ? CFE_STRIKEOUT : 0);
    format.crTextColor = style.foreground;
    format.crBackColor = style.background;
    format.yHeight = style.size;
    lstrcpynW(format.szFaceName, style.face, LF_FACESIZE);
    SendMessageW(edit, EM_SETCHARFORMAT, target, reinterpret_cast<LPARAM>(&format));
}

void setColor(HWND edit, size_t start, size_t end, COLORREF color) {
    selectText(edit, start, end);
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR;
    format.crTextColor = color;
    SendMessageW(edit, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
}

bool wordChar(wchar_t ch) {
    return iswalnum(ch) || ch == L'_';
}

bool wordIn(View list, View token) {
    if (token.empty()) return false;
    size_t pos = list.find(token);
    while (pos != View::npos) {
        if ((pos == 0 || list[pos - 1] == L' ') &&
            (pos + token.size() == list.size() || list[pos + token.size()] == L' ')) return true;
        pos = list.find(token, pos + 1);
    }
    return false;
}

constexpr View commonKeywords =
    L"abstract alignas alignof and as asm async await auto bool boolean break byte case catch char class const constexpr continue def defer delete do double dyn elif else enum except explicit export extends extern false final finally float fn for foreach from func function goto if impl implements import in inline int interface internal is lambda let long loop match mod mut namespace new nil noexcept none nonlocal not null nullptr object of operator or override package pass private protected pub public raise ref return sealed self short signed sizeof static string struct super switch synchronized template this throw throws trait true try type typedef typename union unsafe unsigned use using val var virtual void volatile when where while with yield";
constexpr View sqlKeywords =
    L"select from where as distinct order by group having limit offset join left right inner outer full cross on insert into values update set delete create drop alter table index database schema union all and or not is null true false count sum avg min max case when then else end asc desc primary key references foreign exists in like between begin commit rollback returning with over partition";

constexpr View controlKeywords =
    L"as async await break case catch continue defer do elif else except export finally for foreach from goto if import in match of pass raise return switch throw try using when where while with yield";
constexpr View functionDeclarations = L"def fn func function";
constexpr View typeDeclarations = L"class enum interface struct trait";
constexpr View pythonKeywords =
    L"and as assert async await break case class continue def del elif else except finally for from global if import in is lambda match nonlocal not or pass raise return try while with yield";
constexpr View javascriptKeywords =
    L"as async await break case catch class const continue debugger default delete do else export extends false finally for from function if import in instanceof let new null of return static super switch this throw true try typeof var void while with yield";
constexpr View typescriptKeywords =
    L"abstract any asserts boolean declare enum implements infer interface is keyof namespace never number object override private protected public readonly satisfies string symbol type undefined unique unknown";
constexpr View pythonTypes = L"bool bytearray bytes complex dict float frozenset int list range set str tuple";

COLORREF identifierColor(View token, View previous, bool call, bool member, int language, const SyntaxColors& syntax) {
    if (language == 12) return wordIn(sqlKeywords, lower(token)) ? syntax.keyword : call ? syntax.function : CLR_INVALID;
    if (wordIn(functionDeclarations, previous) || (member && call)) return syntax.function;
    if (wordIn(typeDeclarations, previous)) return syntax.type;
    if (language == pythonLanguage) {
        if (wordIn(pythonTypes, token)) return syntax.type;
        if (wordIn(L"False None True", token)) return syntax.keyword;
        if (token == L"self" || token == L"cls") return syntax.variable;
    }
    const bool script = language == javascriptLanguage || language == typescriptLanguage || language == jqueryLanguage;
    const View words = language == pythonLanguage ? pythonKeywords : script ? javascriptKeywords : commonKeywords;
    if (wordIn(words, token) || (language == typescriptLanguage && wordIn(typescriptKeywords, token)))
        return wordIn(controlKeywords, token) ? syntax.control : syntax.keyword;
    if (call) return syntax.function;
    if (script) return syntax.variable;
    return CLR_INVALID;
}

struct LexState { View close; COLORREF color = CLR_INVALID; };

void colorCode(HWND edit, View line, size_t origin, int language, LexState& state, int& budget, const Palette& colors) {
    if (!language || language == 21 || budget <= 0) return;
    const bool hashComment = language == 1 || language == 13 || language == 14 || language == 16 || language == 17 || language == 18;
    const bool sql = language == 12;
    const bool html = language == 10;
    const bool json = language == 15;
    const bool slashComment = !hashComment && !sql && !html && !json && language != 11;
    const bool cBlock = slashComment || sql || language == 11 || language == 18;
    const bool javascript = language == javascriptLanguage || language == typescriptLanguage || language == jqueryLanguage;
    const auto& syntax = colors.syntax;
    const auto identifier = [javascript](wchar_t ch) { return wordChar(ch) || (javascript && ch == L'$'); };
    View previous;
    bool member = false;

    for (size_t i = 0; i < line.size() && budget > 0;) {
        const size_t start = i;
        COLORREF color = CLR_INVALID;
        const View rest = line.substr(i);
        const wchar_t ch = line[i];

        if (!state.close.empty()) {
            color = state.color;
            const size_t end = rest.find(state.close);
            i = end == View::npos ? line.size() : i + end + state.close.size();
            if (end != View::npos) state = {};
        } else if ((hashComment && ch == L'#') || ((slashComment || language == 18) && rest.starts_with(L"//")) ||
                   (sql && rest.starts_with(L"--"))) {
            color = syntax.comment;
            i = line.size();
        } else if ((cBlock && rest.starts_with(L"/*")) || (html && rest.starts_with(L"<!--"))) {
            const View close = html ? L"-->" : L"*/";
            const size_t skip = html ? 4 : 2;
            const size_t end = rest.find(close, skip);
            color = syntax.comment;
            i = end == View::npos ? line.size() : i + end + close.size();
            if (end == View::npos) state = {close, color};
        } else if (language == pythonLanguage && (rest.starts_with(L"\"\"\"") || rest.starts_with(L"'''"))) {
            const View close = ch == L'"' ? L"\"\"\"" : L"'''";
            const size_t end = rest.find(close, 3);
            color = syntax.string;
            i = end == View::npos ? line.size() : i + end + close.size();
            if (end == View::npos) state = {close, color};
        } else if (ch == L'"' || ch == L'\'' || ((javascript || language == 8) && ch == 0x60)) {
            color = syntax.string;
            for (++i; i < line.size(); ++i) {
                if (line[i] == L'\\' && i + 1 < line.size()) { ++i; continue; }
                if (line[i] == ch) { ++i; break; }
            }
        } else if (ch >= L'0' && ch <= L'9') {
            color = syntax.number;
            for (++i; i < line.size() && (wordChar(line[i]) || line[i] == L'.'); ++i) {}
        } else if (identifier(ch)) {
            for (++i; i < line.size() && identifier(line[i]); ++i) {}
            const View token = line.substr(start, i - start);
            const size_t next = line.find_first_not_of(L" \t", i);
            const bool call = next != View::npos && line[next] == L'(';
            color = identifierColor(token, previous, call, member, language, syntax);
            previous = token;
            member = false;
        } else {
            if (html && (ch == L'<' || ch == L'>')) color = syntax.tag;
            if (ch != L' ' && ch != L'\t') {
                previous = {};
                member = ch == L'.';
            }
            ++i;
        }

        if (color != CLR_INVALID) {
            setColor(edit, origin + start, origin + i, color);
            --budget;
        }
    }
}

ITextDocument* textDocument(HWND edit) {
    IRichEditOle* ole = nullptr;
    ITextDocument* document = nullptr;
    if (SendMessageW(edit, EM_GETOLEINTERFACE, 0, reinterpret_cast<LPARAM>(&ole)) && ole) {
        ole->QueryInterface(__uuidof(ITextDocument), reinterpret_cast<void**>(&document));
        ole->Release();
    }
    return document;
}

void formatEditor(HWND edit, View value, bool normal, int lang, const Palette& colors) {
    const auto old = selection(edit);
    POINT scroll{};
    SendMessageW(edit, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    ITextDocument* document = textDocument(edit);
    if (document) document->Undo(tomSuspend, nullptr);
    SendMessageW(edit, WM_SETREDRAW, FALSE, 0);

    const Style base = normal ? Style{colors.ink} : Style{colors.codeInk, CLR_INVALID, codeFace, codeSize};
    setStyle(edit, base, SCF_ALL);
    int budget = 3200;
    const size_t limit = std::min<size_t>(value.size(), 48000);
    bool inBlock = false;
    LexState lexer;
    for (size_t start = 0; start < limit && budget > 0;) {
        const size_t end = lineEnd(value, start);
        if (end > 48000) break;
        const View line = value.substr(start, end - start);
        if (normal && line.starts_with(fenceMark)) {
            selectText(edit, start, std::min(end + 1, value.size()));
            setStyle(edit, {colors.muted, colors.codeFill, codeFace, 210}, SCF_SELECTION);
            lang = languageIndex(trimRight(line.substr(3)));
            inBlock = !inBlock;
            lexer = {};
            --budget;
        } else if (normal && inBlock) {
            selectText(edit, start, std::min(end + 1, value.size()));
            setStyle(edit, {colors.codeInk, colors.codeFill, codeFace, codeSize}, SCF_SELECTION);
            --budget;
            colorCode(edit, line, start, lang, lexer, budget, colors);
        } else if (normal && line.starts_with(L"# ")) {
            selectText(edit, start, end);
            setStyle(edit, {colors.ink, CLR_INVALID, uiFace, 330, true}, SCF_SELECTION);
            setColor(edit, start, start + 1, colors.quiet);
            --budget;
        } else if (normal && line.starts_with(L"## ")) {
            selectText(edit, start, end);
            setStyle(edit, {colors.ink, CLR_INVALID, uiFace, 265, true}, SCF_SELECTION);
            setColor(edit, start, start + 2, colors.quiet);
            --budget;
        } else if (normal && (line.starts_with(L"- [ ] ") || line.starts_with(L"- [x] "))) {
            const bool done = line[3] == L'x';
            selectText(edit, start, end);
            setStyle(edit, {done ? colors.muted : colors.ink, CLR_INVALID, uiFace, proseSize, false, done}, SCF_SELECTION);
            setColor(edit, start, start + 5, done ? colors.muted : colors.ink);
            --budget;
        } else if (!normal) {
            colorCode(edit, line, start, lang, lexer, budget, colors);
        } else {
            size_t open = line.find(L"**");
            while (open != View::npos && budget > 0) {
                size_t close = line.find(L"**", open + 2);
                if (close == View::npos) break;
                if (close > open + 2) {
                    selectText(edit, start + open + 2, start + close);
                    setStyle(edit, {colors.ink, CLR_INVALID, uiFace, proseSize, true}, SCF_SELECTION);
                    setColor(edit, start + open, start + open + 2, colors.quiet);
                    setColor(edit, start + close, start + close + 2, colors.quiet);
                    --budget;
                }
                open = line.find(L"**", close + 2);
            }
        }
        start = end + 1;
    }

    SendMessageW(edit, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&old));
    SendMessageW(edit, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    if (document) {
        document->Undo(tomResume, nullptr);
        document->Release();
    }
    SendMessageW(edit, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(edit, nullptr, FALSE);
}

void setupEditor(HWND edit, bool normal, const Palette& colors) {
    const Style base = normal ? Style{colors.ink} : Style{colors.codeInk, CLR_INVALID, codeFace, codeSize};
    setStyle(edit, base, SCF_ALL);
    SendMessageW(edit, EM_SETBKGNDCOLOR, 0, normal ? colors.paper : colors.codePaper);
    PARAFORMAT2 format{};
    format.cbSize = sizeof(format);
    format.dwMask = PFM_LINESPACING | PFM_TABSTOPS;
    format.bLineSpacingRule = 5;
    format.dyLineSpacing = normal ? 28 : 27;
    format.cTabCount = 12;
    for (int i = 0; i < format.cTabCount; ++i) format.rgxTabs[i] = (i + 1) * 496;
    SendMessageW(edit, EM_SETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&format));
}

void indentSelection(HWND edit, View value, bool unindent) {
    const auto range = selection(edit);
    const size_t start = lineStart(value, range.cpMin);
    const size_t last = range.cpMax > range.cpMin ? range.cpMax - 1 : range.cpMax;
    const size_t end = lineEnd(value, last);
    if (range.cpMin == range.cpMax && !unindent) {
        insertText(edit, Text(4 - (range.cpMin - start) % 4, L' '));
        return;
    }
    if (!unindent && value.find(L'\r', range.cpMin) >= static_cast<size_t>(range.cpMax)) {
        insertText(edit, L"    ");
        return;
    }
    Text modified;
    size_t removedFirst = 0;
    for (size_t i = start; i <= end;) {
        const size_t next = lineEnd(value, i);
        const View line = value.substr(i, next - i);
        size_t drop = 0;
        if (unindent) {
            if (line.starts_with(L"\t")) drop = 1;
            else while (drop < 4 && drop < line.size() && line[drop] == L' ') ++drop;
        } else modified += L"    ";
        if (i == start) removedFirst = drop;
        modified += line.substr(drop);
        if (next < end) modified += L'\r';
        i = next + 1;
    }
    selectText(edit, start, end);
    insertText(edit, modified);
    if (range.cpMin == range.cpMax) {
        const size_t caret = std::max(start, static_cast<size_t>(range.cpMin) - std::min<size_t>(removedFirst, range.cpMin));
        selectText(edit, caret, caret);
    } else selectText(edit, start, start + modified.size());
}
