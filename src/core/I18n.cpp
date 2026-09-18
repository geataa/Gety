#include "I18n.h"

namespace Gety {

I18n& I18n::Instance() {
    static I18n instance;
    return instance;
}

I18n::I18n() {
    m_languages = {
        { LangId::Turkish,    L"tr", L"Turkish",    L"Türkçe" },
        { LangId::English,    L"en", L"English",    L"English" },
        { LangId::German,     L"de", L"German",     L"Deutsch" },
        { LangId::French,     L"fr", L"French",     L"Français" },
        { LangId::Spanish,    L"es", L"Spanish",    L"Español" },
        { LangId::Russian,    L"ru", L"Russian",    L"Русский" },
        { LangId::Ukrainian,  L"uk", L"Ukrainian",  L"Українська" },
        { LangId::Greek,      L"el", L"Greek",      L"Ελληνικά" },
        { LangId::Italian,    L"it", L"Italian",    L"Italiano" },
        { LangId::Portuguese, L"pt", L"Portuguese", L"Português" },
        { LangId::Japanese,   L"ja", L"Japanese",   L"日本語" },
        { LangId::Chinese,    L"zh", L"Chinese",    L"简体中文" }
    };

    InitStrings();
}

void I18n::SetLanguage(LangId lang) {
    m_currentLang = lang;
}

void I18n::SetLanguageByCode(const std::wstring& code) {
    for (const auto& l : m_languages) {
        if (l.code == code) {
            m_currentLang = l.id;
            return;
        }
    }
    m_currentLang = LangId::English;
}

std::wstring I18n::GetCurrentLanguageCode() const {
    for (const auto& l : m_languages) {
        if (l.id == m_currentLang) {
            return l.code;
        }
    }
    return L"en";
}

const wchar_t* I18n::Get(StrId id) const {
    auto itLang = m_translations.find(m_currentLang);
    if (itLang != m_translations.end()) {
        auto itStr = itLang->second.find(id);
        if (itStr != itLang->second.end()) {
            return itStr->second.c_str();
        }
    }

    // Fallback to English
    auto itEn = m_translations.find(LangId::English);
    if (itEn != m_translations.end()) {
        auto itStr = itEn->second.find(id);
        if (itStr != itEn->second.end()) {
            return itStr->second.c_str();
        }
    }

    // Fallback to Turkish
    auto itTr = m_translations.find(LangId::Turkish);
    if (itTr != m_translations.end()) {
        auto itStr = itTr->second.find(id);
        if (itStr != itTr->second.end()) {
            return itStr->second.c_str();
        }
    }

    return L"";
}

void I18n::InitStrings() {
    // ---------------------------------------------------------
    // 1. TÜRKÇE (TR)
    // ---------------------------------------------------------
    auto& tr = m_translations[LangId::Turkish];
    tr[StrId::MenuFile]             = L"&Dosya";
    tr[StrId::MenuFileNew]          = L"&Yeni İndirme...\tCtrl+N";
    tr[StrId::MenuFileBatch]        = L"&Toplu İndirme...\tCtrl+B";
    tr[StrId::MenuFileOllamaModel]  = L"🤖 &Yapay Zeka / Ollama Modeli İndir...\tCtrl+M";
    tr[StrId::MenuFileHfModel]      = L"🤗 &Hugging Face Modeli İndir...\tCtrl+H";
    tr[StrId::MenuFileExit]         = L"Çı&kış\tAlt+F4";

    tr[StrId::MenuTask]             = L"&Görev";
    tr[StrId::MenuTaskStart]        = L"&Başlat\tF5";
    tr[StrId::MenuTaskPause]        = L"&Duraklat\tF6";
    tr[StrId::MenuTaskStop]         = L"D&urdur";
    tr[StrId::MenuTaskOpenFile]     = L"Dosyayı &Aç";
    tr[StrId::MenuTaskOpenFolder]   = L"Klasörü &Aç";
    tr[StrId::MenuTaskRedownload]   = L"&Yeniden İndir";
    tr[StrId::MenuTaskDelete]       = L"&Sil\tDel";
    tr[StrId::MenuTaskMoveUp]       = L"&Yukarı Taşı";
    tr[StrId::MenuTaskMoveDown]     = L"&Aşağı Taşı";

    tr[StrId::MenuView]             = L"&Görünüm";
    tr[StrId::MenuViewDropZone]     = L"&Kayan İndirme Hedefi (Drop Zone)";
    tr[StrId::MenuViewToolbar]      = L"&Araç Çubuğu";
    tr[StrId::MenuViewStatusbar]    = L"&Durum Çubuğu";
    tr[StrId::MenuViewTree]         = L"&Kategori Ağacı";

    tr[StrId::MenuTools]            = L"&Araçlar";
    tr[StrId::MenuToolsClipboard]   = L"&Pano İzleyici (Otomatik URL Yakala)";
    tr[StrId::MenuToolsSpeed]       = L"&Hız Modu";
    tr[StrId::MenuToolsSpeedUnlim]  = L"&Sınırsız (Maksimum Hız)";
    tr[StrId::MenuToolsSpeedManual] = L"&Manuel Limit";
    tr[StrId::MenuToolsSpeedBack]   = L"&Arka Plan Modu";
    tr[StrId::MenuToolsOptions]     = L"&Seçenekler (Ayarlar)...";

    tr[StrId::MenuLanguage]         = L"&Dil / Language";
    tr[StrId::MenuHelp]             = L"&Yardım";
    tr[StrId::MenuHelpAbout]        = L"&Gety Hakkında";

    tr[StrId::TbNew]                = L" Yeni";
    tr[StrId::TbStart]              = L" Başlat";
    tr[StrId::TbPause]              = L" Duraklat";
    tr[StrId::TbStop]               = L" Durdur";
    tr[StrId::TbDelete]             = L" Sil";
    tr[StrId::TbRedownload]         = L" Yeniden İndir";
    tr[StrId::TbDropZone]           = L" Hedef";
    tr[StrId::TbOptions]            = L" Ayarlar";
    tr[StrId::TbSpeedUnlim]         = L" Hız: Sınırsız";

    tr[StrId::CatAll]               = L"📂 Tüm Görevler";
    tr[StrId::CatUnfinished]        = L"⏳ Devam Edenler";
    tr[StrId::CatDownloading]       = L"⬇ İndiriliyor";
    tr[StrId::CatPaused]            = L"⏸️ Duraklatılanlar";
    tr[StrId::CatDownloaded]        = L"✅ İndirilenler";
    tr[StrId::CatMusic]             = L"🎵 Müzik";
    tr[StrId::CatVideo]             = L"🎬 Film & Video";
    tr[StrId::CatSoftware]          = L"💻 Yazılım";
    tr[StrId::CatGames]             = L"🎮 Oyun";
    tr[StrId::CatDocuments]         = L"📄 Belgeler";
    tr[StrId::CatArchives]          = L"📦 Arşivler";
    tr[StrId::CatAI]                = L"🤖 Yapay Zeka";
    tr[StrId::CatTrash]             = L"🗑️ Çöp Kutusu";
    tr[StrId::CatGeneral]           = L"Genel";

    tr[StrId::ColFilename]          = L"Dosya Adı";
    tr[StrId::ColSize]              = L"Boyut";
    tr[StrId::ColDownloaded]        = L"İndirilen";
    tr[StrId::ColProgress]          = L"İlerleme";
    tr[StrId::ColSpeed]             = L"Hız";
    tr[StrId::ColTimeLeft]          = L"Kalan Süre";
    tr[StrId::ColStatus]            = L"Durum";
    tr[StrId::ColParts]             = L"Parça";
    tr[StrId::ColUrl]               = L"URL";
    tr[StrId::ColDateAdded]         = L"Eklenme Tarihi";

    tr[StrId::TabLog]               = L"📜 Bağlantı Günlüğü (Log)";
    tr[StrId::TabGrid]              = L"🔲 Blok Grafiği";
    tr[StrId::TabInfo]              = L"ℹ️ Görev Detayları";
    tr[StrId::TabNoTask]            = L"Seçili görev yok veya dosya boyutu bekleniyor...";

    tr[StrId::StatusSpeed]          = L"⚡ Hız: ";
    tr[StrId::StatusTasks]          = L"Görevler: %d aktif, %d bitti, %d duraklatıldı";
    tr[StrId::StatusFreeSpace]      = L"💾 Boş Alan: ";
    tr[StrId::StatusDiskReady]      = L"💾 Disk Hazır";
    tr[StrId::StatusModeUnlim]      = L"Hız Modu: Sınırsız (Maksimum)";
    tr[StrId::StatusModeManual]     = L"Hız Modu: Manuel Limit (%d KB/s)";
    tr[StrId::StatusModeBack]       = L"Hız Modu: Arka Plan (%d KB/s)";

    tr[StrId::StateQueued]          = L"Sırada";
    tr[StrId::StateConnecting]      = L"Bağlanıyor...";
    tr[StrId::StateDownloading]     = L"İndiriliyor";
    tr[StrId::StatePaused]          = L"Duraklatıldı";
    tr[StrId::StateCompleted]       = L"Tamamlandı";
    tr[StrId::StateFailed]          = L"Hata";
    tr[StrId::StateDeleted]         = L"Silindi";
    tr[StrId::StateUnknown]         = L"Bilinmiyor";

    tr[StrId::DropZoneTitle]        = L"Gety";
    tr[StrId::DropZoneActive]       = L"%d aktif";
    tr[StrId::DropZoneShow]         = L"Hedefi Göster";
    tr[StrId::DropZoneHide]         = L"Hedefi &Gizle";
    tr[StrId::DropZoneOpacity]      = L"&Şeffaflık";
    tr[StrId::DropZoneOpacity50]    = L"%50 Şeffaf";
    tr[StrId::DropZoneOpacity75]    = L"%75 Şeffaf";
    tr[StrId::DropZoneOpacity100]   = L"%100 Opak";

    tr[StrId::TrayRestore]          = L"&Ana Pencereyi Göster";
    tr[StrId::TrayResumeAll]        = L"&Tümünü Başlat";
    tr[StrId::TrayPauseAll]         = L"&Tümünü Duraklat";
    tr[StrId::TrayBalloonTitle]     = L"Gety Çalışıyor";
    tr[StrId::TrayBalloonText]      = L"Gety arka planda indirmeye devam ediyor. Tamamen çıkmak için tepsi ikonuna sağ tıklayıp 'Çıkış'ı seçin.";

    tr[StrId::GridDone]             = L"Tamamlandı";
    tr[StrId::GridActive]           = L"İndiriliyor";
    tr[StrId::GridPending]          = L"Bekliyor";
    tr[StrId::GridStats]            = L"Parça: %d  |  İlerleme: %.1f%%  |  %s / %s";

    tr[StrId::DlgNewTitle]          = L"Yeni İndirme Görevi";
    tr[StrId::DlgNewUrl]            = L"URL (İndirme Bağlantısı):";
    tr[StrId::DlgNewFilename]       = L"Dosya Adı (Kaydedilecek İsim):";
    tr[StrId::DlgNewDir]            = L"Kayıt Klasörü:";
    tr[StrId::DlgNewBrowse]         = L"Gözat...";
    tr[StrId::DlgNewPaste]          = L"📋 Yapıştır";
    tr[StrId::DlgNewCat]            = L"Kategori:";
    tr[StrId::DlgNewParts]          = L"Parça Sayısı (Split):";
    tr[StrId::DlgNewStartImm]       = L"Görevi hemen başlat";
    tr[StrId::DlgOk]                = L"Tamam";
    tr[StrId::DlgCancel]            = L"İptal";
    tr[StrId::DlgClose]             = L"Kapat";
    tr[StrId::DlgSave]              = L"Kaydet";

    tr[StrId::DlgBatchTitle]        = L"Toplu İndirme (Batch Download)";
    tr[StrId::DlgBatchPattern]      = L"URL Deseni (Joker karakter için (*) kullanın):";
    tr[StrId::DlgBatchFrom]         = L"Başlangıç:";
    tr[StrId::DlgBatchTo]           = L"Bitiş:";
    tr[StrId::DlgBatchDigits]       = L"Basamak Sayısı:";
    tr[StrId::DlgBatchPreview]      = L"Önizle";
    tr[StrId::DlgBatchList]         = L"Oluşturulacak İndirme Listesi:";
    tr[StrId::DlgBatchAddAll]       = L"Tümünü Görevlere Ekle";

    tr[StrId::DlgOptTitle]          = L"Seçenekler & Tercihler";
    tr[StrId::DlgOptMaxTasks]       = L"Eşzamanlı Görev (1-10):";
    tr[StrId::DlgOptDefParts]       = L"Varsayılan Parça (1-30):";
    tr[StrId::DlgOptSpeedLimit]     = L"Manuel Hız Limiti (KB/s):";
    tr[StrId::DlgOptClip]           = L"Panoyu İzle (Kopyalanan URL'leri otomatik yakala)";
    tr[StrId::DlgOptSound]          = L"İndirme tamamlandığında sesli uyarı çal";
    tr[StrId::DlgOptDrop]           = L"Kayan İndirme Hedefini (Drop Zone) Göster";
    tr[StrId::DlgOptShutdown]       = L"Tüm indirmeler bittiğinde bilgisayarı kapat";
    tr[StrId::DlgOptMinToTray]      = L"Pencere kapatıldığında [X] arka planda çalış (Tepsiye küçült)";
    tr[StrId::DlgOptAutoStart]      = L"Windows başladığında otomatik çalıştır";
    tr[StrId::DlgOptLanguage]       = L"Uygulama Dili (Language):";

    tr[StrId::DlgDeletePrompt]      = L"Seçili görevi silmek istediğinize emin misiniz?\n\n(Dosyayı diskten de silmek için 'Evet'e, listeden kaldırmak için 'Hayır'a basın)";
    tr[StrId::DlgDeleteTitle]       = L"Görevi Sil";
    tr[StrId::DlgAboutTitle]        = L"Gety Hakkında";
    tr[StrId::DlgAboutDesc]         = L"⚡ Gety\nSürüm: 1.0 (C++20 Native Ultra-Fast Edition)\n\nModern C++ ile sıfırdan yazılmış %100 taşınabilir (portable) hızlı indirme yöneticisi.\n\nÖzellikler:\n• Çok parçalı (1-30 parça) eşzamanlı HTTP/HTTPS indirme\n• Renkli kare blok indirme matrisi (Matrix Visualizer)\n• Masaüstünde yüzen saydam indirme hedefi (Floating Drop Zone)\n• Otomatik pano URL yakalama (Clipboard Monitor)\n• Sıfır kurulum & sıfır Registry kirliliği (tamamen portable)\n• Token-bucket bant genişliği ve hız sınırlama modları\n• Toplu indirme (Batch URL)\n• 12 Dil desteği ve kusursuz Unicode";

    // ---------------------------------------------------------

    tr[StrId::ActNew]               = L"Yeni";
    tr[StrId::ActBatch]             = L"Toplu";
    tr[StrId::ActVideoLink]         = L"Link Ekle";
    tr[StrId::ActOllamaModel]       = L"Ollama";
    tr[StrId::ActHfModel]           = L"HuggingFace";
    tr[StrId::ActStart]             = L"Başlat";
    tr[StrId::ActPause]             = L"Durdur";
    tr[StrId::ActDelete]            = L"Sil";
    tr[StrId::ActSpeed]             = L"Hız";
    tr[StrId::ActLanguage]          = L"Dil";
    tr[StrId::ActSettings]          = L"Ayarla";
    tr[StrId::ActAbout]             = L"Hakkında";
    tr[StrId::CtxPause]             = L"⏸ Durdur";
    tr[StrId::CtxStart]             = L"▶ Başlat";
    tr[StrId::CtxOpenFile]          = L"📂 Dosyayı Aç";
    tr[StrId::CtxOpenFolder]        = L"📁 Klasörü Aç";
    tr[StrId::CtxCopyUrl]           = L"📋 URL'yi Kopyala";
    tr[StrId::CtxDeleteTask]        = L"🗑 Listeden Sil";
    tr[StrId::CtxOpenGety]          = L"Gety'yi Aç";
    tr[StrId::DlgOptSaveDir]        = L"Varsayılan Kayıt Klasörü:";
    tr[StrId::DlgOptMaxRetries]     = L"Yeniden Deneme (1-50):";
    tr[StrId::CtxUpdateUrl]         = L"🔗 Bağlantıyı Güncelle (URL)...";
    tr[StrId::DlgUpdateUrlTitle]    = L"İndirme Bağlantısını Güncelle";
    tr[StrId::DlgUpdateUrlOld]      = L"Mevcut (Eski) Bağlantı:";
    tr[StrId::DlgUpdateUrlNew]      = L"Yeni İndirme Bağlantısı (URL):";
    tr[StrId::DlgUpdateUrlBtn]      = L"Bağlantıyı Güncelle";

    tr[StrId::DlgVideoTitle]        = L"Video / Medya Linki Ekle";
    tr[StrId::DlgVideoUrlPrompt]    = L"Video veya Oynatma Listesi Bağlantısı (URL):";
    tr[StrId::DlgVideoAnalyze]      = L"Analiz Et";
    tr[StrId::DlgVideoAnalyzing]    = L"Medya analiz ediliyor, lütfen bekleyin...";
    tr[StrId::DlgVideoQuality]      = L"Kalite / Format Seçimi:";
    tr[StrId::DlgVideoPlaylistVideos]= L"İndirilecek Videolar (Oynatma Listesi):";
    tr[StrId::DlgVideoSelectAll]    = L"Tümünü Seç";
    tr[StrId::DlgVideoDeselectAll]  = L"Temizle";
    tr[StrId::DlgVideoAudioOnly]    = L"Sadece Ses İndir (MP3)";
    tr[StrId::DlgVideoDownload]     = L"İndirmeyi Başlat";
    tr[StrId::DlgVideoSaveDir]      = L"Kayıt Klasörü:";
    tr[StrId::DlgVideoBrowse]       = L"Gözat...";
    tr[StrId::DlgVideoFound]        = L"Video Bulundu";

    tr[StrId::DlgModelTabOllama]        = L"🤖 Ollama";
    tr[StrId::DlgModelTabHf]            = L"🤗 Hugging Face";

    tr[StrId::DlgOllamaTitle]           = L"Yapay Zeka Modeli İndir (Ollama)";
    tr[StrId::DlgOllamaUrlPrompt]       = L"Model Etiketi / Adı (örn: nomic-embed-text-v2-moe:latest, llama3.2:1b):";
    tr[StrId::DlgOllamaAnalyze]         = L"Modeli İncele";
    tr[StrId::DlgOllamaAnalyzing]       = L"Ollama kayıt sunucusuna bağlanılıyor, lütfen bekleyin...";
    tr[StrId::DlgOllamaModelInfo]       = L"Model Detayları & Katmanlar:";
    tr[StrId::DlgOllamaFormatGguf]      = L"GGUF Formatı (.gguf) - llama.cpp, LM Studio, Jan için tek dosya (Önerilen)";
    tr[StrId::DlgOllamaFormatOriginal]  = L"Orijinal Format - Tüm Katmanlar ve Modelfile Paketi (Klasör halinde)";
    tr[StrId::DlgOllamaFilename]        = L"Kayıt Dosya Adı / Klasör:";
    tr[StrId::DlgOllamaDownload]        = L"İndirmeyi Başlat";
    tr[StrId::DlgOllamaSaveDir]         = L"Kayıt Klasörü:";
    tr[StrId::DlgOllamaParts]           = L"Parça Sayısı (Split):";
    tr[StrId::DlgOllamaLayers]          = L"Katman Sayısı:";
    tr[StrId::DlgOllamaFound]           = L"Model Doğrulandı";

    tr[StrId::DlgHfTitle]               = L"Yapay Zeka Modeli İndir (Hugging Face)";
    tr[StrId::DlgHfRepoPrompt]          = L"Hugging Face Depo / URL (örn: TheBloke/Mistral-7B-Instruct-v0.2-GGUF):";
    tr[StrId::DlgHfTokenPrompt]         = L"HF Erişim Jetonu (hf_...) [İsteğe bağlı / Gated modeller için]:";
    tr[StrId::DlgHfAnalyze]             = L"Modeli İncele";
    tr[StrId::DlgHfAnalyzing]           = L"Hugging Face Hub API'sine bağlanılıyor, lütfen bekleyin...";
    tr[StrId::DlgHfModelInfo]           = L"Model Deposu ve Dosyaları:";
    tr[StrId::DlgHfQuantPrompt]         = L"İndirilecek Dosya / GGUF Nicelleştirmesi (Quantization):";
    tr[StrId::DlgHfFilename]            = L"Kayıt Dosya Adı:";
    tr[StrId::DlgHfDownload]            = L"İndirmeyi Başlat";
    tr[StrId::DlgHfSaveDir]             = L"Kayıt Klasörü:";
    tr[StrId::DlgHfParts]               = L"Parça Sayısı (Split):";
    tr[StrId::DlgHfFound]               = L"Model Ağacı Doğrulandı";

    // 2. ENGLISH (EN)
    // ---------------------------------------------------------
    auto& en = m_translations[LangId::English];
    en[StrId::MenuFile]             = L"&File";
    en[StrId::MenuFileNew]          = L"&New Download...\tCtrl+N";
    en[StrId::MenuFileBatch]        = L"&Batch Download...\tCtrl+B";
    en[StrId::MenuFileOllamaModel]  = L"🤖 &AI / Ollama Model Download...\tCtrl+M";
    en[StrId::MenuFileHfModel]      = L"🤗 &Hugging Face Model Download...\tCtrl+H";
    en[StrId::MenuFileExit]         = L"E&xit\tAlt+F4";

    en[StrId::MenuTask]             = L"&Task";
    en[StrId::MenuTaskStart]        = L"&Start\tF5";
    en[StrId::MenuTaskPause]        = L"&Pause\tF6";
    en[StrId::MenuTaskStop]         = L"S&top";
    en[StrId::MenuTaskOpenFile]     = L"&Open File";
    en[StrId::MenuTaskOpenFolder]   = L"Open &Folder";
    en[StrId::MenuTaskRedownload]   = L"&Redownload";
    en[StrId::MenuTaskDelete]       = L"&Delete\tDel";
    en[StrId::MenuTaskMoveUp]       = L"Move &Up";
    en[StrId::MenuTaskMoveDown]     = L"Move &Down";

    en[StrId::MenuView]             = L"&View";
    en[StrId::MenuViewDropZone]     = L"&Floating Drop Zone";
    en[StrId::MenuViewToolbar]      = L"&Toolbar";
    en[StrId::MenuViewStatusbar]    = L"&Status Bar";
    en[StrId::MenuViewTree]         = L"&Category Tree";

    en[StrId::MenuTools]            = L"&Tools";
    en[StrId::MenuToolsClipboard]   = L"&Clipboard Monitor";
    en[StrId::MenuToolsSpeed]       = L"&Speed Mode";
    en[StrId::MenuToolsSpeedUnlim]  = L"&Unlimited (Maximum Speed)";
    en[StrId::MenuToolsSpeedManual] = L"&Manual Limit";
    en[StrId::MenuToolsSpeedBack]   = L"&Background Mode";
    en[StrId::MenuToolsOptions]     = L"&Options (Preferences)...";

    en[StrId::MenuLanguage]         = L"&Language";
    en[StrId::MenuHelp]             = L"&Help";
    en[StrId::MenuHelpAbout]        = L"&About Gety";

    en[StrId::TbNew]                = L" New";
    en[StrId::TbStart]              = L" Start";
    en[StrId::TbPause]              = L" Pause";
    en[StrId::TbStop]               = L" Stop";
    en[StrId::TbDelete]             = L" Delete";
    en[StrId::TbRedownload]         = L" Redownload";
    en[StrId::TbDropZone]           = L" Drop Zone";
    en[StrId::TbOptions]            = L" Options";
    en[StrId::TbSpeedUnlim]         = L" Speed: Unlimited";

    en[StrId::CatAll]               = L"📂 All Tasks";
    en[StrId::CatUnfinished]        = L"⏳ Incomplete";
    en[StrId::CatDownloading]       = L"⬇ Downloading";
    en[StrId::CatPaused]            = L"⏸️ Paused";
    en[StrId::CatDownloaded]        = L"✅ Downloaded";
    en[StrId::CatMusic]             = L"🎵 Music";
    en[StrId::CatVideo]             = L"🎬 Movies & Video";
    en[StrId::CatSoftware]          = L"💻 Software";
    en[StrId::CatGames]             = L"🎮 Games";
    en[StrId::CatDocuments]         = L"📄 Documents";
    en[StrId::CatArchives]          = L"📦 Archives";
    en[StrId::CatAI]                = L"🤖 AI Models";
    en[StrId::CatTrash]             = L"🗑️ Trash";
    en[StrId::CatGeneral]           = L"General";

    en[StrId::ColFilename]          = L"File Name";
    en[StrId::ColSize]              = L"Size";
    en[StrId::ColDownloaded]        = L"Downloaded";
    en[StrId::ColProgress]          = L"Progress";
    en[StrId::ColSpeed]             = L"Speed";
    en[StrId::ColTimeLeft]          = L"Time Left";
    en[StrId::ColStatus]            = L"Status";
    en[StrId::ColParts]             = L"Parts";
    en[StrId::ColUrl]               = L"URL";
    en[StrId::ColDateAdded]         = L"Date Added";

    en[StrId::TabLog]               = L"📜 Connection Log";
    en[StrId::TabGrid]              = L"🔲 Chunk Grid";
    en[StrId::TabInfo]              = L"ℹ️ Task Details";
    en[StrId::TabNoTask]            = L"No task selected or waiting for file size...";

    en[StrId::StatusSpeed]          = L"⚡ Speed: ";
    en[StrId::StatusTasks]          = L"Tasks: %d active, %d completed, %d paused";
    en[StrId::StatusFreeSpace]      = L"💾 Free Space: ";
    en[StrId::StatusDiskReady]      = L"💾 Disk Ready";
    en[StrId::StatusModeUnlim]      = L"Speed Mode: Unlimited";
    en[StrId::StatusModeManual]     = L"Speed Mode: Manual Limit (%d KB/s)";
    en[StrId::StatusModeBack]       = L"Speed Mode: Background (%d KB/s)";

    en[StrId::StateQueued]          = L"Queued";
    en[StrId::StateConnecting]      = L"Connecting...";
    en[StrId::StateDownloading]     = L"Downloading";
    en[StrId::StatePaused]          = L"Paused";
    en[StrId::StateCompleted]       = L"Completed";
    en[StrId::StateFailed]          = L"Failed";
    en[StrId::StateDeleted]         = L"Deleted";
    en[StrId::StateUnknown]         = L"Unknown";

    en[StrId::DropZoneTitle]        = L"Gety";
    en[StrId::DropZoneActive]       = L"%d active";
    en[StrId::DropZoneShow]         = L"Show Drop Zone";
    en[StrId::DropZoneHide]         = L"&Hide Drop Zone";
    en[StrId::DropZoneOpacity]      = L"&Opacity";
    en[StrId::DropZoneOpacity50]    = L"50% Opacity";
    en[StrId::DropZoneOpacity75]    = L"75% Opacity";
    en[StrId::DropZoneOpacity100]   = L"100% Opaque";

    en[StrId::TrayRestore]          = L"&Show Main Window";
    en[StrId::TrayResumeAll]        = L"&Resume All";
    en[StrId::TrayPauseAll]         = L"&Pause All";
    en[StrId::TrayBalloonTitle]     = L"Gety Running";
    en[StrId::TrayBalloonText]      = L"Gety is still downloading in the background. Right-click the tray icon and choose 'Exit' to quit.";

    en[StrId::GridDone]             = L"Completed";
    en[StrId::GridActive]           = L"Downloading";
    en[StrId::GridPending]          = L"Pending";
    en[StrId::GridStats]            = L"Parts: %d  |  Progress: %.1f%%  |  %s / %s";

    en[StrId::DlgNewTitle]          = L"New Download Task";
    en[StrId::DlgNewUrl]            = L"URL (Download Link):";
    en[StrId::DlgNewFilename]       = L"File Name (Save As):";
    en[StrId::DlgNewDir]            = L"Save Folder:";
    en[StrId::DlgNewBrowse]         = L"Browse...";
    en[StrId::DlgNewPaste]          = L"📋 Paste";
    en[StrId::DlgNewCat]            = L"Category:";
    en[StrId::DlgNewParts]          = L"Split Parts:";
    en[StrId::DlgNewStartImm]       = L"Start immediately";
    en[StrId::DlgOk]                = L"OK";
    en[StrId::DlgCancel]            = L"Cancel";
    en[StrId::DlgClose]             = L"Close";
    en[StrId::DlgSave]              = L"Save";

    en[StrId::DlgBatchTitle]        = L"Batch Download";
    en[StrId::DlgBatchPattern]      = L"URL Pattern (Use (*) for wildcard):";
    en[StrId::DlgBatchFrom]         = L"From:";
    en[StrId::DlgBatchTo]           = L"To:";
    en[StrId::DlgBatchDigits]       = L"Digits:";
    en[StrId::DlgBatchPreview]      = L"Preview";
    en[StrId::DlgBatchList]         = L"Generated Download List:";
    en[StrId::DlgBatchAddAll]       = L"Add All to Tasks";

    en[StrId::DlgOptTitle]          = L"Options & Preferences";
    en[StrId::DlgOptMaxTasks]       = L"Concurrent Downloads (1 - 10):";
    en[StrId::DlgOptDefParts]       = L"Default Split Parts (1 - 30):";
    en[StrId::DlgOptSpeedLimit]     = L"Manual Speed Limit (KB/s):";
    en[StrId::DlgOptClip]           = L"Monitor Clipboard (Auto-capture URLs)";
    en[StrId::DlgOptSound]          = L"Play notification sound when finished";
    en[StrId::DlgOptDrop]           = L"Show Floating Drop Zone";
    en[StrId::DlgOptShutdown]       = L"Shutdown computer when all downloads finish";
    en[StrId::DlgOptMinToTray]      = L"Minimize to tray on close [X] (Run in background)";
    en[StrId::DlgOptAutoStart]      = L"Start automatically with Windows";
    en[StrId::DlgOptLanguage]       = L"Interface Language:";

    en[StrId::DlgDeletePrompt]      = L"Are you sure you want to delete this task?\n\n(Click 'Yes' to delete from disk as well, or 'No' to remove from list only)";
    en[StrId::DlgDeleteTitle]       = L"Delete Task";
    en[StrId::DlgAboutTitle]        = L"About Gety";
    en[StrId::DlgAboutDesc]         = L"⚡ Gety\nVersion: 1.0 (C++20 Native Ultra-Fast Edition)\n\nA 100% portable fast download manager written from scratch in modern C++.\n\nFeatures:\n• Multi-segmented (1-30 parts) accelerated downloading\n• Chunk matrix grid visualizer\n• Floating desktop drop zone\n• Clipboard monitor\n• Zero registry pollution / USB-ready\n• Token-bucket speed throttling modes\n• Batch URL expansion\n• 12 Languages with complete Unicode support";

    // ---------------------------------------------------------

    en[StrId::ActNew]               = L"New";
    en[StrId::ActBatch]             = L"Batch";
    en[StrId::ActVideoLink]         = L"Add Link";
    en[StrId::ActOllamaModel]       = L"Ollama";
    en[StrId::ActHfModel]           = L"HuggingFace";
    en[StrId::ActStart]             = L"Start";
    en[StrId::ActPause]             = L"Pause";
    en[StrId::ActDelete]            = L"Delete";
    en[StrId::ActSpeed]             = L"Speed";
    en[StrId::ActLanguage]          = L"Lang";
    en[StrId::ActSettings]          = L"Settings";
    en[StrId::ActAbout]             = L"About";
    en[StrId::CtxPause]             = L"⏸ Pause";
    en[StrId::CtxStart]             = L"▶ Start";
    en[StrId::CtxOpenFile]          = L"📂 Open File";
    en[StrId::CtxOpenFolder]        = L"📁 Open Folder";
    en[StrId::CtxCopyUrl]           = L"📋 Copy URL";
    en[StrId::CtxDeleteTask]        = L"🗑 Remove";
    en[StrId::CtxOpenGety]          = L"Open Gety";
    en[StrId::DlgOptSaveDir]        = L"Default Save Folder:";
    en[StrId::DlgOptMaxRetries]     = L"Retry Attempts on Error (1-50):";
    en[StrId::CtxUpdateUrl]         = L"🔗 Update Download URL...";
    en[StrId::DlgUpdateUrlTitle]    = L"Update Download URL";
    en[StrId::DlgUpdateUrlOld]      = L"Current (Old) URL:";
    en[StrId::DlgUpdateUrlNew]      = L"New Download URL:";
    en[StrId::DlgUpdateUrlBtn]      = L"Update URL";

    en[StrId::DlgVideoTitle]        = L"Add Video / Media Link";
    en[StrId::DlgVideoUrlPrompt]    = L"Video or Playlist URL:";
    en[StrId::DlgVideoAnalyze]      = L"Analyze";
    en[StrId::DlgVideoAnalyzing]    = L"Analyzing media, please wait...";
    en[StrId::DlgVideoQuality]      = L"Select Quality / Format:";
    en[StrId::DlgVideoPlaylistVideos]= L"Videos to Download (Playlist):";
    en[StrId::DlgVideoSelectAll]    = L"Select All";
    en[StrId::DlgVideoDeselectAll]  = L"Deselect All";
    en[StrId::DlgVideoAudioOnly]    = L"Audio Only (MP3)";
    en[StrId::DlgVideoDownload]     = L"Start Download";
    en[StrId::DlgVideoSaveDir]      = L"Save Folder:";
    en[StrId::DlgVideoBrowse]       = L"Browse...";
    en[StrId::DlgVideoFound]        = L"Video Found";

    en[StrId::DlgModelTabOllama]        = L"🤖 Ollama";
    en[StrId::DlgModelTabHf]            = L"🤗 Hugging Face";

    en[StrId::DlgOllamaTitle]           = L"AI Model Downloader (Ollama)";
    en[StrId::DlgOllamaUrlPrompt]       = L"Model Tag or Name (e.g. nomic-embed-text-v2-moe:latest, llama3.2:1b):";
    en[StrId::DlgOllamaAnalyze]         = L"Inspect Model";
    en[StrId::DlgOllamaAnalyzing]       = L"Connecting to Ollama registry, please wait...";
    en[StrId::DlgOllamaModelInfo]       = L"Model Details & Layers:";
    en[StrId::DlgOllamaFormatGguf]      = L"GGUF Format (.gguf) - Single file for llama.cpp, LM Studio, Jan (Recommended)";
    en[StrId::DlgOllamaFormatOriginal]  = L"Original Format - All Layers and Modelfile Package (in folder)";
    en[StrId::DlgOllamaFilename]        = L"Target Filename / Folder:";
    en[StrId::DlgOllamaDownload]        = L"Start Download";
    en[StrId::DlgOllamaSaveDir]         = L"Save Directory:";
    en[StrId::DlgOllamaParts]           = L"Split Parts:";
    en[StrId::DlgOllamaLayers]          = L"Layers Count:";
    en[StrId::DlgOllamaFound]           = L"Model Verified";

    en[StrId::DlgHfTitle]               = L"AI Model Downloader (Hugging Face)";
    en[StrId::DlgHfRepoPrompt]          = L"Hugging Face Repo / URL (e.g. TheBloke/Mistral-7B-Instruct-v0.2-GGUF):";
    en[StrId::DlgHfTokenPrompt]         = L"HF Access Token (hf_...) [Optional / For gated models]:";
    en[StrId::DlgHfAnalyze]             = L"Inspect Model";
    en[StrId::DlgHfAnalyzing]           = L"Connecting to Hugging Face Hub API, please wait...";
    en[StrId::DlgHfModelInfo]           = L"Model Repository & Files:";
    en[StrId::DlgHfQuantPrompt]         = L"Target File / GGUF Quantization:";
    en[StrId::DlgHfFilename]            = L"Target Filename:";
    en[StrId::DlgHfDownload]            = L"Start Download";
    en[StrId::DlgHfSaveDir]             = L"Save Directory:";
    en[StrId::DlgHfParts]               = L"Split Parts:";
    en[StrId::DlgHfFound]               = L"Model Tree Verified";

    // 3. DEUTSCH (DE)
    // ---------------------------------------------------------
    auto& de = m_translations[LangId::German];
    de[StrId::MenuFile]             = L"&Datei";
    de[StrId::MenuFileNew]          = L"&Neuer Download...\tStrg+N";
    de[StrId::MenuFileBatch]        = L"&Stapel-Download...\tStrg+B";
    de[StrId::MenuFileExit]         = L"&Beenden\tAlt+F4";
    de[StrId::MenuTask]             = L"&Aufgabe";
    de[StrId::MenuTaskStart]        = L"&Starten\tF5";
    de[StrId::MenuTaskPause]        = L"&Pause\tF6";
    de[StrId::MenuTaskStop]         = L"S&topp";
    de[StrId::MenuTaskOpenFile]     = L"Datei &öffnen";
    de[StrId::MenuTaskOpenFolder]   = L"Ordner ö&ffnen";
    de[StrId::MenuTaskRedownload]   = L"&Erneut herunterladen";
    de[StrId::MenuTaskDelete]       = L"&Löschen\tEntf";
    de[StrId::MenuView]             = L"&Ansicht";
    de[StrId::MenuViewDropZone]     = L"&Schwebendes Drop-Ziel";
    de[StrId::MenuTools]            = L"&Extras";
    de[StrId::MenuToolsClipboard]   = L"&Zwischenablage überwachen";
    de[StrId::MenuToolsOptions]     = L"&Optionen (Einstellungen)...";
    de[StrId::MenuLanguage]         = L"&Sprache / Language";
    de[StrId::MenuHelp]             = L"&Hilfe";
    de[StrId::MenuHelpAbout]        = L"&Über Gety";
    de[StrId::TbNew]                = L" Neu";
    de[StrId::TbStart]              = L" Start";
    de[StrId::TbPause]              = L" Pause";
    de[StrId::TbStop]               = L" Stopp";
    de[StrId::TbDelete]             = L" Löschen";
    de[StrId::CatAll]               = L"📂 Alle Aufgaben";
    de[StrId::CatDownloading]       = L"⏳ Wird geladen";
    de[StrId::CatPaused]            = L"⏸️ Pausiert";
    de[StrId::CatDownloaded]        = L"✅ Abgeschlossen";
    de[StrId::ColFilename]          = L"Dateiname";
    de[StrId::ColSize]              = L"Größe";
    de[StrId::ColProgress]          = L"Fortschritt";
    de[StrId::ColSpeed]             = L"Geschwindigkeit";
    de[StrId::DlgOk]                = L"OK";
    de[StrId::DlgCancel]            = L"Abbrechen";
    de[StrId::TrayBalloonText]      = L"Gety läuft im Hintergrund weiter.";

    // ---------------------------------------------------------

    de[StrId::ActNew]               = L"Neu";
    de[StrId::ActBatch]             = L"Stapel";
    de[StrId::ActStart]             = L"Start";
    de[StrId::ActPause]             = L"Pause";
    de[StrId::ActDelete]            = L"Löschen";
    de[StrId::ActSpeed]             = L"Tempo";
    de[StrId::ActLanguage]          = L"Sprache";
    de[StrId::ActSettings]          = L"Einst.";
    de[StrId::ActAbout]             = L"Über";
    de[StrId::CtxPause]             = L"⏸ Pause";
    de[StrId::CtxStart]             = L"▶ Starten";
    de[StrId::CtxOpenFile]          = L"📂 Datei öffnen";
    de[StrId::CtxOpenFolder]        = L"📁 Ordner öffnen";
    de[StrId::CtxCopyUrl]           = L"📋 URL kopieren";
    de[StrId::CtxDeleteTask]        = L"🗑 Entfernen";
    de[StrId::CtxOpenGety]          = L"Gety öffnen";
    de[StrId::DlgOptSaveDir]        = L"Standard-Speicherordner:";

    // 4. ΕΛΛΗΝΙΚΑ (EL - GREEK)
    // ---------------------------------------------------------
    auto& el = m_translations[LangId::Greek];
    el[StrId::MenuFile]             = L"&Αρχείο";
    el[StrId::MenuFileNew]          = L"&Νέα Λήψη...\tCtrl+N";
    el[StrId::MenuFileBatch]        = L"&Μαζική Λήψη...\tCtrl+B";
    el[StrId::MenuFileExit]         = L"Έ&ξοδος\tAlt+F4";
    el[StrId::MenuTask]             = L"&Εργασία";
    el[StrId::MenuTaskStart]        = L"&Έναρξη\tF5";
    el[StrId::MenuTaskPause]        = L"&Παύση\tF6";
    el[StrId::MenuTaskStop]         = L"Δ&ιακοπή";
    el[StrId::MenuTaskOpenFile]     = L"&Άνοιγμα Αρχείου";
    el[StrId::MenuTaskOpenFolder]   = L"Άνοιγμα &Φακέλου";
    el[StrId::MenuTaskRedownload]   = L"&Επαναλήψη";
    el[StrId::MenuTaskDelete]       = L"&Διαγραφή\tDel";
    el[StrId::MenuView]             = L"&Προβολή";
    el[StrId::MenuViewDropZone]     = L"&Πλωτός Στόχος (Drop Zone)";
    el[StrId::MenuTools]            = L"Ε&ργαλεία";
    el[StrId::MenuToolsClipboard]   = L"&Παρακολούθηση Προχείρου";
    el[StrId::MenuToolsOptions]     = L"&Επιλογές (Ρυθμίσεις)...";
    el[StrId::MenuLanguage]         = L"&Γλώσσα / Language";
    el[StrId::MenuHelp]             = L"&Βοήθεια";
    el[StrId::MenuHelpAbout]        = L"&Σχετικά με το Gety";
    el[StrId::TbNew]                = L" Νέο";
    el[StrId::TbStart]              = L" Έναρξη";
    el[StrId::TbPause]              = L" Παύση";
    el[StrId::TbStop]               = L" Διακοπή";
    el[StrId::TbDelete]             = L" Διαγραφή";
    el[StrId::CatAll]               = L"📂 Όλες οι Εργασίες";
    el[StrId::CatDownloading]       = L"⏳ Λήψη σε εξέλιξη";
    el[StrId::CatPaused]            = L"⏸️ Σε παύση";
    el[StrId::CatDownloaded]        = L"✅ Ολοκληρωμένα";
    el[StrId::ColFilename]          = L"Όνομα Αρχείου";
    el[StrId::ColSize]              = L"Μέγεθος";
    el[StrId::ColProgress]          = L"Πρόοδος";
    el[StrId::ColSpeed]             = L"Ταχύτητα";
    el[StrId::ColStatus]            = L"Κατάσταση";
    el[StrId::DlgOk]                = L"Εντάξει";
    el[StrId::DlgCancel]            = L"Άκυρο";
    el[StrId::StateCompleted]       = L"Ολοκληρώθηκε";
    el[StrId::TrayBalloonText]      = L"Το Gety συνεχίζει να εκτελείται στο παρασκήνιο.";

    // ---------------------------------------------------------

    el[StrId::ActNew]               = L"Νέο";
    el[StrId::ActBatch]             = L"Μαζική";
    el[StrId::ActStart]             = L"Έναρξη";
    el[StrId::ActPause]             = L"Παύση";
    el[StrId::ActDelete]            = L"Διαγραφή";
    el[StrId::ActSpeed]             = L"Ταχύτητα";
    el[StrId::ActLanguage]          = L"Γλώσσα";
    el[StrId::ActSettings]          = L"Ρυθμίσεις";
    el[StrId::ActAbout]             = L"Σχετικά";
    el[StrId::CtxPause]             = L"⏸ Παύση";
    el[StrId::CtxStart]             = L"▶ Έναρξη";
    el[StrId::CtxOpenFile]          = L"📂 Άνοιγμα Αρχείου";
    el[StrId::CtxOpenFolder]        = L"📁 Άνοιγμα Φακέλου";
    el[StrId::CtxCopyUrl]           = L"📋 Αντιγραφή URL";
    el[StrId::CtxDeleteTask]        = L"🗑 Αφαίρεση";
    el[StrId::CtxOpenGety]          = L"Άνοιγμα Gety";
    el[StrId::DlgOptSaveDir]        = L"Προεπιλεγμένος Φάκελος:";

    // 5. РУССКИЙ (RU)
    // ---------------------------------------------------------
    auto& ru = m_translations[LangId::Russian];
    ru[StrId::MenuFile]             = L"&Файл";
    ru[StrId::MenuFileNew]          = L"&Новая закачка...\tCtrl+N";
    ru[StrId::MenuFileBatch]        = L"&Пакетная закачка...\tCtrl+B";
    ru[StrId::MenuFileExit]         = L"В&ыход\tAlt+F4";
    ru[StrId::MenuTask]             = L"&Задачи";
    ru[StrId::MenuTaskStart]        = L"&Старт\tF5";
    ru[StrId::MenuTaskPause]        = L"&Пауза\tF6";
    ru[StrId::MenuTaskStop]         = L"С&топ";
    ru[StrId::MenuTaskOpenFile]     = L"&Открыть файл";
    ru[StrId::MenuTaskOpenFolder]   = L"Открыть &папку";
    ru[StrId::MenuTaskDelete]       = L"&Удалить\tDel";
    ru[StrId::MenuView]             = L"&Вид";
    ru[StrId::MenuViewDropZone]     = L"&Плавающее окно (Drop Zone)";
    ru[StrId::MenuTools]            = L"&Инструменты";
    ru[StrId::MenuToolsClipboard]   = L"&Следить за буфером обмена";
    ru[StrId::MenuToolsOptions]     = L"&Настройки...";
    ru[StrId::MenuLanguage]         = L"&Язык / Language";
    ru[StrId::MenuHelp]             = L"&Справка";
    ru[StrId::MenuHelpAbout]        = L"&О программе Gety";
    ru[StrId::TbNew]                = L" Новая";
    ru[StrId::TbStart]              = L" Старт";
    ru[StrId::TbPause]              = L" Пауза";
    ru[StrId::TbStop]               = L" Стоп";
    ru[StrId::TbDelete]             = L" Удалить";
    ru[StrId::CatAll]               = L"📂 Все закачки";
    ru[StrId::CatDownloading]       = L"⏳ Загружаются";
    ru[StrId::CatPaused]            = L"⏸️ На паузе";
    ru[StrId::CatDownloaded]        = L"✅ Завершенные";
    ru[StrId::ColFilename]          = L"Имя файла";
    ru[StrId::ColSize]              = L"Размер";
    ru[StrId::ColProgress]          = L"Прогресс";
    ru[StrId::ColSpeed]             = L"Скорость";
    ru[StrId::DlgOk]                = L"OK";
    ru[StrId::DlgCancel]            = L"Отмена";
    ru[StrId::TrayBalloonText]      = L"Gety продолжает работать в фоновом режиме.";

    // ---------------------------------------------------------

    ru[StrId::ActNew]               = L"Новая";
    ru[StrId::ActBatch]             = L"Пакет";
    ru[StrId::ActStart]             = L"Старт";
    ru[StrId::ActPause]             = L"Пауза";
    ru[StrId::ActDelete]            = L"Удалить";
    ru[StrId::ActSpeed]             = L"Скорость";
    ru[StrId::ActLanguage]          = L"Язык";
    ru[StrId::ActSettings]          = L"Настр.";
    ru[StrId::ActAbout]             = L"О прог.";
    ru[StrId::CtxPause]             = L"⏸ Пауза";
    ru[StrId::CtxStart]             = L"▶ Старт";
    ru[StrId::CtxOpenFile]          = L"📂 Открыть файл";
    ru[StrId::CtxOpenFolder]        = L"📁 Открыть папку";
    ru[StrId::CtxCopyUrl]           = L"📋 Копировать URL";
    ru[StrId::CtxDeleteTask]        = L"🗑 Удалить";
    ru[StrId::CtxOpenGety]          = L"Открыть Gety";
    ru[StrId::DlgOptSaveDir]        = L"Папка сохранения:";

    // ---------------------------------------------------------
    // УКРАЇНСЬКА (UK - UKRAINIAN)
    // ---------------------------------------------------------
    auto& uk = m_translations[LangId::Ukrainian];
    uk[StrId::MenuFile]             = L"&Файл";
    uk[StrId::MenuFileNew]          = L"&Нове завантаження...\tCtrl+N";
    uk[StrId::MenuFileBatch]        = L"&Пакетне завантаження...\tCtrl+B";
    uk[StrId::MenuFileExit]         = L"В&ихід\tAlt+F4";

    uk[StrId::MenuTask]             = L"&Завдання";
    uk[StrId::MenuTaskStart]        = L"&Запустити\tF5";
    uk[StrId::MenuTaskPause]        = L"&Пауза\tF6";
    uk[StrId::MenuTaskStop]         = L"З&упинити";
    uk[StrId::MenuTaskOpenFile]     = L"&Відкрити файл";
    uk[StrId::MenuTaskOpenFolder]   = L"Відкрити &папку";
    uk[StrId::MenuTaskRedownload]   = L"&Перезавантажити";
    uk[StrId::MenuTaskDelete]       = L"&Видалити\tDel";
    uk[StrId::MenuTaskMoveUp]       = L"Перемістити в&гору";
    uk[StrId::MenuTaskMoveDown]     = L"Перемістити в&низ";

    uk[StrId::MenuView]             = L"&Вигляд";
    uk[StrId::MenuViewDropZone]     = L"&Плаваюче вікно (Drop Zone)";
    uk[StrId::MenuViewToolbar]      = L"&Панель інструментів";
    uk[StrId::MenuViewStatusbar]    = L"&Рядок стану";
    uk[StrId::MenuViewTree]         = L"&Дерево категорій";

    uk[StrId::MenuTools]            = L"&Інструменти";
    uk[StrId::MenuToolsClipboard]   = L"&Монітор буфера обміну";
    uk[StrId::MenuToolsSpeed]       = L"&Режим швидкості";
    uk[StrId::MenuToolsSpeedUnlim]  = L"&Необмежено (Максимум)";
    uk[StrId::MenuToolsSpeedManual] = L"&Власний ліміт";
    uk[StrId::MenuToolsSpeedBack]   = L"&Фоновий режим";
    uk[StrId::MenuToolsOptions]     = L"&Налаштування...";

    uk[StrId::MenuLanguage]         = L"&Мова / Language";
    uk[StrId::MenuHelp]             = L"&Довідка";
    uk[StrId::MenuHelpAbout]        = L"&Про програму Gety";

    uk[StrId::TbNew]                = L" Створити";
    uk[StrId::TbStart]              = L" Старт";
    uk[StrId::TbPause]              = L" Пауза";
    uk[StrId::TbStop]               = L" Стоп";
    uk[StrId::TbDelete]             = L" Видалити";
    uk[StrId::TbRedownload]         = L" Перезавантажити";
    uk[StrId::TbDropZone]           = L" Вікно";
    uk[StrId::TbOptions]            = L" Налаштування";
    uk[StrId::TbSpeedUnlim]         = L" Швидкість: Необмежено";

    uk[StrId::CatAll]               = L"📂 Усі завдання";
    uk[StrId::CatUnfinished]        = L"⏳ Незавершені";
    uk[StrId::CatDownloading]       = L"⬇ Завантажуються";
    uk[StrId::CatPaused]            = L"⏸️ На паузі";
    uk[StrId::CatDownloaded]        = L"✅ Завершені";
    uk[StrId::CatMusic]             = L"🎵 Музика";
    uk[StrId::CatVideo]             = L"🎬 Відео";
    uk[StrId::CatSoftware]          = L"💻 Програми";
    uk[StrId::CatGames]             = L"🎮 Ігри";
    uk[StrId::CatDocuments]         = L"📄 Документи";
    uk[StrId::CatArchives]          = L"📦 Архіви";
    uk[StrId::CatTrash]             = L"🗑️ Кошик";
    uk[StrId::CatGeneral]           = L"Загальні";

    uk[StrId::ColFilename]          = L"Назва файлу";
    uk[StrId::ColSize]              = L"Розмір";
    uk[StrId::ColDownloaded]        = L"Завантажено";
    uk[StrId::ColProgress]          = L"Прогрес";
    uk[StrId::ColSpeed]             = L"Швидкість";
    uk[StrId::ColTimeLeft]          = L"Залишилося";
    uk[StrId::ColStatus]            = L"Статус";
    uk[StrId::ColParts]             = L"Частин";
    uk[StrId::ColUrl]               = L"URL";
    uk[StrId::ColDateAdded]         = L"Дата додавання";

    uk[StrId::TabLog]               = L"📜 Журнал з'єднання";
    uk[StrId::TabGrid]              = L"🔲 Графік сегментів";
    uk[StrId::TabInfo]              = L"ℹ️ Інформація про завдання";
    uk[StrId::TabNoTask]            = L"Немає вибраного завдання або очікується розмір...";

    uk[StrId::StatusSpeed]          = L"⚡ Швидкість: ";
    uk[StrId::StatusTasks]          = L"Завдання: %d активних, %d завершено, %d на паузі";
    uk[StrId::StatusFreeSpace]      = L"💾 Вільне місце: ";
    uk[StrId::StatusDiskReady]      = L"💾 Диск готовий";
    uk[StrId::StatusModeUnlim]      = L"Режим швидкості: Необмежено";
    uk[StrId::StatusModeManual]     = L"Режим швидкості: Ліміт (%d КБ/с)";
    uk[StrId::StatusModeBack]       = L"Режим швидкості: Фоновий (%d КБ/с)";

    uk[StrId::StateQueued]          = L"У черзі";
    uk[StrId::StateConnecting]      = L"З'єднання...";
    uk[StrId::StateDownloading]     = L"Завантаження";
    uk[StrId::StatePaused]          = L"Призупинено";
    uk[StrId::StateCompleted]       = L"Завершено";
    uk[StrId::StateFailed]          = L"Помилка";
    uk[StrId::StateDeleted]         = L"Видалено";
    uk[StrId::StateUnknown]         = L"Невідомо";

    uk[StrId::DropZoneTitle]        = L"Gety";
    uk[StrId::DropZoneActive]       = L"%d активних";
    uk[StrId::DropZoneShow]         = L"Показати плаваюче вікно";
    uk[StrId::DropZoneHide]         = L"&Приховати плаваюче вікно";
    uk[StrId::DropZoneOpacity]      = L"&Прозорість";
    uk[StrId::DropZoneOpacity50]    = L"50% прозорість";
    uk[StrId::DropZoneOpacity75]    = L"75% прозорість";
    uk[StrId::DropZoneOpacity100]   = L"100% непрозорий";

    uk[StrId::TrayRestore]          = L"&Показати головне вікно";
    uk[StrId::TrayResumeAll]        = L"&Запустити всі";
    uk[StrId::TrayPauseAll]         = L"&Призупинити всі";
    uk[StrId::TrayBalloonTitle]     = L"Gety працює";
    uk[StrId::TrayBalloonText]      = L"Gety продовжує завантаження у фоновому режимі.";

    uk[StrId::GridDone]             = L"Завершено";
    uk[StrId::GridActive]           = L"Завантаження";
    uk[StrId::GridPending]          = L"Очікування";
    uk[StrId::GridStats]            = L"Частини: %d  |  Прогрес: %.1f%%  |  %s / %s";

    uk[StrId::DlgNewTitle]          = L"Нове завдання завантаження";
    uk[StrId::DlgNewUrl]            = L"URL (Посилання):";
    uk[StrId::DlgNewFilename]       = L"Назва файлу:";
    uk[StrId::DlgNewDir]            = L"Папка збереження:";
    uk[StrId::DlgNewBrowse]         = L"Огляд...";
    uk[StrId::DlgNewPaste]          = L"📋 Вставити";
    uk[StrId::DlgNewCat]            = L"Категорія:";
    uk[StrId::DlgNewParts]          = L"Кількість частин (сегментів):";
    uk[StrId::DlgNewStartImm]       = L"Запустити негайно";
    uk[StrId::DlgOk]                = L"OK";
    uk[StrId::DlgCancel]            = L"Скасувати";
    uk[StrId::DlgClose]             = L"Закрити";
    uk[StrId::DlgSave]              = L"Зберегти";

    uk[StrId::DlgBatchTitle]        = L"Пакетне завантаження";
    uk[StrId::DlgBatchPattern]      = L"Шаблон URL (використовуйте (*) як маску):";
    uk[StrId::DlgBatchFrom]         = L"Від:";
    uk[StrId::DlgBatchTo]           = L"До:";
    uk[StrId::DlgBatchDigits]       = L"Кількість цифр:";
    uk[StrId::DlgBatchPreview]      = L"Перегляд";
    uk[StrId::DlgBatchList]         = L"Список для створення:";
    uk[StrId::DlgBatchAddAll]       = L"Додати всі до завдань";

    uk[StrId::DlgOptTitle]          = L"Налаштування";
    uk[StrId::DlgOptMaxTasks]       = L"Одночасних завантажень (1-10):";
    uk[StrId::DlgOptDefParts]       = L"Сегментів за замовчуванням (1-30):";
    uk[StrId::DlgOptSpeedLimit]     = L"Ліміт швидкості (КБ/с):";
    uk[StrId::DlgOptClip]           = L"Моніторинг буфера (автоперехоплення URL)";
    uk[StrId::DlgOptSound]          = L"Звуковий сигнал після завершення";
    uk[StrId::DlgOptDrop]           = L"Показувати плаваюче вікно (Drop Zone)";
    uk[StrId::DlgOptShutdown]       = L"Вимкнути комп'ютер після завершення всіх завантажень";
    uk[StrId::DlgOptMinToTray]      = L"При закритті [X] згортати в системний трей";
    uk[StrId::DlgOptAutoStart]      = L"Запускати автоматично разом з Windows";
    uk[StrId::DlgOptLanguage]       = L"Мова інтерфейсу (Language):";

    uk[StrId::DlgDeletePrompt]      = L"Ви дійсно бажаєте видалити вибрані завдання?\n\n(Натисніть 'Так' для видалення файлу з диска, 'Ні' щоб прибрати лише зі списку)";
    uk[StrId::DlgDeleteTitle]       = L"Видалити завдання";
    uk[StrId::DlgAboutTitle]        = L"Про програму Gety";
    uk[StrId::DlgAboutDesc]         = L"⚡ Gety\nВерсія: 1.0 (C++20 Native Ultra-Fast Edition)\n\nСучасний портативний менеджер швидкого завантаження, написаний на C++.\n\nМожливості:\n• Багатопотокове (1-30 сегментів) одночасне завантаження HTTP/HTTPS\n• Матричний візуалізатор блоків\n• Плаваюче вікно для скидання посилань (Drop Zone)\n• Автоперехоплення посилань із буфера обміну\n• Повна портативність без інсталяції\n• Гнучке обмеження швидкості\n• Пакетне завантаження (Batch URL)\n• Підтримка 12 мов і Unicode";

    uk[StrId::ActNew]               = L"Створити";
    uk[StrId::ActBatch]             = L"Пакет";
    uk[StrId::ActVideoLink]         = L"Додати посилання";
    uk[StrId::ActStart]             = L"Старт";
    uk[StrId::ActPause]             = L"Пауза";
    uk[StrId::ActDelete]            = L"Видалити";
    uk[StrId::ActSpeed]             = L"Швидкість";
    uk[StrId::ActLanguage]          = L"Мова";
    uk[StrId::ActSettings]          = L"Налашт.";
    uk[StrId::ActAbout]             = L"Про прогр.";
    uk[StrId::CtxPause]             = L"⏸ Пауза";
    uk[StrId::CtxStart]             = L"▶ Старт";
    uk[StrId::CtxOpenFile]          = L"📂 Відкрити файл";
    uk[StrId::CtxOpenFolder]        = L"📁 Відкрити папку";
    uk[StrId::CtxCopyUrl]           = L"📋 Скопіювати URL";
    uk[StrId::CtxDeleteTask]        = L"🗑 Видалити";
    uk[StrId::CtxOpenGety]          = L"Відкрити Gety";
    uk[StrId::DlgOptSaveDir]        = L"Папка збереження:";
    uk[StrId::DlgOptMaxRetries]     = L"Спроб повтору (1-50):";
    uk[StrId::CtxUpdateUrl]         = L"🔗 Оновити посилання (URL)...";
    uk[StrId::DlgUpdateUrlTitle]    = L"Оновлення посилання завантаження";
    uk[StrId::DlgUpdateUrlOld]      = L"Поточне (старе) посилання:";
    uk[StrId::DlgUpdateUrlNew]      = L"Нове посилання (URL):";
    uk[StrId::DlgUpdateUrlBtn]      = L"Оновити посилання";

    uk[StrId::DlgVideoTitle]        = L"Додати посилання на відео/медіа";
    uk[StrId::DlgVideoUrlPrompt]    = L"Посилання на відео або список відтворення (URL):";
    uk[StrId::DlgVideoAnalyze]      = L"Аналізувати";
    uk[StrId::DlgVideoAnalyzing]    = L"Аналіз медіа, зачекайте будь ласка...";
    uk[StrId::DlgVideoQuality]      = L"Якість / Формат:";
    uk[StrId::DlgVideoPlaylistVideos]= L"Відео для завантаження (Плейлист):";
    uk[StrId::DlgVideoSelectAll]    = L"Вибрати всі";
    uk[StrId::DlgVideoDeselectAll]  = L"Зняти вибір";
    uk[StrId::DlgVideoAudioOnly]    = L"Лише аудіо (MP3)";
    uk[StrId::DlgVideoDownload]     = L"Почати завантаження";
    uk[StrId::DlgVideoSaveDir]      = L"Папка збереження:";
    uk[StrId::DlgVideoBrowse]       = L"Огляд...";
    uk[StrId::DlgVideoFound]        = L"Відео знайдено";

    // 6. FRANÇAIS (FR)
    // ---------------------------------------------------------
    auto& fr = m_translations[LangId::French];
    fr[StrId::MenuFile]             = L"&Fichier";
    fr[StrId::MenuFileNew]          = L"&Nouveau téléchargement...\tCtrl+N";
    fr[StrId::MenuFileExit]         = L"&Quitter\tAlt+F4";
    fr[StrId::MenuTask]             = L"&Tâche";
    fr[StrId::MenuTaskStart]        = L"&Démarrer\tF5";
    fr[StrId::MenuTaskPause]        = L"&Pause\tF6";
    fr[StrId::MenuTaskOpenFile]     = L"&Ouvrir le fichier";
    fr[StrId::MenuTaskOpenFolder]   = L"Ouvrir le &dossier";
    fr[StrId::MenuTaskDelete]       = L"&Supprimer\tDel";
    fr[StrId::MenuView]             = L"&Affichage";
    fr[StrId::MenuTools]            = L"&Outils";
    fr[StrId::MenuToolsOptions]     = L"&Options...";
    fr[StrId::MenuLanguage]         = L"&Langue / Language";
    fr[StrId::MenuHelp]             = L"&Aide";
    fr[StrId::TbNew]                = L" Nouveau";
    fr[StrId::TbStart]              = L" Démarrer";
    fr[StrId::TbPause]              = L" Pause";
    fr[StrId::TbDelete]             = L" Supprimer";
    fr[StrId::CatAll]               = L"📂 Toutes les tâches";
    fr[StrId::CatDownloading]       = L"⏳ En cours";
    fr[StrId::CatDownloaded]        = L"✅ Téléchargés";
    fr[StrId::ColFilename]          = L"Nom du fichier";
    fr[StrId::ColSize]              = L"Taille";
    fr[StrId::ColProgress]          = L"Progression";
    fr[StrId::ColSpeed]             = L"Vitesse";
    fr[StrId::DlgOk]                = L"OK";
    fr[StrId::DlgCancel]            = L"Annuler";

    // ---------------------------------------------------------

    fr[StrId::ActNew]               = L"Nouveau";
    fr[StrId::ActBatch]             = L"Lot";
    fr[StrId::ActStart]             = L"Démarrer";
    fr[StrId::ActPause]             = L"Pause";
    fr[StrId::ActDelete]            = L"Supprimer";
    fr[StrId::ActSpeed]             = L"Vitesse";
    fr[StrId::ActLanguage]          = L"Langue";
    fr[StrId::ActSettings]          = L"Param.";
    fr[StrId::ActAbout]             = L"À propos";
    fr[StrId::CtxPause]             = L"⏸ Pause";
    fr[StrId::CtxStart]             = L"▶ Démarrer";
    fr[StrId::CtxOpenFile]          = L"📂 Ouvrir le fichier";
    fr[StrId::CtxOpenFolder]        = L"📁 Ouvrir le dossier";
    fr[StrId::CtxCopyUrl]           = L"📋 Copier l'URL";
    fr[StrId::CtxDeleteTask]        = L"🗑 Supprimer";
    fr[StrId::CtxOpenGety]          = L"Ouvrir Gety";
    fr[StrId::DlgOptSaveDir]        = L"Dossier d'enregistrement:";

    // 7. ESPAÑOL (ES)
    // ---------------------------------------------------------
    auto& es = m_translations[LangId::Spanish];
    es[StrId::MenuFile]             = L"&Archivo";
    es[StrId::MenuFileNew]          = L"&Nueva descarga...\tCtrl+N";
    es[StrId::MenuFileExit]         = L"&Salir\tAlt+F4";
    es[StrId::MenuTask]             = L"&Tarea";
    es[StrId::MenuTaskStart]        = L"&Iniciar\tF5";
    es[StrId::MenuTaskPause]        = L"&Pausar\tF6";
    es[StrId::MenuTaskOpenFile]     = L"&Abrir archivo";
    es[StrId::MenuTaskOpenFolder]   = L"Abrir &carpeta";
    es[StrId::MenuTaskDelete]       = L"&Eliminar\tDel";
    es[StrId::MenuView]             = L"&Ver";
    es[StrId::MenuTools]            = L"&Herramientas";
    es[StrId::MenuToolsOptions]     = L"&Opciones...";
    es[StrId::MenuLanguage]         = L"&Idioma / Language";
    es[StrId::MenuHelp]             = L"&Ayuda";
    es[StrId::TbNew]                = L" Nuevo";
    es[StrId::TbStart]              = L" Iniciar";
    es[StrId::TbPause]              = L" Pausar";
    es[StrId::TbDelete]             = L" Eliminar";
    es[StrId::CatAll]               = L"📂 Todas las tareas";
    es[StrId::CatDownloading]       = L"⏳ Descargando";
    es[StrId::CatDownloaded]        = L"✅ Descargados";
    es[StrId::ColFilename]          = L"Nombre";
    es[StrId::ColSize]              = L"Tamaño";
    es[StrId::ColProgress]          = L"Progreso";
    es[StrId::ColSpeed]             = L"Velocidad";
    es[StrId::DlgOk]                = L"Aceptar";
    es[StrId::DlgCancel]            = L"Cancelar";

    // ---------------------------------------------------------

    es[StrId::ActNew]               = L"Nuevo";
    es[StrId::ActBatch]             = L"Lote";
    es[StrId::ActStart]             = L"Iniciar";
    es[StrId::ActPause]             = L"Pausar";
    es[StrId::ActDelete]            = L"Eliminar";
    es[StrId::ActSpeed]             = L"Velocidad";
    es[StrId::ActLanguage]          = L"Idioma";
    es[StrId::ActSettings]          = L"Ajustes";
    es[StrId::ActAbout]             = L"Acerca de";
    es[StrId::CtxPause]             = L"⏸ Pausar";
    es[StrId::CtxStart]             = L"▶ Iniciar";
    es[StrId::CtxOpenFile]          = L"📂 Abrir archivo";
    es[StrId::CtxOpenFolder]        = L"📁 Abrir carpeta";
    es[StrId::CtxCopyUrl]           = L"📋 Copiar URL";
    es[StrId::CtxDeleteTask]        = L"🗑 Eliminar";
    es[StrId::CtxOpenGety]          = L"Abrir Gety";
    es[StrId::DlgOptSaveDir]        = L"Carpeta de destino:";

    // 8. ITALIANO (IT)
    // ---------------------------------------------------------
    auto& it = m_translations[LangId::Italian];
    it[StrId::MenuFile]             = L"&File";
    it[StrId::MenuFileNew]          = L"&Nuovo download...\tCtrl+N";
    it[StrId::MenuFileExit]         = L"&Esci\tAlt+F4";
    it[StrId::MenuTask]             = L"&Attività";
    it[StrId::MenuTaskStart]        = L"&Avvia\tF5";
    it[StrId::MenuTaskPause]        = L"&Pausa\tF6";
    it[StrId::MenuTaskOpenFile]     = L"&Apri file";
    it[StrId::MenuTaskOpenFolder]   = L"Apri &cartella";
    it[StrId::MenuTaskDelete]       = L"&Elimina\tDel";
    it[StrId::MenuView]             = L"&Visualizza";
    it[StrId::MenuTools]            = L"&Strumenti";
    it[StrId::MenuToolsOptions]     = L"&Opzioni...";
    it[StrId::MenuLanguage]         = L"&Lingua / Language";
    it[StrId::MenuHelp]             = L"&Aiuto";
    it[StrId::TbNew]                = L" Nuovo";
    it[StrId::TbStart]              = L" Avvia";
    it[StrId::TbPause]              = L" Pausa";
    it[StrId::TbDelete]             = L" Elimina";
    it[StrId::CatAll]               = L"📂 Tutte le attività";
    it[StrId::CatDownloading]       = L"⏳ In scaricamento";
    it[StrId::CatDownloaded]        = L"✅ Completati";
    it[StrId::ColFilename]          = L"Nome file";
    it[StrId::ColSize]              = L"Dimensione";
    it[StrId::ColProgress]          = L"Avanzamento";
    it[StrId::ColSpeed]             = L"Velocità";
    it[StrId::DlgOk]                = L"OK";
    it[StrId::DlgCancel]            = L"Annulla";

    // ---------------------------------------------------------

    it[StrId::ActNew]               = L"Nuovo";
    it[StrId::ActBatch]             = L"Batch";
    it[StrId::ActStart]             = L"Avvia";
    it[StrId::ActPause]             = L"Pausa";
    it[StrId::ActDelete]            = L"Elimina";
    it[StrId::ActSpeed]             = L"Velocità";
    it[StrId::ActLanguage]          = L"Lingua";
    it[StrId::ActSettings]          = L"Impost.";
    it[StrId::ActAbout]             = L"Info";
    it[StrId::CtxPause]             = L"⏸ Pausa";
    it[StrId::CtxStart]             = L"▶ Avvia";
    it[StrId::CtxOpenFile]          = L"📂 Apri file";
    it[StrId::CtxOpenFolder]        = L"📁 Apri cartella";
    it[StrId::CtxCopyUrl]           = L"📋 Copia URL";
    it[StrId::CtxDeleteTask]        = L"🗑 Rimuovi";
    it[StrId::CtxOpenGety]          = L"Apri Gety";
    it[StrId::DlgOptSaveDir]        = L"Cartella predefinita:";

    // 9. PORTUGUÊS (PT)
    // ---------------------------------------------------------
    auto& pt = m_translations[LangId::Portuguese];
    pt[StrId::MenuFile]             = L"&Arquivo";
    pt[StrId::MenuFileNew]          = L"&Novo Download...\tCtrl+N";
    pt[StrId::MenuFileExit]         = L"&Sair\tAlt+F4";
    pt[StrId::MenuTask]             = L"&Tarefa";
    pt[StrId::MenuTaskStart]        = L"&Iniciar\tF5";
    pt[StrId::MenuTaskPause]        = L"&Pausar\tF6";
    pt[StrId::MenuTaskOpenFile]     = L"&Abrir arquivo";
    pt[StrId::MenuTaskOpenFolder]   = L"Abrir &pasta";
    pt[StrId::MenuTaskDelete]       = L"&Excluir\tDel";
    pt[StrId::MenuView]             = L"&Exibir";
    pt[StrId::MenuTools]            = L"&Ferramentas";
    pt[StrId::MenuToolsOptions]     = L"&Opções...";
    pt[StrId::MenuLanguage]         = L"&Idioma / Language";
    pt[StrId::MenuHelp]             = L"&Ajuda";
    pt[StrId::TbNew]                = L" Novo";
    pt[StrId::TbStart]              = L" Iniciar";
    pt[StrId::TbPause]              = L" Pausar";
    pt[StrId::TbDelete]             = L" Excluir";
    pt[StrId::CatAll]               = L"📂 Todas as tarefas";
    pt[StrId::CatDownloading]       = L"⏳ Baixando";
    pt[StrId::CatDownloaded]        = L"✅ Concluídos";
    pt[StrId::ColFilename]          = L"Nome do arquivo";
    pt[StrId::ColSize]              = L"Tamanho";
    pt[StrId::ColProgress]          = L"Progresso";
    pt[StrId::ColSpeed]             = L"Velocidade";
    pt[StrId::DlgOk]                = L"OK";
    pt[StrId::DlgCancel]            = L"Cancelar";

    // ---------------------------------------------------------

    pt[StrId::ActNew]               = L"Novo";
    pt[StrId::ActBatch]             = L"Lote";
    pt[StrId::ActStart]             = L"Iniciar";
    pt[StrId::ActPause]             = L"Pausar";
    pt[StrId::ActDelete]            = L"Excluir";
    pt[StrId::ActSpeed]             = L"Velocidade";
    pt[StrId::ActLanguage]          = L"Idioma";
    pt[StrId::ActSettings]          = L"Config.";
    pt[StrId::ActAbout]             = L"Sobre";
    pt[StrId::CtxPause]             = L"⏸ Pausar";
    pt[StrId::CtxStart]             = L"▶ Iniciar";
    pt[StrId::CtxOpenFile]          = L"📂 Abrir arquivo";
    pt[StrId::CtxOpenFolder]        = L"📁 Abrir pasta";
    pt[StrId::CtxCopyUrl]           = L"📋 Copiar URL";
    pt[StrId::CtxDeleteTask]        = L"🗑 Remover";
    pt[StrId::CtxOpenGety]          = L"Abrir Gety";
    pt[StrId::DlgOptSaveDir]        = L"Pasta padrão:";

    // 10. 日本語 (JA)
    // ---------------------------------------------------------
    auto& ja = m_translations[LangId::Japanese];
    ja[StrId::MenuFile]             = L"ファイル(&F)";
    ja[StrId::MenuFileNew]          = L"新規ダウンロード(&N)...\tCtrl+N";
    ja[StrId::MenuFileExit]         = L"終了(&X)\tAlt+F4";
    ja[StrId::MenuTask]             = L"タスク(&T)";
    ja[StrId::MenuTaskStart]        = L"開始(&S)\tF5";
    ja[StrId::MenuTaskPause]        = L"一時停止(&P)\tF6";
    ja[StrId::MenuTaskOpenFile]     = L"ファイルを開く(&O)";
    ja[StrId::MenuTaskOpenFolder]   = L"フォルダを開く(&F)";
    ja[StrId::MenuTaskDelete]       = L"削除(&D)\tDel";
    ja[StrId::MenuView]             = L"表示(&V)";
    ja[StrId::MenuTools]            = L"ツール(&T)";
    ja[StrId::MenuToolsOptions]     = L"設定(&O)...";
    ja[StrId::MenuLanguage]         = L"言語(&L) / Language";
    ja[StrId::MenuHelp]             = L"ヘルプ(&H)";
    ja[StrId::TbNew]                = L" 新規";
    ja[StrId::TbStart]              = L" 開始";
    ja[StrId::TbPause]              = L" 一時停止";
    ja[StrId::TbDelete]             = L" 削除";
    ja[StrId::CatAll]               = L"📂 すべてのタスク";
    ja[StrId::CatDownloading]       = L"⏳ ダウンロード中";
    ja[StrId::CatDownloaded]        = L"✅ 完了";
    ja[StrId::ColFilename]          = L"ファイル名";
    ja[StrId::ColSize]              = L"サイズ";
    ja[StrId::ColProgress]          = L"進捗";
    ja[StrId::ColSpeed]             = L"速度";
    ja[StrId::DlgOk]                = L"OK";
    ja[StrId::DlgCancel]            = L"キャンセル";

    // ---------------------------------------------------------

    ja[StrId::ActNew]               = L"新規";
    ja[StrId::ActBatch]             = L"バッチ";
    ja[StrId::ActStart]             = L"開始";
    ja[StrId::ActPause]             = L"停止";
    ja[StrId::ActDelete]            = L"削除";
    ja[StrId::ActSpeed]             = L"速度";
    ja[StrId::ActLanguage]          = L"言語";
    ja[StrId::ActSettings]          = L"設定";
    ja[StrId::ActAbout]             = L"情報";
    ja[StrId::CtxPause]             = L"⏸ 停止";
    ja[StrId::CtxStart]             = L"▶ 開始";
    ja[StrId::CtxOpenFile]          = L"📂 ファイルを開く";
    ja[StrId::CtxOpenFolder]        = L"📁 フォルダを開く";
    ja[StrId::CtxCopyUrl]           = L"📋 URLをコピー";
    ja[StrId::CtxDeleteTask]        = L"🗑 削除";
    ja[StrId::CtxOpenGety]          = L"Getyを開く";
    ja[StrId::DlgOptSaveDir]        = L"保存先フォルダ:";

    // 11. 简体中文 (ZH)
    // ---------------------------------------------------------
    auto& zh = m_translations[LangId::Chinese];
    zh[StrId::MenuFile]             = L"文件(&F)";
    zh[StrId::MenuFileNew]          = L"新建下载(&N)...\tCtrl+N";
    zh[StrId::MenuFileBatch]        = L"批量下载(&B)...\tCtrl+B";
    zh[StrId::MenuFileExit]         = L"退出(&X)\tAlt+F4";
    zh[StrId::MenuTask]             = L"任务(&T)";
    zh[StrId::MenuTaskStart]        = L"开始(&S)\tF5";
    zh[StrId::MenuTaskPause]        = L"暂停(&P)\tF6";
    zh[StrId::MenuTaskOpenFile]     = L"打开文件(&O)";
    zh[StrId::MenuTaskOpenFolder]   = L"打开所在文件夹(&F)";
    zh[StrId::MenuTaskDelete]       = L"删除(&D)\tDel";
    zh[StrId::MenuView]             = L"视图(&V)";
    zh[StrId::MenuTools]            = L"工具(&T)";
    zh[StrId::MenuToolsOptions]     = L"选项(&O)...";
    zh[StrId::MenuLanguage]         = L"语言(&L) / Language";
    zh[StrId::MenuHelp]             = L"帮助(&H)";
    zh[StrId::TbNew]                = L" 新建";
    zh[StrId::TbStart]              = L" 开始";
    zh[StrId::TbPause]              = L" 暂停";
    zh[StrId::TbDelete]             = L" 删除";
    zh[StrId::CatAll]               = L"📂 所有任务";
    zh[StrId::CatDownloading]       = L"⏳ 正在下载";
    zh[StrId::CatDownloaded]        = L"✅ 已完成";
    zh[StrId::ColFilename]          = L"文件名";
    zh[StrId::ColSize]              = L"大小";
    zh[StrId::ColProgress]          = L"进度";
    zh[StrId::ColSpeed]             = L"速度";
    zh[StrId::DlgOk]                = L"确定";
    zh[StrId::DlgCancel]            = L"取消";

    zh[StrId::ActNew]               = L"新建";
    zh[StrId::ActBatch]             = L"批量";
    zh[StrId::ActStart]             = L"开始";
    zh[StrId::ActPause]             = L"暂停";
    zh[StrId::ActDelete]            = L"删除";
    zh[StrId::ActSpeed]             = L"速度";
    zh[StrId::ActLanguage]          = L"语言";
    zh[StrId::ActSettings]          = L"设置";
    zh[StrId::ActAbout]             = L"关于";
    zh[StrId::CtxPause]             = L"⏸ 暂停";
    zh[StrId::CtxStart]             = L"▶ 开始";
    zh[StrId::CtxOpenFile]          = L"📂 打开文件";
    zh[StrId::CtxOpenFolder]        = L"📁 打开文件夹";
    zh[StrId::CtxCopyUrl]           = L"📋 复制 URL";
    zh[StrId::CtxDeleteTask]        = L"🗑 移除";
    zh[StrId::CtxOpenGety]          = L"打开 Gety";
    zh[StrId::DlgOptSaveDir]        = L"默认保存文件夹:";
}

} // namespace Gety
