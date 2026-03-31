#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>
#include <commctrl.h>

namespace {

constexpr int kRefreshSeconds = 5;
constexpr int kBarWidth = 32;
constexpr int kInputEditId = 1001;
constexpr int kConfirmButtonId = 1002;
constexpr int kViewerEditId = 1003;
constexpr int kSaveButtonId = 1004;

std::atomic<bool> g_done{false};
std::wstring g_user_input;
std::wstring g_file_content;
std::wstring g_file_display_name;
std::mutex g_input_mutex;
HWND g_input_edit = nullptr;
HWND g_viewer_edit = nullptr;

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    int size = WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    int size = MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring result(size, L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::wstring LocalToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    int size = MultiByteToWideChar(
        CP_ACP, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring result(size, L'\0');
    MultiByteToWideChar(
        CP_ACP, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::wstring Utf16BeToWide(const std::vector<char>& bytes, size_t start) {
    std::wstring result;
    for (size_t i = start; i + 1 < bytes.size(); i += 2) {
        wchar_t ch = static_cast<unsigned char>(bytes[i]) << 8 |
                     static_cast<unsigned char>(bytes[i + 1]);
        result.push_back(ch);
    }
    return result;
}

std::wstring LoadTextFile(const std::filesystem::path& file_path) {
    std::ifstream input(file_path, std::ios::binary);
    if (!input) {
        return L"Unable to open file.";
    }

    std::vector<char> bytes((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());
    if (bytes.empty()) {
        return L"";
    }

    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        return Utf8ToWide(std::string(bytes.begin() + 3, bytes.end()));
    }

    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        return std::wstring(reinterpret_cast<const wchar_t*>(bytes.data() + 2),
                            (bytes.size() - 2) / sizeof(wchar_t));
    }

    if (bytes.size() >= 2 &&
        static_cast<unsigned char>(bytes[0]) == 0xFE &&
        static_cast<unsigned char>(bytes[1]) == 0xFF) {
        return Utf16BeToWide(bytes, 2);
    }

    std::wstring utf8_text = Utf8ToWide(std::string(bytes.begin(), bytes.end()));
    if (!utf8_text.empty()) {
        return utf8_text;
    }
    return LocalToWide(std::string(bytes.begin(), bytes.end()));
}

bool SaveDisplayedText(std::wstring* saved_path) {
    try {
        std::filesystem::path save_dir = std::filesystem::current_path() / "aimonitor_save";
        std::filesystem::create_directories(save_dir);

        std::wstring file_name = g_file_display_name.empty() ? L"aimonitor_saved.txt" : g_file_display_name;
        std::filesystem::path output_path = save_dir / file_name;

        std::ofstream output(output_path, std::ios::binary);
        if (!output) {
            return false;
        }

        std::string utf8_content = WideToUtf8(g_file_content);
        output.write(utf8_content.data(), static_cast<std::streamsize>(utf8_content.size()));
        output.close();

        if (!output) {
            return false;
        }

        if (saved_path != nullptr) {
            *saved_path = output_path.wstring();
        }
        return true;
    } catch (...) {
        return false;
    }
}

std::string RenderProgressLine(const std::string& value) {
    double ratio = 0.0;
    try {
        ratio = std::stod(value) / 100.0;
    } catch (...) {
        ratio = 0.0;
    }
    if (ratio > 1.0) {
        ratio = 1.0;
    }
    int filled = static_cast<int>(ratio * kBarWidth);
    if (filled > kBarWidth) {
        filled = kBarWidth;
    }
    return "\r[" + std::string(filled, '=') + std::string(kBarWidth - filled, '.') + "] " + value + "%";
}

class ProgressGenerator {
public:
    std::string Next() {
        if (integer_part_ <= 99) {
            return std::to_string(integer_part_++);
        }

        if (suffix_.empty() || index_ >= 9) {
            suffix_.push_back('9');
            index_ = 0;
        }
        return "99." + suffix_ + digits_[index_++];
    }

private:
    int integer_part_ = 1;
    std::string suffix_;
    size_t index_ = 0;
    const std::string digits_ = "123456789";
};

void PrintProgress() {
    ProgressGenerator generator;

    while (!g_done.load()) {
        std::string value = generator.Next();
        std::cout << RenderProgressLine(value) << std::flush;

        for (int i = 0; i < kRefreshSeconds * 10; ++i) {
            if (g_done.load()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::cout << std::endl;
}

void SubmitAndClose(HWND hwnd) {
    int length = GetWindowTextLengthW(g_input_edit);
    std::wstring value(length, L'\0');
    if (length > 0) {
        GetWindowTextW(g_input_edit, value.data(), length + 1);
    }
    {
        std::lock_guard<std::mutex> lock(g_input_mutex);
        g_user_input = value;
    }
    g_done.store(true);
    DestroyWindow(hwnd);
}

LRESULT CALLBACK EditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                          UINT_PTR subclassId, DWORD_PTR refData) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        SubmitAndClose(GetParent(hwnd));
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

        HWND file_label = CreateWindowExW(
            0, L"STATIC",
            g_file_display_name.empty() ? L"No file provided. The text area is empty." : g_file_display_name.c_str(),
            WS_CHILD | WS_VISIBLE, 16, 14, 560, 22, hwnd, nullptr,
            GetModuleHandleW(nullptr), nullptr);
        SendMessageW(file_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

        g_viewer_edit = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", g_file_content.c_str(),
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
            16, 40, 560, 250, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kViewerEditId)),
            GetModuleHandleW(nullptr), nullptr);
        SendMessageW(g_viewer_edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

        HWND input_label = CreateWindowExW(
            0, L"STATIC", L"Enter instructions for the agent, then press Enter or click Confirm:",
            WS_CHILD | WS_VISIBLE, 16, 304, 360, 24, hwnd, nullptr,
            GetModuleHandleW(nullptr), nullptr);
        SendMessageW(input_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

        g_input_edit = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            16, 336, 560, 28, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kInputEditId)),
            GetModuleHandleW(nullptr), nullptr);
        SendMessageW(g_input_edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SetWindowSubclass(g_input_edit, EditProc, 1, 0);

        HWND save_button = CreateWindowExW(
            0, L"BUTTON", L"Save Text",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            390, 376, 90, 30, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSaveButtonId)),
            GetModuleHandleW(nullptr), nullptr);
        SendMessageW(save_button, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

        HWND confirm_button = CreateWindowExW(
            0, L"BUTTON", L"Confirm",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            501, 376, 75, 30, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kConfirmButtonId)),
            GetModuleHandleW(nullptr), nullptr);
        SendMessageW(confirm_button, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

        SetFocus(g_input_edit);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == kConfirmButtonId && HIWORD(wParam) == BN_CLICKED) {
            SubmitAndClose(hwnd);
            return 0;
        }
        if (LOWORD(wParam) == kSaveButtonId && HIWORD(wParam) == BN_CLICKED) {
            std::wstring saved_path;
            bool ok = SaveDisplayedText(&saved_path);
            MessageBoxW(
                hwnd,
                ok ? (L"Text saved to:\n" + saved_path).c_str() : L"Save failed. Please make sure the current working directory is writable.",
                ok ? L"Saved" : L"Save Failed",
                MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONERROR));
            return 0;
        }
        break;
    case WM_CLOSE:
        g_done.store(true);
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int RunInputWindow() {
    const wchar_t* class_name = L"AIMonitorInputWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, class_name, L"AgentCarryOn",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 610, 460,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    InitCommonControls();

    if (argc >= 2) {
        std::filesystem::path input_path(argv[1]);
        g_file_display_name = input_path.filename().wstring();
        g_file_content = LoadTextFile(input_path);
    } else {
        g_file_content = L"";
        g_file_display_name = L"";
    }

    std::thread progress_thread(PrintProgress);
    int window_result = RunInputWindow();

    g_done.store(true);
    if (progress_thread.joinable()) {
        progress_thread.join();
    }

    if (window_result != 0) {
        return window_result;
    }

    std::wstring value;
    {
        std::lock_guard<std::mutex> lock(g_input_mutex);
        value = g_user_input;
    }

    if (!value.empty()) {
        std::cout << WideToUtf8(value) << std::endl;
    }

    return 0;
}
