
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>

using Text = std::wstring;
using View = std::wstring_view;

enum class Kind : uint32_t { Note, Code, Study };

struct Language {
    const wchar_t* name;
    const wchar_t* tag;
    const wchar_t* extension;
};

constexpr Language languages[] = {
    {L"Düz metin", L"text", L"txt"}, {L"Python", L"python", L"py"},
    {L"JavaScript", L"javascript", L"js"}, {L"TypeScript", L"typescript", L"ts"},
    {L"C", L"c", L"c"}, {L"C++", L"cpp", L"cpp"}, {L"C#", L"csharp", L"cs"},
    {L"Java", L"java", L"java"}, {L"Go", L"go", L"go"}, {L"Rust", L"rust", L"rs"},
    {L"HTML", L"html", L"html"}, {L"CSS", L"css", L"css"},
    {L"SQL", L"sql", L"sql"}, {L"Bash", L"bash", L"sh"},
    {L"PowerShell", L"powershell", L"ps1"}, {L"JSON", L"json", L"json"},
    {L"YAML", L"yaml", L"yaml"}, {L"Ruby", L"ruby", L"rb"},
    {L"PHP", L"php", L"php"}, {L"Swift", L"swift", L"swift"},
    {L"Kotlin", L"kotlin", L"kt"}, {L"Markdown", L"markdown", L"md"},
    {L"jQuery", L"jquery", L"js"}
};

constexpr int pythonLanguage = 1;
constexpr int javascriptLanguage = 2;
constexpr int typescriptLanguage = 3;
constexpr int jqueryLanguage = 22;
static_assert(View(languages[jqueryLanguage].tag) == L"jquery");

constexpr size_t maxFileSize = 128 * 1024 * 1024;
constexpr LONG maxEditSize = 2'000'000;

struct Note {
    uint32_t id = 0;
    Kind kind = Kind::Note;
    uint32_t language = 0;
    Text title, body, practice;
};

Text label(const Note& note) {
    return note.title.empty() ? L"Başlıksız" : note.title;
}

Text lower(View value) {
    Text result(value);
    if (!result.empty()) CharLowerBuffW(result.data(), static_cast<DWORD>(result.size()));
    return result;
}

int languageIndex(View tag) {
    Text key = lower(tag);
    if (key == L"js") key = L"javascript";
    if (key == L"ts") key = L"typescript";
    if (key == L"py") key = L"python";
    if (key == L"c++") key = L"cpp";
    if (key == L"cs" || key == L"c#") key = L"csharp";
    for (int i = 0; i < static_cast<int>(std::size(languages)); ++i)
        if (key == languages[i].tag) return i;
    return 0;
}

std::string utf8(View value) {
    if (value.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string bytes(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), bytes.data(), size, nullptr, nullptr);
    return bytes;
}

Text fromUtf8(std::string_view bytes) {
    if (bytes.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    Text result(size, L'\0');
    if (size) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), result.data(), size);
    return result;
}

bool writeFile(const Text& path, std::string_view bytes) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr)
        && written == bytes.size() && FlushFileBuffers(file);
    return CloseHandle(file) && ok;
}

void putNumber(std::string& bytes, uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<char>(value >> (i * 8)));
}

void putText(std::string& bytes, View value) {
    const auto data = utf8(value);
    putNumber(bytes, static_cast<uint32_t>(data.size()));
    bytes += data;
}

struct Reader {
    std::string_view bytes;
    bool valid = true;

    uint32_t number() {
        if (bytes.size() < 4) { valid = false; return 0; }
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) value |= uint32_t(static_cast<unsigned char>(bytes[i])) << (i * 8);
        bytes.remove_prefix(4);
        return value;
    }

    Text string(size_t limit) {
        const uint32_t size = number();
        if (!valid || size > bytes.size() || size > limit * 4) { valid = false; return {}; }
        Text result = fromUtf8(bytes.substr(0, size));
        if ((size && result.empty()) || result.size() > limit || result.find(L'\0') != Text::npos) valid = false;
        bytes.remove_prefix(size);
        return result;
    }
};

struct Notebook {
    Text path;
    std::vector<Note> notes;
    uint32_t active = 0;
    uint32_t nextId = 1;
    bool preserveBackup = false;

