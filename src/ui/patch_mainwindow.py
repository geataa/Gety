import sys

with open(r'E:\0_SkySoft\Gety\src\ui\MainWindow.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# a) ShowTrayContextMenu
content = content.replace('AppendMenuW(hMenu, MF_STRING, 9501, L"Gety\'yi Aç");', 'AppendMenuW(hMenu, MF_STRING, ID_TRAY_RESTORE, LStr(StrId::CtxOpenGety));')
content = content.replace('if (cmd == 9501)', 'if (cmd == ID_TRAY_RESTORE)')

# b) ShowTaskContextMenu
content = content.replace('AppendMenuW(hMenu, MF_STRING, 9601, L"⏸ Durdur");', 'AppendMenuW(hMenu, MF_STRING, ID_DOWNLOAD_PAUSE, LStr(StrId::CtxPause));')
content = content.replace('AppendMenuW(hMenu, MF_STRING, 9602, L"▶ Başlat");', 'AppendMenuW(hMenu, MF_STRING, ID_DOWNLOAD_START, LStr(StrId::CtxStart));')
content = content.replace('AppendMenuW(hMenu, MF_STRING, 9603, L"📂 Dosyayı Aç");', 'AppendMenuW(hMenu, MF_STRING, ID_DOWNLOAD_OPENFILE, LStr(StrId::CtxOpenFile));')
content = content.replace('AppendMenuW(hMenu, MF_STRING, 9604, L"📁 Klasörü Aç");', 'AppendMenuW(hMenu, MF_STRING, ID_DOWNLOAD_OPENFOLDER, LStr(StrId::CtxOpenFolder));')
content = content.replace('AppendMenuW(hMenu, MF_STRING, 9605, L"📋 URL\'yi Kopyala");', 'AppendMenuW(hMenu, MF_STRING, ID_DOWNLOAD_PROPERTIES, LStr(StrId::CtxCopyUrl));')
content = content.replace('AppendMenuW(hMenu, MF_STRING, 9606, L"🗑 Listeden Sil");', 'AppendMenuW(hMenu, MF_STRING, ID_DOWNLOAD_DELETE, LStr(StrId::CtxDeleteTask));')

content = content.replace('if (cmd == 9601) {', 'if (cmd == ID_DOWNLOAD_PAUSE) {')
content = content.replace('} else if (cmd == 9602) {', '} else if (cmd == ID_DOWNLOAD_START) {')
content = content.replace('} else if (cmd == 9603) {', '} else if (cmd == ID_DOWNLOAD_OPENFILE) {')
content = content.replace('} else if (cmd == 9604) {', '} else if (cmd == ID_DOWNLOAD_OPENFOLDER) {')
content = content.replace('} else if (cmd == 9605) {', '} else if (cmd == ID_DOWNLOAD_PROPERTIES) {')
content = content.replace('} else if (cmd == 9606) {', '} else if (cmd == ID_DOWNLOAD_DELETE) {')

# c) ShowLanguageMenu
content = content.replace('2000 + (UINT)i', 'ID_LANG_BASE + (UINT)i')

# d) HandleCommand
content = content.replace('id >= 2000 && id < 2000 +', 'id >= ID_LANG_BASE && id < ID_LANG_BASE +')
content = content.replace('int langIdx = id - 2000;', 'int langIdx = id - ID_LANG_BASE;')

# e) HandleMessage additions
handlers = '''        case WM_APP_CLIPBOARD_URL: {
            wchar_t* pUrl = (wchar_t*)lParam;
            if (pUrl) {
                std::wstring url = pUrl;
                free(pUrl);
                std::wstring saveDir, category;
                int splitParts = Config::Instance().defaultSplitParts;
                bool startImmediately = true;
                if (Dialogs::ShowNewDownloadDialog(m_hwnd, url, url, saveDir, category, splitParts, startImmediately)) {
                    m_selectedTaskId = DownloadManager::Instance().AddTask(url, saveDir, category, splitParts, startImmediately);
                    Invalidate();
                }
            }
            return 0;
        }

        case WM_APP_DROPZONE_DROP: {
            wchar_t* pUrl = (wchar_t*)lParam;
            if (pUrl) {
                std::wstring url = pUrl;
                free(pUrl);
                std::wstring saveDir, category;
                int splitParts = Config::Instance().defaultSplitParts;
                bool startImmediately = true;
                if (Dialogs::ShowNewDownloadDialog(m_hwnd, url, url, saveDir, category, splitParts, startImmediately)) {
                    m_selectedTaskId = DownloadManager::Instance().AddTask(url, saveDir, category, splitParts, startImmediately);
                    Invalidate();
                }
            }
            return 0;
        }

'''
content = content.replace('        case WM_APP_TASK_UPDATE: {', handlers + '        case WM_APP_TASK_UPDATE: {')

with open(r'E:\0_SkySoft\Gety\src\ui\MainWindow.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print('MainWindow.cpp patched')
