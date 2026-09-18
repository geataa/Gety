#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <sstream>
#include "ui/MainWindow.h"
#include "ui/Dialogs.h"
#include "core/Config.h"
#include "engine/DownloadManager.h"
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PWSTR pCmdLine, int nCmdShow) {
    // Set DPI awareness for modern crisp displays
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Initialize COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // Initialize Common Controls v6
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_COOL_CLASSES | ICC_USEREX_CLASSES | ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS | ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    // Check if --snapshot argument was passed
    std::wstring cmdLine = GetCommandLineW();

    size_t snapDlgPos = cmdLine.find(L"--snapshot-dialog");
    if (snapDlgPos != std::wstring::npos) {
        std::wstring rest = cmdLine.substr(snapDlgPos + 17);
        while (!rest.empty() && (rest.front() == L' ' || rest.front() == L'=')) rest.erase(0, 1);
        std::wistringstream iss(rest);
        std::wstring dlgType, snapPath;
        iss >> dlgType >> snapPath;
        if (!snapPath.empty() && snapPath.front() == L'"') snapPath.erase(0, 1);
        if (!snapPath.empty() && snapPath.back() == L'"') snapPath.pop_back();
        if (snapPath.empty()) snapPath = dlgType + L"_dialog_snapshot.png";

        Gety::Dialogs::SetDialogSnapshotTarget(snapPath);

        if (dlgType == L"new") {
            std::wstring url = L"https://releases.ubuntu.com/24.04/ubuntu-24.04.1-desktop-amd64.iso";
            std::wstring outUrl, outFn, outDir, outCat;
            int splitParts = 8;
            bool startImm = true;
            Gety::Dialogs::ShowNewDownloadDialog(NULL, url, outUrl, outFn, outDir, outCat, splitParts, startImm);
        } else if (dlgType == L"new_clipboard") {
            std::wstring outUrl, outFn, outDir, outCat;
            int splitParts = 8;
            bool startImm = true;
            Gety::Dialogs::ShowNewDownloadDialog(NULL, L"", outUrl, outFn, outDir, outCat, splitParts, startImm);
        } else if (dlgType == L"batch") {
            Gety::Dialogs::ShowBatchDownloadDialog(NULL);
        } else if (dlgType == L"video") {
            std::wstring videoUrl;
            iss >> videoUrl;
            if (!videoUrl.empty() && videoUrl.front() == L'"') videoUrl.erase(0, 1);
            if (!videoUrl.empty() && videoUrl.back() == L'"') videoUrl.pop_back();
            Gety::Dialogs::ShowVideoLinkDialog(NULL, videoUrl);
        } else if (dlgType == L"ollama" || dlgType == L"ai") {
            std::wstring modelName;
            if (iss >> modelName) {
                if (!modelName.empty() && modelName.front() == L'"') modelName.erase(0, 1);
                if (!modelName.empty() && modelName.back() == L'"') modelName.pop_back();
            }
            if (modelName.empty()) {
                modelName = L"nomic-embed-text-v2-moe:latest";
            }
            Gety::Dialogs::ShowOllamaModelDialog(NULL, modelName);
        } else if (dlgType == L"hf" || dlgType == L"huggingface") {
            std::wstring modelName;
            if (iss >> modelName) {
                if (!modelName.empty() && modelName.front() == L'"') modelName.erase(0, 1);
                if (!modelName.empty() && modelName.back() == L'"') modelName.pop_back();
            }
            if (modelName.empty()) {
                modelName = L"TheBloke/Mistral-7B-Instruct-v0.2-GGUF";
            }
            Gety::Dialogs::ShowHuggingFaceModelDialog(NULL, modelName);
        } else if (dlgType == L"options") {
            Gety::Dialogs::ShowOptionsDialog(NULL);
        } else if (dlgType == L"about") {
            Gety::Dialogs::ShowAboutDialog(NULL);
        } else if (dlgType == L"update_url") {
            std::wstring outNew;
            Gety::Dialogs::ShowUpdateUrlDialog(NULL, L"https://example.com/downloads/temp_token_expired.zip", outNew);
        }

        if (SUCCEEDED(hr)) CoUninitialize();
        return 0;
    }

    size_t snapDzPos = cmdLine.find(L"--snapshot-dropzone");
    if (snapDzPos != std::wstring::npos) {
        std::wstring rest = cmdLine.substr(snapDzPos + 19);
        while (!rest.empty() && (rest.front() == L' ' || rest.front() == L'=')) rest.erase(0, 1);
        std::wistringstream iss(rest);
        std::wstring mode, snapPath;
        iss >> mode >> snapPath;
        if (!snapPath.empty() && snapPath.front() == L'"') snapPath.erase(0, 1);
        if (!snapPath.empty() && snapPath.back() == L'"') snapPath.pop_back();
        if (snapPath.empty()) snapPath = mode + L"_dropzone.png";

        Gety::FloatingWindow dz;
        dz.Create(NULL, 100, 100, 255);
        bool ok = false;
        if (mode == L"active") {
            ok = dz.SaveSnapshot(snapPath, 14.80 * 1024 * 1024, 2, 0.425);
        } else {
            ok = dz.SaveSnapshot(snapPath, 0.0, 0, 0.0);
        }

        if (SUCCEEDED(hr)) CoUninitialize();
        return ok ? 0 : 1;
    }

    size_t snapPos = cmdLine.find(L"--snapshot");
    if (snapPos != std::wstring::npos) {
        std::wstring snapPath = cmdLine.substr(snapPos + 10);
        while (!snapPath.empty() && (snapPath.front() == L' ' || snapPath.front() == L'=')) {
            snapPath.erase(0, 1);
        }
        while (!snapPath.empty() && snapPath.back() == L' ') {
            snapPath.pop_back();
        }
        if (!snapPath.empty() && snapPath.front() == L'"') snapPath.erase(0, 1);
        if (!snapPath.empty() && snapPath.back() == L'"') snapPath.pop_back();
        while (!snapPath.empty() && snapPath.back() == L' ') {
            snapPath.pop_back();
        }
        if (snapPath.empty()) {
            snapPath = L"gety_snapshot.png";
        }

        bool created = Gety::MainWindow::Instance().Create(hInstance, SW_HIDE);
        bool saved = false;
        if (created) {
            saved = Gety::MainWindow::Instance().SaveSnapshot(snapPath);
        }

        FILE* fp = nullptr;
        _wfopen_s(&fp, L"snapshot_debug.log", L"w");
        if (fp) {
            fwprintf(fp, L"cmdLine: %s\nsnapPath: %s\ncreated: %d\nsaved: %d\n", cmdLine.c_str(), snapPath.c_str(), created ? 1 : 0, saved ? 1 : 0);
            fclose(fp);
        }

        if (SUCCEEDED(hr)) CoUninitialize();
        return saved ? 0 : 1;
    }

    // Single instance mutex
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"SkySoft_Gety_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(L"GetyMainWindow", NULL);
        if (hExisting) {
            ShowWindow(hExisting, SW_RESTORE);
            SetForegroundWindow(hExisting);

            // If command line provided an URL, forward it via WM_COPYDATA
            if (pCmdLine && wcslen(pCmdLine) > 0) {
                COPYDATASTRUCT cds = { 0 };
                cds.dwData = 1;
                cds.cbData = (DWORD)((wcslen(pCmdLine) + 1) * sizeof(wchar_t));
                cds.lpData = (PVOID)pCmdLine;
                SendMessageW(hExisting, WM_COPYDATA, 0, (LPARAM)&cds);
            }
        }
        if (hMutex) CloseHandle(hMutex);
        if (SUCCEEDED(hr)) CoUninitialize();
        return 0;
    }

    // Check if started minimized (e.g. from Windows autostart)
    bool startMinimized = (cmdLine.find(L"--minimized") != std::wstring::npos ||
                           cmdLine.find(L"--autostart") != std::wstring::npos ||
                           cmdLine.find(L"--tray") != std::wstring::npos);
    int initialShow = startMinimized ? SW_HIDE : nCmdShow;

    // Create and run Main Window
    if (Gety::MainWindow::Instance().Create(hInstance, initialShow)) {
        // If command-line URL given, add it
        if (pCmdLine && wcslen(pCmdLine) > 0) {
            std::wstring cmdStr = pCmdLine;
            // Strip quotes if any
            if (cmdStr.front() == L'"' && cmdStr.back() == L'"') {
                cmdStr = cmdStr.substr(1, cmdStr.length() - 2);
            }
            if (!cmdStr.empty()) {
                Gety::DownloadManager::Instance().AddTask(cmdStr, Gety::Config::Instance().defaultSavePath, L"Genel", Gety::Config::Instance().defaultSplitParts, true);
            }
        }

        Gety::MainWindow::Instance().RunMessageLoop();
    }

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    if (SUCCEEDED(hr)) {
        CoUninitialize();
    }

    return 0;
}
