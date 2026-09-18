#pragma once

#define IDI_APP_ICON            101
#define IDI_TRAY_ICON           102
#define IDR_MAIN_MENU           103
#define IDR_ACCELERATOR         104

// Commands
#define ID_FILE_NEW             201
#define ID_FILE_BATCH_NEW       202
#define ID_FILE_VIDEO_LINK      204
#define ID_FILE_OLLAMA_MODEL    205
#define ID_FILE_HF_MODEL        206
#define ID_FILE_EXIT            203

#define ID_DOWNLOAD_START       210
#define ID_DOWNLOAD_PAUSE       211
#define ID_DOWNLOAD_STOP        212
#define ID_DOWNLOAD_DELETE      213
#define ID_DOWNLOAD_REDOWNLOAD  214
#define ID_DOWNLOAD_MOVEUP      215
#define ID_DOWNLOAD_MOVEDOWN    216
#define ID_DOWNLOAD_PROPERTIES  217
#define ID_DOWNLOAD_OPENFILE    218
#define ID_DOWNLOAD_OPENFOLDER  219
#define ID_DOWNLOAD_UPDATE_URL  220

#define ID_VIEW_DROPZONE        230
#define ID_VIEW_TOOLBAR         231
#define ID_VIEW_STATUSBAR       232
#define ID_VIEW_TREEVIEW        233

#define ID_TOOLS_OPTIONS        240
#define ID_TOOLS_CLIPBOARD      241
#define ID_TOOLS_SPEED_UNLIM    242
#define ID_TOOLS_SPEED_MANUAL   243
#define ID_TOOLS_SPEED_BACK     244

#define ID_HELP_ABOUT           250

// Tray & Drop Zone commands
#define ID_TRAY_RESTORE         260
#define ID_TRAY_PAUSEALL        261
#define ID_TRAY_RESUMEALL       262
#define ID_DROPZONE_TOPMOST     263
#define ID_DROPZONE_OPACITY50   264
#define ID_DROPZONE_OPACITY75   265
#define ID_DROPZONE_OPACITY100  266
#define ID_DROPZONE_HIDE        267

// Language command IDs
#define ID_LANG_BASE            500
#define ID_LANG_MAX             520

// Custom Win32 Messages
#define WM_APP_TRAYMSG          (WM_APP + 1)
#define WM_APP_TASK_UPDATE      (WM_APP + 2)
#define WM_APP_TASK_LOG         (WM_APP + 3)
#define WM_APP_TASK_FINISHED    (WM_APP + 4)
#define WM_APP_DROPZONE_DROP    (WM_APP + 5)
#define WM_APP_CLIPBOARD_URL    (WM_APP + 6)
