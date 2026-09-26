#pragma once

#include <windows.h>
#include <string>

namespace Gety {

class Dialogs {
public:
    static bool ShowNewDownloadDialog(HWND hParent,
                                      const std::wstring& initialUrl,
                                      std::wstring& outUrl,
                                      std::wstring& outFilename,
                                      std::wstring& outSaveDir,
                                      std::wstring& outCategory,
                                      int& outSplitParts,
                                      bool& outStartImmediately);

    static bool ShowBatchDownloadDialog(HWND hParent);
    static bool ShowVideoLinkDialog(HWND hParent, const std::wstring& initialUrl = L"");
    static bool ShowOllamaModelDialog(HWND hParent, const std::wstring& initialModel = L"");
    static bool ShowHuggingFaceModelDialog(HWND hParent, const std::wstring& initialModel = L"");
    static bool ShowModelDownloaderDialog(HWND hParent, int initialTab = 0, const std::wstring& initialModel = L"");

    static bool ShowOptionsDialog(HWND hParent);

    static void ShowAboutDialog(HWND hParent);
    static bool ShowUpdateUrlDialog(HWND hParent, const std::wstring& currentUrl, std::wstring& outNewUrl);
    static bool ShowRenameDialog(HWND hParent, const std::wstring& currentFilename, std::wstring& outNewFilename);
    static bool ShowDeleteConfirmDialog(HWND hParent, int taskCount, bool& outDeleteFile, bool defaultDeleteFile = false);

    static void SetDialogSnapshotTarget(const std::wstring& path);
};

} // namespace Gety