    bool read(const Text& source) {
        HANDLE file = CreateFileW(source.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER length{};
        if (!GetFileSizeEx(file, &length) || length.QuadPart < 16 || length.QuadPart > maxFileSize) {
            CloseHandle(file);
            return false;
        }
        std::string bytes(static_cast<size_t>(length.QuadPart), '\0');
        DWORD count = 0;
        bool ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr) && count == bytes.size();
        CloseHandle(file);
        if (!ok || bytes.compare(0, 4, "YPR1") != 0) return false;

        Reader reader{std::string_view(bytes).substr(4)};
        const uint32_t chosen = reader.number();
        const uint32_t next = reader.number();
        const uint32_t total = reader.number();
        if (total > 10000 || total > reader.bytes.size() / 24 || !next) return false;

        std::vector<Note> loaded;
        std::vector<uint32_t> ids;
        loaded.reserve(total);
        ids.reserve(total);
        for (uint32_t i = 0; i < total && reader.valid; ++i) {
            Note item;
            item.id = reader.number();
            const uint32_t kind = reader.number();
            item.language = reader.number();
            if (kind > 2 || item.language >= std::size(languages) || !item.id || item.id >= next) return false;
            item.kind = static_cast<Kind>(kind);
            item.title = reader.string(140);
            item.body = reader.string(maxEditSize);
            item.practice = reader.string(maxEditSize);
            ids.push_back(item.id);
            loaded.push_back(std::move(item));
        }
        std::sort(ids.begin(), ids.end());
        if (!reader.valid || !reader.bytes.empty() || std::adjacent_find(ids.begin(), ids.end()) != ids.end()) return false;
        notes = std::move(loaded);
        active = std::binary_search(ids.begin(), ids.end(), chosen) ? chosen : (notes.empty() ? 0 : notes.front().id);
        nextId = next;
        return true;
    }

    bool save() {
        if (path.empty()) return false;
        std::string bytes = "YPR1";
        putNumber(bytes, active);
        putNumber(bytes, nextId);
        putNumber(bytes, static_cast<uint32_t>(notes.size()));
        for (const auto& note : notes) {
            putNumber(bytes, note.id);
            putNumber(bytes, static_cast<uint32_t>(note.kind));
            putNumber(bytes, note.language);
            putText(bytes, note.title);
            putText(bytes, note.body);
            putText(bytes, note.practice);
            if (bytes.size() > maxFileSize) return false;
        }
        const Text pending = path + L".tmp";
        const Text backup = path + L".bak";
        if (!writeFile(pending, bytes)) return false;
        if (!preserveBackup && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
            CopyFileW(path.c_str(), backup.c_str(), FALSE);
        const bool saved = MoveFileExW(pending.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        if (saved) preserveBackup = false;
        return saved;
    }
};

size_t lineStart(View value, size_t pos) {
    if (!pos) return 0;
    size_t previous = value.rfind(L'\r', pos - 1);
    return previous == View::npos ? 0 : previous + 1;
}

size_t lineEnd(View value, size_t pos) {
    return std::min(value.size(), value.find(L'\r', pos));
}

View trimRight(View line) {
    const auto end = line.find_last_not_of(L" \t");
    return end == View::npos ? View{} : line.substr(0, end + 1);
}

std::pair<int, View> comparableLine(View line) {
    line = trimRight(line);
    int width = 0;
    size_t offset = 0;
    while (offset < line.size() && (line[offset] == L' ' || line[offset] == L'\t')) {
        width += line[offset] == L'\t' ? 4 - width % 4 : 1;
        ++offset;
    }
    return {width, line.substr(offset)};
}

std::array<int, 3> compareLines(View reference, View attempt) {
    std::array<int, 3> totals{};
    if (reference.empty()) return totals;
    while (!reference.empty() && reference.back() == L'\r') reference.remove_suffix(1);
    if (reference.empty()) return totals;
    while (!attempt.empty() && attempt.back() == L'\r') attempt.remove_suffix(1);
    const bool started = !attempt.empty();
    size_t left = 0, right = 0;
    while (left <= reference.size()) {
        const size_t endLeft = lineEnd(reference, left);
        if (started && right <= attempt.size()) {
            const size_t endRight = lineEnd(attempt, right);
            if (comparableLine(reference.substr(left, endLeft - left)) == comparableLine(attempt.substr(right, endRight - right)))
                ++totals[0];
            right = endRight + 1;
        }
        ++totals[1];
        left = endLeft + 1;
    }
    totals[2] = started && totals[0] == totals[1] && right > attempt.size();
    return totals;
}

struct Fence {
    size_t opening = View::npos;
    size_t end = View::npos;
    int language = 0;
};

constexpr View fenceMark = L"\x60\x60\x60";

Fence fenceAt(View value, size_t caret) {
    Fence fence;
    for (size_t start = 0; start <= caret && start < value.size();) {
        size_t end = lineEnd(value, start);
        View line = value.substr(start, end - start);
        if (line.starts_with(fenceMark)) {
            if (fence.opening == View::npos) fence = {start, end, languageIndex(trimRight(line.substr(3)))};
            else if (caret <= end) return fence;
            else fence = {};
        }
        start = end + 1;
    }
    return fence;
}
