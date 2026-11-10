
#pragma once

struct SyntaxColors {
    COLORREF keyword, control, function, type, variable, string, number, comment, tag;
};

struct Palette {
    COLORREF ink, muted, quiet;
    COLORREF paper, side, line, border, hover, selected, thumb;
    COLORREF codePaper, codeFill, codeInk;
    SyntaxColors syntax;
};

constexpr COLORREF gray(BYTE shade) { return RGB(shade, shade, shade); }

constexpr COLORREF hexColor(uint32_t rgb) { return RGB(rgb >> 16, rgb >> 8, rgb); }

// Token colors from the VS Code Light+ and Dark+ themes.
constexpr SyntaxColors lightSyntax{
    hexColor(0x0000FF), hexColor(0xAF00DB), hexColor(0x795E26), hexColor(0x267F99), hexColor(0x001080),
    hexColor(0xA31515), hexColor(0x098658), hexColor(0x008000), hexColor(0x800000)
};
constexpr SyntaxColors darkSyntax{
    hexColor(0x569CD6), hexColor(0xC586C0), hexColor(0xDCDCAA), hexColor(0x4EC9B0), hexColor(0x9CDCFE),
    hexColor(0xCE9178), hexColor(0xB5CEA8), hexColor(0x6A9955), hexColor(0x569CD6)
};

constexpr Palette lightPalette{
    gray(34), gray(92), gray(108),
    gray(255), gray(247), gray(231), gray(219), gray(239), gray(232), gray(193),
    gray(249), gray(242), gray(0), lightSyntax
};

constexpr Palette darkPalette{
    gray(229), gray(175), gray(152),
    gray(25), gray(31), gray(51), gray(61), gray(43), gray(51), gray(85),
    gray(31), gray(39), gray(212), darkSyntax
};

constexpr wchar_t appName[] = L"Defter";
constexpr wchar_t testName[] = L"Defter - test";

constexpr int zoomLevels[] = {50, 67, 80, 90, 100, 110, 125, 150, 175, 200, 250, 300};
constexpr int defaultZoom = 100;

struct Preferences {
    bool dark = false;
    bool sidebar = true;
    int zoom = defaultZoom;

    void load(const Text& path) {
        if (path.empty()) return;
        HANDLE file = CreateFileW((path + L".ui").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return;
        unsigned char data[4]{};
        DWORD size = 0;
        if (ReadFile(file, data, sizeof(data), &size, nullptr) && size >= 2 && data[0] == 'D') {
            dark = (data[1] & 1) != 0;
            sidebar = (data[1] & 2) == 0;
            if (size == sizeof(data)) {
                const int savedZoom = data[2] | (data[3] << 8);
                if (std::binary_search(std::begin(zoomLevels), std::end(zoomLevels), savedZoom)) zoom = savedZoom;
            }
        }
        CloseHandle(file);
    }

    void save(const Text& path) const {
        if (path.empty()) return;
        const char data[]{'D', static_cast<char>((dark ? 1 : 0) | (sidebar ? 0 : 2)),
            static_cast<char>(zoom & 255), static_cast<char>(zoom >> 8)};
        const Text settings = path + L".ui";
        const Text pending = settings + L".tmp";
        if (writeFile(pending, std::string_view(data, sizeof(data))))
            MoveFileExW(pending.c_str(), settings.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    }
};
