#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace Gety {

enum class LangId {
    Turkish,    // Türkçe
    English,    // English
    German,     // Deutsch
    French,     // Français
    Spanish,    // Español
    Russian,    // Русский
    Ukrainian,  // Українська
    Greek,      // Ελληνικά
    Italian,    // Italiano
    Portuguese, // Português
    Japanese,   // 日本語
    Chinese     // 简体中文
};

struct LanguageInfo {
    LangId id;
    std::wstring code;
    std::wstring name;
    std::wstring nativeName;
};

enum class StrId {
    // Menus
    MenuFile,
    MenuFileNew,
    MenuFileBatch,
    MenuFileOllamaModel,
    MenuFileHfModel,
    MenuFileExit,
    MenuTask,
    MenuTaskStart,
    MenuTaskPause,
    MenuTaskStop,
    MenuTaskOpenFile,
    MenuTaskOpenFolder,
    MenuTaskRedownload,
    MenuTaskDelete,
    MenuTaskMoveUp,
    MenuTaskMoveDown,
    MenuView,
    MenuViewDropZone,
    MenuViewToolbar,
    MenuViewStatusbar,
    MenuViewTree,
    MenuTools,
    MenuToolsClipboard,
    MenuToolsSpeed,
    MenuToolsSpeedUnlim,
    MenuToolsSpeedManual,
    MenuToolsSpeedBack,
    MenuToolsOptions,
    MenuLanguage,
    MenuHelp,
    MenuHelpAbout,

    // Toolbar
    TbNew,
    TbStart,
    TbPause,
    TbStop,
    TbDelete,
    TbRedownload,
    TbDropZone,
    TbOptions,
    TbSpeedUnlim,

    // Categories
    CatAll,
    CatUnfinished,
    CatDownloading,
    CatPaused,
    CatDownloaded,
    CatMusic,
    CatVideo,
    CatSoftware,
    CatGames,
    CatDocuments,
    CatArchives,
    CatAI,
    CatTrash,
    CatGeneral,

    // Columns
    ColFilename,
    ColSize,
    ColDownloaded,
    ColProgress,
    ColSpeed,
    ColTimeLeft,
    ColStatus,
    ColParts,
    ColUrl,
    ColDateAdded,

    // Bottom Panel Tabs
    TabLog,
    TabGrid,
    TabInfo,
    TabNoTask,

    // Status Bar
    StatusSpeed,
    StatusTasks,
    StatusFreeSpace,
    StatusDiskReady,
    StatusModeUnlim,
    StatusModeManual,
    StatusModeBack,

    // States
    StateQueued,
    StateConnecting,
    StateDownloading,
    StatePaused,
    StateCompleted,
    StateFailed,
    StateDeleted,
    StateUnknown,

    // Drop Zone & Tray
    DropZoneTitle,
    DropZoneActive,
    DropZoneShow,
    DropZoneHide,
    DropZoneOpacity,
    DropZoneOpacity50,
    DropZoneOpacity75,
    DropZoneOpacity100,
    TrayRestore,
    TrayResumeAll,
    TrayPauseAll,
    TrayBalloonTitle,
    TrayBalloonText,

    // Grid Visualizer
    GridDone,
    GridActive,
    GridPending,
    GridStats,

    // Dialogs
    DlgNewTitle,
    DlgNewUrl,
    DlgNewFilename,
    DlgNewDir,
    DlgNewBrowse,
    DlgNewPaste,
    DlgNewCat,
    DlgNewParts,
    DlgNewStartImm,
    DlgOk,
    DlgCancel,
    DlgClose,
    DlgSave,

    DlgBatchTitle,
    DlgBatchPattern,
    DlgBatchFrom,
    DlgBatchTo,
    DlgBatchDigits,
    DlgBatchPreview,
    DlgBatchList,
    DlgBatchAddAll,

    DlgOptTitle,
    DlgOptMaxTasks,
    DlgOptDefParts,
    DlgOptSpeedLimit,
    DlgOptClip,
    DlgOptSound,
    DlgOptDrop,
    DlgOptShutdown,
    DlgOptMinToTray,
    DlgOptAutoStart,
    DlgOptLanguage,

    DlgDeletePrompt,
    DlgDeleteTitle,
    DlgAboutTitle,

    // Action Bar Labels (for D2D top bar buttons)
    ActNew,
    ActBatch,
    ActVideoLink,
    ActOllamaModel,
    ActHfModel,
    ActStart,
    ActPause,
    ActDelete,
    ActSpeed,
    ActLanguage,
    ActSettings,
    ActAbout,

    // Context Menu Items
    CtxPause,
    CtxStart,
    CtxOpenFile,
    CtxOpenFolder,
    CtxCopyUrl,
    CtxDeleteTask,
    CtxOpenGety,

    // Misc
    DlgOptSaveDir,
    DlgOptMaxRetries,
    CtxUpdateUrl,
    DlgUpdateUrlTitle,
    DlgUpdateUrlOld,
    DlgUpdateUrlNew,
    DlgUpdateUrlBtn,
    DlgAboutDesc,

    // Video Link Dialog
    DlgVideoTitle,
    DlgVideoUrlPrompt,
    DlgVideoAnalyze,
    DlgVideoAnalyzing,
    DlgVideoQuality,
    DlgVideoPlaylistVideos,
    DlgVideoSelectAll,
    DlgVideoDeselectAll,
    DlgVideoAudioOnly,
    DlgVideoDownload,
    DlgVideoSaveDir,
    DlgVideoBrowse,
    DlgVideoFound,

    // Shared Model Dialog Tabs
    DlgModelTabOllama,
    DlgModelTabHf,

    // Ollama Model Dialog
    DlgOllamaTitle,
    DlgOllamaUrlPrompt,
    DlgOllamaAnalyze,
    DlgOllamaAnalyzing,
    DlgOllamaModelInfo,
    DlgOllamaFormatGguf,
    DlgOllamaFormatOriginal,
    DlgOllamaFilename,
    DlgOllamaDownload,
    DlgOllamaSaveDir,
    DlgOllamaParts,
    DlgOllamaLayers,
    DlgOllamaFound,

    // Hugging Face Model Dialog
    DlgHfTitle,
    DlgHfRepoPrompt,
    DlgHfTokenPrompt,
    DlgHfAnalyze,
    DlgHfAnalyzing,
    DlgHfModelInfo,
    DlgHfQuantPrompt,
    DlgHfFilename,
    DlgHfDownload,
    DlgHfSaveDir,
    DlgHfParts,
    DlgHfFound
};

class I18n {
public:
    static I18n& Instance();

    void SetLanguage(LangId lang);
    void SetLanguageByCode(const std::wstring& code);
    LangId GetCurrentLanguage() const { return m_currentLang; }
    std::wstring GetCurrentLanguageCode() const;

    const std::vector<LanguageInfo>& GetLanguages() const { return m_languages; }

    const wchar_t* Get(StrId id) const;

private:
    I18n();
    ~I18n() = default;

    void InitStrings();

    LangId m_currentLang = LangId::Turkish;
    std::vector<LanguageInfo> m_languages;
    std::unordered_map<LangId, std::unordered_map<StrId, std::wstring>> m_translations;
};

inline const wchar_t* LStr(StrId id) {
    return I18n::Instance().Get(id);
}

} // namespace Gety
