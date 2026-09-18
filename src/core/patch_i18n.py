import sys

translations = {
    'tr': """    tr[StrId::ActNew]               = L"Yeni";
    tr[StrId::ActBatch]             = L"Toplu";
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
    tr[StrId::DlgOptSaveDir]        = L"Varsayılan Kayıt Klasörü:";\n""",
    
    'en': """    en[StrId::ActNew]               = L"New";
    en[StrId::ActBatch]             = L"Batch";
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
    en[StrId::DlgOptSaveDir]        = L"Default Save Folder:";\n""",
    
    'de': """    de[StrId::ActNew]               = L"Neu";
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
    de[StrId::DlgOptSaveDir]        = L"Standard-Speicherordner:";\n""",
    
    'el': """    el[StrId::ActNew]               = L"Νέο";
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
    el[StrId::DlgOptSaveDir]        = L"Προεπιλεγμένος Φάκελος:";\n""",
    
    'ru': """    ru[StrId::ActNew]               = L"Новая";
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
    ru[StrId::DlgOptSaveDir]        = L"Папка сохранения:";\n""",
    
    'fr': """    fr[StrId::ActNew]               = L"Nouveau";
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
    fr[StrId::DlgOptSaveDir]        = L"Dossier d'enregistrement:";\n""",
    
    'es': """    es[StrId::ActNew]               = L"Nuevo";
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
    es[StrId::DlgOptSaveDir]        = L"Carpeta de destino:";\n""",
    
    'it': """    it[StrId::ActNew]               = L"Nuovo";
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
    it[StrId::DlgOptSaveDir]        = L"Cartella predefinita:";\n""",
    
    'pt': """    pt[StrId::ActNew]               = L"Novo";
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
    pt[StrId::DlgOptSaveDir]        = L"Pasta padrão:";\n""",
    
    'ja': """    ja[StrId::ActNew]               = L"新規";
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
    ja[StrId::DlgOptSaveDir]        = L"保存先フォルダ:";\n""",
    
    'zh': """    zh[StrId::ActNew]               = L"新建";
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
    zh[StrId::DlgOptSaveDir]        = L"默认保存文件夹:";\n"""
}

with open(r'E:\0_SkySoft\Gety\src\core\I18n.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

def insert_before_str(text, search, insert):
    idx = text.find(search)
    if idx == -1: return text
    return text[:idx] + insert + text[idx:]

blocks = [
    ('tr', '    // 2. ENGLISH (EN)'),
    ('en', '    // 3. DEUTSCH (DE)'),
    ('de', '    // 4. ΕΛΛΗΝΙΚΑ (EL - GREEK)'),
    ('el', '    // 5. РУССКИЙ (RU)'),
    ('ru', '    // 6. FRANÇAIS (FR)'),
    ('fr', '    // 7. ESPAÑOL (ES)'),
    ('es', '    // 8. ITALIANO (IT)'),
    ('it', '    // 9. PORTUGUÊS (PT)'),
    ('pt', '    // 10. 日本語 (JA)'),
    ('ja', '    // 11. 简体中文 (ZH)')
]

for lang, next_block in blocks:
    content = insert_before_str(content, next_block, '\n' + translations[lang] + '\n')

content = insert_before_str(content, '}\n\n} // namespace Gety', '\n' + translations['zh'])

with open(r'E:\0_SkySoft\Gety\src\core\I18n.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print('I18n.cpp patched successfully')
