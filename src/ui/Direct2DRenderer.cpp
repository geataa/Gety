#include "Direct2DRenderer.h"
#include "Theme.h"
#include "../core/I18n.h"
#include <algorithm>
#include <cwchar>
#include <wincodec.h>

namespace Gety {

Direct2DRenderer::Direct2DRenderer() {
}

Direct2DRenderer::~Direct2DRenderer() {
    DiscardDeviceResources();
    DiscardTextFormats();

    if (m_pDWriteFactory) { m_pDWriteFactory->Release(); m_pDWriteFactory = nullptr; }
    if (m_pD2DFactory) { m_pD2DFactory->Release(); m_pD2DFactory = nullptr; }
}

void Direct2DRenderer::DiscardTextFormats() {
    if (m_pFormatRegular) { m_pFormatRegular->Release(); m_pFormatRegular = nullptr; }
    if (m_pFormatMedium) { m_pFormatMedium->Release(); m_pFormatMedium = nullptr; }
    if (m_pFormatSemiBold) { m_pFormatSemiBold->Release(); m_pFormatSemiBold = nullptr; }
    if (m_pFormatBold) { m_pFormatBold->Release(); m_pFormatBold = nullptr; }
    if (m_pFormatSmall) { m_pFormatSmall->Release(); m_pFormatSmall = nullptr; }
    if (m_pFormatIcons) { m_pFormatIcons->Release(); m_pFormatIcons = nullptr; }
    if (m_pFormatMono) { m_pFormatMono->Release(); m_pFormatMono = nullptr; }
    if (m_pFormatTopBtn) { m_pFormatTopBtn->Release(); m_pFormatTopBtn = nullptr; }
}

void Direct2DRenderer::CreateTextFormats() {
    DiscardTextFormats();
    if (!m_pDWriteFactory) return;

    auto createFormat = [&](const wchar_t* fontName, float size, DWRITE_FONT_WEIGHT weight, IDWriteTextFormat** ppFormat) {
        if (!m_pDWriteFactory) return;
        m_pDWriteFactory->CreateTextFormat(
            fontName,
            nullptr,
            weight,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size * m_dpiScale,
            L"tr-TR",
            ppFormat
        );
        if (*ppFormat) {
            (*ppFormat)->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
    };

    // Windows Standard Segoe UI Typography Scale (9pt = 12.0 DIP baseline)
    createFormat(L"Segoe UI", 12.0f, DWRITE_FONT_WEIGHT_NORMAL, &m_pFormatRegular);
    createFormat(L"Segoe UI", 12.0f, DWRITE_FONT_WEIGHT_MEDIUM, &m_pFormatMedium);
    createFormat(L"Segoe UI", 12.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_pFormatSemiBold);
    createFormat(L"Segoe UI", 14.0f, DWRITE_FONT_WEIGHT_BOLD, &m_pFormatBold);
    createFormat(L"Segoe UI", 10.5f, DWRITE_FONT_WEIGHT_NORMAL, &m_pFormatSmall);
    createFormat(L"Segoe UI Symbol", 12.0f, DWRITE_FONT_WEIGHT_NORMAL, &m_pFormatIcons);
    createFormat(L"Consolas", 11.0f, DWRITE_FONT_WEIGHT_NORMAL, &m_pFormatMono);
    createFormat(L"Segoe UI", 11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, &m_pFormatTopBtn);

    if (m_pFormatMedium) {
        DWRITE_TRIMMING trimming = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        IDWriteInlineObject* pEllipsis = nullptr;
        if (SUCCEEDED(m_pDWriteFactory->CreateEllipsisTrimmingSign(m_pFormatMedium, &pEllipsis)) && pEllipsis) {
            m_pFormatMedium->SetTrimming(&trimming, pEllipsis);
            pEllipsis->Release();
        }
    }

    if (m_pFormatTopBtn) {
        m_pFormatTopBtn->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        m_pFormatTopBtn->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
}

void Direct2DRenderer::OnDpiChanged(UINT newDpi) {
    m_dpiScale = (float)newDpi / 96.0f;
    if (m_dpiScale < 0.5f) m_dpiScale = 1.0f;

    CreateTextFormats();

    if (m_hwnd && IsWindow(m_hwnd)) {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        m_width = rc.right - rc.left;
        m_height = rc.bottom - rc.top;
        if (m_pRenderTarget) {
            m_pRenderTarget->Resize(D2D1::SizeU(m_width, m_height));
        }
        UpdateLayout(m_width, m_height, m_splitterX, m_splitterY);
    }
}

bool Direct2DRenderer::Initialize(HWND hwnd) {
    m_hwnd = hwnd;

    // Detect DPI
    UINT dpi = static_cast<UINT>(Theme::GetWindowDpi(m_hwnd));
    m_dpiScale = (float)dpi / 96.0f;
    if (m_dpiScale < 0.5f) m_dpiScale = 1.0f;

    // Create D2D Factory
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    if (FAILED(hr)) return false;

    // Create DWrite Factory
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&m_pDWriteFactory)
    );
    if (FAILED(hr)) return false;

    // Create Text Formats
    CreateTextFormats();

    bool ok = SUCCEEDED(CreateDeviceResources());
    if (m_hwnd) {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        m_width = rc.right - rc.left;
        m_height = rc.bottom - rc.top;
        if (m_width > 0 && m_height > 0) {
            UpdateLayout(m_width, m_height, m_splitterX, m_splitterY);
        }
    }
    return ok;
}

HRESULT Direct2DRenderer::CreateDeviceResources() {
    if (m_pRenderTarget) return S_OK;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    if (size.width == 0) size.width = 800;
    if (size.height == 0) size.height = 600;

    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        96.0f, 96.0f
    );

    HRESULT hr = m_pD2DFactory->CreateHwndRenderTarget(
        rtProps,
        D2D1::HwndRenderTargetProperties(m_hwnd, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY),
        &m_pRenderTarget
    );
    if (FAILED(hr)) return hr;

    m_pRenderTarget->SetDpi(96.0f, 96.0f);
    m_pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    m_pRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

    m_pCurrentRT = m_pRenderTarget;
    CreateBrushesFor(m_pRenderTarget);

    return S_OK;
}

void Direct2DRenderer::CreateBrushesFor(ID2D1RenderTarget* pRT) {
    if (!pRT) return;
    DiscardBrushes();

    pRT->CreateSolidColorBrush(D2D1::ColorF(0.05f, 0.06f, 0.08f, 1.0f), &m_pBgBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.09f, 0.13f, 0.95f), &m_pCardBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.14f, 0.15f, 0.20f, 0.9f), &m_pCardBorderBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.10f, 0.11f, 0.15f, 1.0f), &m_pHeaderBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.95f, 0.96f, 0.98f, 1.0f), &m_pTextBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.56f, 0.59f, 0.66f, 1.0f), &m_pMutedTextBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.36f, 0.39f, 0.45f, 1.0f), &m_pDimTextBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.00f, 0.65f, 1.00f, 1.0f), &m_pCyanBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.00f, 0.65f, 1.00f, 0.22f), &m_pCyanGlowBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.13f, 0.77f, 0.37f, 1.0f), &m_pGreenBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.13f, 0.77f, 0.37f, 0.22f), &m_pGreenGlowBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.96f, 0.62f, 0.04f, 1.0f), &m_pAmberBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.94f, 0.27f, 0.27f, 1.0f), &m_pRedBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.00f, 1.00f, 1.00f, 0.08f), &m_pHoverBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.00f, 0.65f, 1.00f, 0.40f), &m_pPressedBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.00f, 0.60f, 1.00f, 0.20f), &m_pActivePillBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(1.00f, 1.00f, 1.00f, 0.14f), &m_pPillBorderBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.11f, 0.13f, 0.18f, 1.0f), &m_pProgressTrackBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.00f, 0.65f, 1.00f, 0.75f), &m_pSplitterHoverBrush);
    pRT->CreateSolidColorBrush(D2D1::ColorF(0.13f, 0.14f, 0.20f, 1.0f), &m_pPendingBlockBrush);
}

void Direct2DRenderer::DiscardBrushes() {
    if (m_pBgBrush) { m_pBgBrush->Release(); m_pBgBrush = nullptr; }
    if (m_pCardBrush) { m_pCardBrush->Release(); m_pCardBrush = nullptr; }
    if (m_pCardBorderBrush) { m_pCardBorderBrush->Release(); m_pCardBorderBrush = nullptr; }
    if (m_pHeaderBrush) { m_pHeaderBrush->Release(); m_pHeaderBrush = nullptr; }
    if (m_pTextBrush) { m_pTextBrush->Release(); m_pTextBrush = nullptr; }
    if (m_pMutedTextBrush) { m_pMutedTextBrush->Release(); m_pMutedTextBrush = nullptr; }
    if (m_pDimTextBrush) { m_pDimTextBrush->Release(); m_pDimTextBrush = nullptr; }
    if (m_pCyanBrush) { m_pCyanBrush->Release(); m_pCyanBrush = nullptr; }
    if (m_pCyanGlowBrush) { m_pCyanGlowBrush->Release(); m_pCyanGlowBrush = nullptr; }
    if (m_pGreenBrush) { m_pGreenBrush->Release(); m_pGreenBrush = nullptr; }
    if (m_pGreenGlowBrush) { m_pGreenGlowBrush->Release(); m_pGreenGlowBrush = nullptr; }
    if (m_pAmberBrush) { m_pAmberBrush->Release(); m_pAmberBrush = nullptr; }
    if (m_pRedBrush) { m_pRedBrush->Release(); m_pRedBrush = nullptr; }
    if (m_pHoverBrush) { m_pHoverBrush->Release(); m_pHoverBrush = nullptr; }
    if (m_pPressedBrush) { m_pPressedBrush->Release(); m_pPressedBrush = nullptr; }
    if (m_pActivePillBrush) { m_pActivePillBrush->Release(); m_pActivePillBrush = nullptr; }
    if (m_pPillBorderBrush) { m_pPillBorderBrush->Release(); m_pPillBorderBrush = nullptr; }
    if (m_pProgressTrackBrush) { m_pProgressTrackBrush->Release(); m_pProgressTrackBrush = nullptr; }
    if (m_pSplitterHoverBrush) { m_pSplitterHoverBrush->Release(); m_pSplitterHoverBrush = nullptr; }
    if (m_pPendingBlockBrush) { m_pPendingBlockBrush->Release(); m_pPendingBlockBrush = nullptr; }
}

void Direct2DRenderer::DiscardDeviceResources() {
    DiscardBrushes();
    if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = nullptr; }
    m_pCurrentRT = nullptr;
}

void Direct2DRenderer::OnResize(UINT width, UINT height) {
    m_width = static_cast<int>(width);
    m_height = static_cast<int>(height);
    if (m_pRenderTarget) {
        D2D1_SIZE_U size = D2D1::SizeU(width, height);
        m_pRenderTarget->Resize(size);
    }
    UpdateLayout(m_width, m_height, m_splitterX, m_splitterY);
}

void Direct2DRenderer::UpdateLayout(int width, int height, int splitterX, int splitterY) {
    m_width = width;
    m_height = height;
    m_splitterX = (std::max)(static_cast<int>(140 * m_dpiScale), (std::min)(width - static_cast<int>(260 * m_dpiScale), splitterX));
    m_splitterY = (std::max)(static_cast<int>(140 * m_dpiScale), (std::min)(height - static_cast<int>(130 * m_dpiScale), splitterY));

    float w = static_cast<float>(width);
    float h = static_cast<float>(height);
    float sX = static_cast<float>(m_splitterX);
    float sY = static_cast<float>(m_splitterY);

    float topBarH = 40.0f * m_dpiScale;

    // 1. Top Bar
    m_rcTopBar = D2D1::RectF(0.0f, 0.0f, w, topBarH);
    m_rcBrand = D2D1::RectF(12.0f * m_dpiScale, 6.0f * m_dpiScale, 105.0f * m_dpiScale, topBarH - 6.0f * m_dpiScale);

    // Top Right Window Buttons (GhostView circular/rounded style)
    float btnSize = 24.0f * m_dpiScale;
    float btnMarginR = 10.0f * m_dpiScale;
    float winBtnY = (topBarH - btnSize) / 2.0f;
    float gap = 5.0f * m_dpiScale;

    float closeR = w - btnMarginR;
    float closeL = closeR - btnSize;
    m_btnClose.id = BTN_CLOSE;
    m_btnClose.icon = L"\x2715"; // ✕
    m_btnClose.rect = D2D1::RectF(closeL, winBtnY, closeR, winBtnY + btnSize);

    float maxR = closeL - gap;
    float maxL = maxR - btnSize;
    m_btnMax.id = BTN_MAXIMIZE;
    m_btnMax.icon = (m_hwnd && IsZoomed(m_hwnd)) ? L"\x2922" : L"\x25A2";
    m_btnMax.rect = D2D1::RectF(maxL, winBtnY, maxR, winBtnY + btnSize);

    float minR = maxL - gap;
    float minL = minR - btnSize;
    m_btnMin.id = BTN_MINIMIZE;
    m_btnMin.icon = L"\x2015"; // ─
    m_btnMin.rect = D2D1::RectF(minL, winBtnY, minR, winBtnY + btnSize);

    // Floating Action Pill Bar
    m_actionButtons.clear();
    struct BtnConfig { int id; std::wstring icon; std::wstring label; };
    std::vector<BtnConfig> btnConfigs = {
        { BTN_NEW, L"＋", LStr(StrId::ActNew) },
        { BTN_BATCH, L"📋", LStr(StrId::ActBatch) },
        { BTN_VIDEO_LINK, L"🎬", LStr(StrId::ActVideoLink) },
        { BTN_OLLAMA_MODEL, L"🤖", LStr(StrId::ActOllamaModel) },
        { BTN_HF_MODEL, L"🤗", LStr(StrId::ActHfModel) },
        { BTN_START, L"▶", LStr(StrId::ActStart) },
        { BTN_PAUSE, L"⏸", LStr(StrId::ActPause) },
        { BTN_DELETE, L"🗑", LStr(StrId::ActDelete) },
        { BTN_SPEED_MODE, L"🎛", LStr(StrId::ActSpeed) },
        { BTN_LANGUAGE, L"🌐", LStr(StrId::ActLanguage) },
        { BTN_SETTINGS, L"⚙", LStr(StrId::ActSettings) },
        { BTN_ABOUT, L"ℹ", LStr(StrId::ActAbout) }
    };

    float actionL = 106.0f * m_dpiScale;
    float curBtnX = actionL + 4.0f * m_dpiScale;
    float btnH = 26.0f * m_dpiScale;
    float btnY = (topBarH - btnH) / 2.0f;
    float maxAllowedX = minL - 10.0f * m_dpiScale;

    // Responsive toolbar: measure full text vs icon widths
    std::vector<float> fullWidths(btnConfigs.size(), 28.0f * m_dpiScale);
    std::vector<float> iconWidths(btnConfigs.size(), 28.0f * m_dpiScale);
    float totalFullW = 0.0f;
    float totalHybridW = 0.0f;
    float totalIconW = 0.0f;

    for (size_t i = 0; i < btnConfigs.size(); ++i) {
        std::wstring fullLabel = btnConfigs[i].icon + L" " + btnConfigs[i].label;
        if (m_pDWriteFactory && m_pFormatTopBtn) {
            IDWriteTextLayout* pLayout = nullptr;
            HRESULT hr = m_pDWriteFactory->CreateTextLayout(
                fullLabel.c_str(), static_cast<UINT32>(fullLabel.length()),
                m_pFormatTopBtn, 1000.0f, 1000.0f, &pLayout
            );
            if (SUCCEEDED(hr) && pLayout) {
                DWRITE_TEXT_METRICS metrics;
                pLayout->GetMetrics(&metrics);
                fullWidths[i] = metrics.width + 16.0f * m_dpiScale;
                pLayout->Release();
            }
        }
        iconWidths[i] = 28.0f * m_dpiScale;

        totalFullW += fullWidths[i] + 3.0f * m_dpiScale;
        float hybridBtnW = (i <= 4) ? fullWidths[i] : iconWidths[i];
        totalHybridW += hybridBtnW + 3.0f * m_dpiScale;
        totalIconW += iconWidths[i] + 3.0f * m_dpiScale;
    }

    float availableW = maxAllowedX - actionL - 8.0f * m_dpiScale;
    enum class PillMode { FullText, Hybrid, IconOnly };
    PillMode pMode = PillMode::FullText;
    if (totalFullW > availableW) {
        pMode = (totalHybridW <= availableW) ? PillMode::Hybrid : PillMode::IconOnly;
    }

    for (size_t i = 0; i < btnConfigs.size(); ++i) {
        Direct2DTopButton b;
        b.id = btnConfigs[i].id;
        b.icon = btnConfigs[i].icon;
        b.label = btnConfigs[i].label;

        float bW = 28.0f * m_dpiScale;
        if (pMode == PillMode::FullText) {
            bW = fullWidths[i];
        } else if (pMode == PillMode::Hybrid) {
            bW = (i <= 4) ? fullWidths[i] : iconWidths[i];
        } else {
            bW = iconWidths[i];
        }

        if (curBtnX + bW > maxAllowedX) {
            bW = iconWidths[i];
            if (curBtnX + bW > maxAllowedX) break;
        }

        b.rect = D2D1::RectF(curBtnX, btnY, curBtnX + bW, btnY + btnH);
        m_actionButtons.push_back(b);
        curBtnX += bW + 3.0f * m_dpiScale;
    }

    // Wrap pill tightly around buttons
    m_rcActionPill = D2D1::RectF(actionL, btnY - 2.5f * m_dpiScale, curBtnX + 2.0f * m_dpiScale, btnY + btnH + 2.5f * m_dpiScale);

    // 2. Main Work Cards
    float marginX = 10.0f * m_dpiScale;
    float topCardsY = topBarH + 4.0f * m_dpiScale;
    float bottomBarH = 28.0f * m_dpiScale;
    float bottomMargin = 6.0f * m_dpiScale;
    float bottomCardsY = h - bottomBarH - bottomMargin - 4.0f * m_dpiScale;

    // Left Category Card
    m_rcCategoryCard = D2D1::RectF(marginX, topCardsY, sX, bottomCardsY);

    // Splitter Vertical
    m_rcSplitterV = D2D1::RectF(sX, topCardsY, sX + 5.0f * m_dpiScale, bottomCardsY);

    // Right Top Task List Card
    float rightCardsL = sX + 5.0f * m_dpiScale;
    float rightCardsR = w - marginX;
    m_rcTaskListCard = D2D1::RectF(rightCardsL, topCardsY, rightCardsR, sY);

    // Splitter Horizontal
    m_rcSplitterH = D2D1::RectF(rightCardsL, sY, rightCardsR, sY + 5.0f * m_dpiScale);

    // Right Bottom Detail Card
    m_rcDetailCard = D2D1::RectF(rightCardsL, sY + 5.0f * m_dpiScale, rightCardsR, bottomCardsY);

    // Bottom Status Bar Pill
    m_rcBottomBar = D2D1::RectF(marginX, h - bottomBarH - bottomMargin, w - marginX, h - bottomMargin);

    // Setup Category item rects
    m_categoryDefs = {
        { CategoryFilterType::All, LStr(StrId::CatAll), L"" },
        { CategoryFilterType::Unfinished, LStr(StrId::CatUnfinished), L"" },
        { CategoryFilterType::Downloading, LStr(StrId::CatDownloading), L"" },
        { CategoryFilterType::Paused, LStr(StrId::CatPaused), L"" },
        { CategoryFilterType::Downloaded, LStr(StrId::CatDownloaded), L"" },
        { CategoryFilterType::Trash, LStr(StrId::CatTrash), L"" },
        { CategoryFilterType::Music, LStr(StrId::CatMusic), L"" },
        { CategoryFilterType::Video, LStr(StrId::CatVideo), L"" },
        { CategoryFilterType::Software, LStr(StrId::CatSoftware), L"" },
        { CategoryFilterType::Documents, LStr(StrId::CatDocuments), L"" },
        { CategoryFilterType::AI, LStr(StrId::CatAI), L"" }
    };

    float catItemY = topCardsY + 30.0f * m_dpiScale;
    float catItemH = 28.0f * m_dpiScale;
    for (auto& cat : m_categoryDefs) {
        cat.rect = D2D1::RectF(marginX + 6.0f * m_dpiScale, catItemY, sX - 6.0f * m_dpiScale, catItemY + catItemH);
        catItemY += catItemH + 2.0f * m_dpiScale;
        if (cat.type == CategoryFilterType::Trash) {
            catItemY += 5.0f * m_dpiScale;
        }
    }

    // Setup Detail Tabs
    bool isTr = (I18n::Instance().GetCurrentLanguage() == LangId::Turkish);
    m_detailTabs = {
        { DetailTab::Matrix, isTr ? L"Blok Grafiği" : L"Block Matrix", L"🔲" },
        { DetailTab::Segments, isTr ? L"Segmentler" : L"Segments", L"📊" },
        { DetailTab::Logs, isTr ? L"Günlük (Log)" : L"Connection Log", L"📝" },
        { DetailTab::Info, isTr ? L"Dosya Bilgisi" : L"Task Info", L"ℹ" }
    };

    float tabX = rightCardsL + 8.0f * m_dpiScale;
    float tabY = sY + 5.0f * m_dpiScale + 5.0f * m_dpiScale;
    float tabH = 26.0f * m_dpiScale;
    for (auto& tab : m_detailTabs) {
        float tabW = 96.0f * m_dpiScale;
        tab.rect = D2D1::RectF(tabX, tabY, tabX + tabW, tabY + tabH);
        tabX += tabW + 4.0f * m_dpiScale;
    }
}

// -------------------------------------------------------------
// Hit Testing
// -------------------------------------------------------------

bool Direct2DRenderer::IsOverTopRightButtons(int x, int y, int& outBtnId) const {
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);

    auto hit = [&](const Direct2DTopButton& btn) {
        return (fx >= btn.rect.left && fx <= btn.rect.right && fy >= btn.rect.top && fy <= btn.rect.bottom);
    };

    if (hit(m_btnClose)) { outBtnId = BTN_CLOSE; return true; }
    if (hit(m_btnMax)) { outBtnId = BTN_MAXIMIZE; return true; }
    if (hit(m_btnMin)) { outBtnId = BTN_MINIMIZE; return true; }
    return false;
}

bool Direct2DRenderer::IsOverActionBar(int x, int y, int& outBtnId) const {
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);

    for (const auto& btn : m_actionButtons) {
        if (fx >= btn.rect.left && fx <= btn.rect.right && fy >= btn.rect.top && fy <= btn.rect.bottom) {
            outBtnId = btn.id;
            return true;
        }
    }
    return false;
}

bool Direct2DRenderer::IsOverCategories(int x, int y, CategoryFilterType& outCat) const {
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);

    if (fx < m_rcCategoryCard.left || fx > m_rcCategoryCard.right ||
        fy < m_rcCategoryCard.top + 26.0f * m_dpiScale || fy > m_rcCategoryCard.bottom) {
        return false;
    }

    for (const auto& cat : m_categoryDefs) {
        float rTop = cat.rect.top - static_cast<float>(m_categoryScrollY);
        float rBottom = cat.rect.bottom - static_cast<float>(m_categoryScrollY);
        if (fx >= cat.rect.left && fx <= cat.rect.right && fy >= rTop && fy <= rBottom) {
            outCat = cat.type;
            return true;
        }
    }
    return false;
}

bool Direct2DRenderer::IsOverTaskList(int x, int y, int& outTaskRowIndex, std::wstring& outTaskId) const {
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);

    if (fx < m_rcTaskListCard.left || fx > m_rcTaskListCard.right ||
        fy < m_rcTaskListCard.top + 26.0f * m_dpiScale || fy > m_rcTaskListCard.bottom) {
        return false;
    }

    for (size_t i = 0; i < m_visibleTaskRowRects.size(); ++i) {
        const auto& r = m_visibleTaskRowRects[i];
        if (fx >= r.left && fx <= r.right && fy >= r.top && fy <= r.bottom) {
            outTaskRowIndex = static_cast<int>(i);
            if (i < m_visibleTaskIds.size()) {
                outTaskId = m_visibleTaskIds[i];
            }
            return true;
        }
    }
    return false;
}

bool Direct2DRenderer::IsOverDetailTabs(int x, int y, DetailTab& outTab) const {
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);

    for (const auto& tab : m_detailTabs) {
        if (fx >= tab.rect.left && fx <= tab.rect.right && fy >= tab.rect.top && fy <= tab.rect.bottom) {
            outTab = tab.tab;
            return true;
        }
    }
    return false;
}

bool Direct2DRenderer::IsOverSplitterV(int x, int y) const {
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);
    return (fx >= m_rcSplitterV.left && fx <= m_rcSplitterV.right && fy >= m_rcSplitterV.top && fy <= m_rcSplitterV.bottom);
}

bool Direct2DRenderer::IsOverSplitterH(int x, int y) const {
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);
    return (fx >= m_rcSplitterH.left && fx <= m_rcSplitterH.right && fy >= m_rcSplitterH.top && fy <= m_rcSplitterH.bottom);
}

bool Direct2DRenderer::IsInDraggableHeader(int x, int y) const {
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);

    if (fy < 0.0f || fy > 38.0f * m_dpiScale) return false;

    // Check if over buttons
    int btnId = 0;
    if (IsOverTopRightButtons(x, y, btnId)) return false;
    if (IsOverActionBar(x, y, btnId)) return false;

    return true;
}

void Direct2DRenderer::ResetHoverStates() {
    m_hoveredActionBtnId = -1;
    m_hoveredTopRightId = -1;
    m_hasHoveredCategory = false;
    m_hasHoveredDetailTab = false;
    m_hoveredTaskIndex = -1;
}

void Direct2DRenderer::OnMouseMove(int x, int y) {
    m_mouseNearTop = (y <= static_cast<int>(50 * m_dpiScale));

    int trId = 0;
    if (IsOverTopRightButtons(x, y, trId)) {
        m_hoveredTopRightId = trId;
    } else {
        m_hoveredTopRightId = -1;
    }

    int actId = 0;
    if (IsOverActionBar(x, y, actId)) {
        m_hoveredActionBtnId = actId;
    } else {
        m_hoveredActionBtnId = -1;
    }

    CategoryFilterType cat;
    if (IsOverCategories(x, y, cat)) {
        m_hoveredCategory = cat;
        m_hasHoveredCategory = true;
    } else {
        m_hasHoveredCategory = false;
    }

    DetailTab tab;
    if (IsOverDetailTabs(x, y, tab)) {
        m_hoveredDetailTab = tab;
        m_hasHoveredDetailTab = true;
    } else {
        m_hasHoveredDetailTab = false;
    }

    int taskRow = -1;
    std::wstring tId;
    if (IsOverTaskList(x, y, taskRow, tId)) {
        m_hoveredTaskIndex = taskRow;
    } else {
        m_hoveredTaskIndex = -1;
    }
}

void Direct2DRenderer::OnLButtonDown(int x, int y) {
    int trId = 0;
    if (IsOverTopRightButtons(x, y, trId)) {
        m_pressedTopRightId = trId;
    }

    int actId = 0;
    if (IsOverActionBar(x, y, actId)) {
        m_pressedActionBtnId = actId;
    }
}

void Direct2DRenderer::OnLButtonUp(int x, int y) {
    m_pressedTopRightId = -1;
    m_pressedActionBtnId = -1;
    m_isDraggingScrollbar = false;
    m_dragScrollArea = ScrollArea::None;
}

void Direct2DRenderer::OnMouseWheel(int x, int y, int delta) {
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);
    int step = static_cast<int>(32 * m_dpiScale);

    // 1. Is mouse over Categories card?
    if (fx >= m_rcCategoryCard.left && fx <= m_rcCategoryCard.right &&
        fy >= m_rcCategoryCard.top && fy <= m_rcCategoryCard.bottom)
    {
        if (delta > 0) {
            m_categoryScrollY = (std::max)(0, m_categoryScrollY - step);
        } else {
            m_categoryScrollY += step;
        }
        return;
    }

    // 2. Is mouse over Detail card?
    if (fx >= m_rcDetailCard.left && fx <= m_rcDetailCard.right &&
        fy >= m_rcDetailCard.top && fy <= m_rcDetailCard.bottom)
    {
        if (delta > 0) {
            m_matrixScrollY = (std::max)(0, m_matrixScrollY - step);
            m_logsScrollY = (std::max)(0, m_logsScrollY - step);
            m_segmentsScrollY = (std::max)(0, m_segmentsScrollY - step);
            m_fileInfoScrollY = (std::max)(0, m_fileInfoScrollY - step);
        } else {
            m_matrixScrollY += step;
            m_logsScrollY += step;
            m_segmentsScrollY += step;
            m_fileInfoScrollY += step;
        }
        return;
    }

    // 3. Otherwise, scroll Task List
    if (delta > 0) {
        m_taskListScrollY = (std::max)(0, m_taskListScrollY - step);
    } else {
        m_taskListScrollY += step;
    }
}

void Direct2DRenderer::DrawModernScrollbar(
    const D2D1_RECT_F& trackRect,
    float contentHeight,
    float viewportHeight,
    float scrollY,
    bool isHovered,
    bool isDragging
) {
    if (!m_pCurrentRT || contentHeight <= viewportHeight || viewportHeight <= 0) return;

    // Background track (subtle dark pill)
    float trackR = (trackRect.right - trackRect.left) / 2.0f;
    D2D1_ROUNDED_RECT trackRR = D2D1::RoundedRect(trackRect, trackR, trackR);
    m_pCurrentRT->FillRoundedRectangle(&trackRR, m_pProgressTrackBrush);

    // Thumb calculation
    float ratio = viewportHeight / contentHeight;
    float thumbH = (std::max)(20.0f * m_dpiScale, viewportHeight * ratio);
    float maxScroll = contentHeight - viewportHeight;
    float scrollRatio = (maxScroll > 0) ? (scrollY / maxScroll) : 0.0f;
    if (scrollRatio < 0.0f) scrollRatio = 0.0f;
    if (scrollRatio > 1.0f) scrollRatio = 1.0f;

    float trackH = trackRect.bottom - trackRect.top;
    float thumbY = trackRect.top + (trackH - thumbH) * scrollRatio;
    D2D1_RECT_F thumbRect = D2D1::RectF(trackRect.left, thumbY, trackRect.right, thumbY + thumbH);
    D2D1_ROUNDED_RECT thumbRR = D2D1::RoundedRect(thumbRect, trackR, trackR);

    ID2D1Brush* thumbBrush = isDragging ? m_pCyanBrush : (isHovered ? m_pMutedTextBrush : m_pDimTextBrush);
    m_pCurrentRT->FillRoundedRectangle(&thumbRR, thumbBrush);
}

// -------------------------------------------------------------
// Drawing Helpers
// -------------------------------------------------------------

void Direct2DRenderer::DrawRoundedPill(const D2D1_RECT_F& rect, ID2D1Brush* fill, ID2D1Brush* stroke, float strokeWidth) {
    if (!m_pCurrentRT) return;
    float r = (rect.bottom - rect.top) / 2.0f;
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, r, r);
    if (fill) m_pCurrentRT->FillRoundedRectangle(&rr, fill);
    if (stroke) m_pCurrentRT->DrawRoundedRectangle(&rr, stroke, strokeWidth);
}

void Direct2DRenderer::DrawTextCentered(const std::wstring& text, const D2D1_RECT_F& rect, IDWriteTextFormat* format, ID2D1Brush* brush) {
    if (!m_pCurrentRT || !format || !brush || text.empty()) return;
    format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    m_pCurrentRT->DrawText(text.c_str(), static_cast<UINT32>(text.length()), format, rect, brush);
}

void Direct2DRenderer::DrawTextLeft(const std::wstring& text, const D2D1_RECT_F& rect, IDWriteTextFormat* format, ID2D1Brush* brush) {
    if (!m_pCurrentRT || !format || !brush || text.empty()) return;
    format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    m_pCurrentRT->DrawText(text.c_str(), static_cast<UINT32>(text.length()), format, rect, brush);
}

void Direct2DRenderer::DrawTextRight(const std::wstring& text, const D2D1_RECT_F& rect, IDWriteTextFormat* format, ID2D1Brush* brush) {
    if (!m_pCurrentRT || !format || !brush || text.empty()) return;
    format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    m_pCurrentRT->DrawText(text.c_str(), static_cast<UINT32>(text.length()), format, rect, brush);
}

// -------------------------------------------------------------
// Rendering Main Function
// -------------------------------------------------------------

void Direct2DRenderer::BeginDraw() {
    CreateDeviceResources();
    m_pCurrentRT = m_pRenderTarget;
    if (m_pCurrentRT) {
        m_pCurrentRT->BeginDraw();
    }
}

HRESULT Direct2DRenderer::EndDraw() {
    if (!m_pCurrentRT) return S_OK;
    HRESULT hr = m_pCurrentRT->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
    return hr;
}

void Direct2DRenderer::RenderAll(
    const std::vector<DownloadTaskInfo>& tasks,
    const std::unordered_set<std::wstring>& selectedTaskIds,
    const std::wstring& selectedTaskId,
    CategoryFilterType activeCategory,
    DetailTab activeTab,
    int splitterX,
    int splitterY,
    bool isSplitterVHovered,
    bool isSplitterHHovered,
    double totalSpeedBps,
    int activeTasksCount,
    uint64_t totalBytesDownloaded,
    uint64_t totalBytesAll
) {
    if (!m_pCurrentRT) return;

    UpdateLayout(m_width, m_height, splitterX, splitterY);

    // 1. Background
    RenderBackground();

    // 2. Top Bar
    RenderTopBar();

    // 3. Category Card
    RenderCategoryCard(tasks, activeCategory);

    // 4. Splitters
    RenderSplitters(isSplitterVHovered, isSplitterHHovered);

    // 5. Task List Card
    RenderTaskListCard(tasks, selectedTaskIds, selectedTaskId, activeCategory);

    // 6. Detail Card
    const DownloadTaskInfo* pSelected = nullptr;
    for (const auto& t : tasks) {
        if (t.id == selectedTaskId) {
            pSelected = &t;
            break;
        }
    }
    RenderDetailCard(pSelected, activeTab);

    // 7. Bottom Status Bar
    RenderBottomBar(totalSpeedBps, activeTasksCount, totalBytesDownloaded, totalBytesAll);
}

void Direct2DRenderer::RenderBackground() {
    D2D1_RECT_F fullRect = D2D1::RectF(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
    m_pCurrentRT->FillRectangle(&fullRect, m_pBgBrush);
    m_pCurrentRT->DrawRectangle(&fullRect, m_pCardBorderBrush, 1.0f);
}

void Direct2DRenderer::RenderTopBar() {
    // 1. Brand: ⚡ GETY
    DrawTextLeft(L"⚡ GETY", m_rcBrand, m_pFormatBold, m_pCyanBrush);

    // Top bar is always visible (user request: "ust menu otomatık saklanmasın sımdılık hep visible olsun")

    // 2. Action Pill Container (GhostView HUD style)
    float pillR = (m_rcActionPill.bottom - m_rcActionPill.top) / 2.0f;
    D2D1_ROUNDED_RECT actionPillRR = D2D1::RoundedRect(m_rcActionPill, pillR, pillR);
    m_pCurrentRT->FillRoundedRectangle(&actionPillRR, m_pCardBrush);
    m_pCurrentRT->DrawRoundedRectangle(&actionPillRR, m_pCardBorderBrush, 1.2f);

    // Action Buttons
    for (const auto& btn : m_actionButtons) {
        float bR = 6.0f * m_dpiScale;
        D2D1_ROUNDED_RECT bRR = D2D1::RoundedRect(btn.rect, bR, bR);

        bool isHov = (btn.id == m_hoveredActionBtnId);
        bool isPress = (btn.id == m_pressedActionBtnId);

        if (isPress) {
            m_pCurrentRT->FillRoundedRectangle(&bRR, m_pPressedBrush);
        } else if (isHov) {
            m_pCurrentRT->FillRoundedRectangle(&bRR, m_pHoverBrush);
        }

        std::wstring fullLabel = (btn.rect.right - btn.rect.left < 34.0f * m_dpiScale) ? btn.icon : (btn.icon + L" " + btn.label);
        m_pCurrentRT->PushAxisAlignedClip(&btn.rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        DrawTextCentered(fullLabel, btn.rect, m_pFormatTopBtn, isHov ? m_pTextBrush : m_pMutedTextBrush);
        m_pCurrentRT->PopAxisAlignedClip();
    }

    // 3. Top-Right Window Buttons (GhostView circular rounded style)
    auto drawTopBtn = [&](const Direct2DTopButton& btn, bool isClose) {
        float r = (btn.rect.bottom - btn.rect.top) / 2.0f;
        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(btn.rect, r, r);

        bool isHov = (btn.id == m_hoveredTopRightId);
        bool isPress = (btn.id == m_pressedTopRightId);

        if (isClose && isHov) {
            m_pCurrentRT->FillRoundedRectangle(&rr, m_pRedBrush);
        } else if (isPress) {
            m_pCurrentRT->FillRoundedRectangle(&rr, m_pPressedBrush);
        } else if (isHov) {
            m_pCurrentRT->FillRoundedRectangle(&rr, m_pHoverBrush);
        } else {
            m_pCurrentRT->FillRoundedRectangle(&rr, m_pCardBrush);
        }
        m_pCurrentRT->DrawRoundedRectangle(&rr, m_pCardBorderBrush, 1.0f);

        DrawTextCentered(btn.icon, btn.rect, m_pFormatTopBtn, m_pTextBrush);
    };

    drawTopBtn(m_btnMin, false);
    drawTopBtn(m_btnMax, false);
    drawTopBtn(m_btnClose, true);
}

void Direct2DRenderer::RenderCategoryCard(const std::vector<DownloadTaskInfo>& tasks, CategoryFilterType activeCategory) {
    D2D1_ROUNDED_RECT cardRR = D2D1::RoundedRect(m_rcCategoryCard, 8.0f * m_dpiScale, 8.0f * m_dpiScale);
    m_pCurrentRT->FillRoundedRectangle(&cardRR, m_pCardBrush);
    m_pCurrentRT->DrawRoundedRectangle(&cardRR, m_pCardBorderBrush, 1.0f);

    D2D1_RECT_F headerRect = D2D1::RectF(
        m_rcCategoryCard.left + 10.0f * m_dpiScale,
        m_rcCategoryCard.top + 6.0f * m_dpiScale,
        m_rcCategoryCard.right - 10.0f * m_dpiScale,
        m_rcCategoryCard.top + 24.0f * m_dpiScale
    );
    bool isTr = (I18n::Instance().GetCurrentLanguage() == LangId::Turkish);
    DrawTextLeft(isTr ? L"📁 KATEGORİLER" : L"📁 CATEGORIES", headerRect, m_pFormatSemiBold, m_pMutedTextBrush);

    int countAll = static_cast<int>(tasks.size());
    int countUnfinished = 0;
    int countDownloading = 0, countPaused = 0, countCompleted = 0, countTrash = 0;
    int countMusic = 0, countVideo = 0, countSoftware = 0, countDocs = 0, countAI = 0;

    for (const auto& t : tasks) {
        if (t.state != DownloadState::Completed && t.state != DownloadState::Deleted) countUnfinished++;
        if (t.state == DownloadState::Downloading || t.state == DownloadState::Connecting) countDownloading++;
        else if (t.state == DownloadState::Paused) countPaused++;
        else if (t.state == DownloadState::Completed) countCompleted++;
        else if (t.state == DownloadState::Failed) countTrash++;

        if (t.category == L"Müzik" || t.filename.ends_with(L".mp3") || t.filename.ends_with(L".flac") || t.filename.ends_with(L".wav")) countMusic++;
        else if (t.category == L"Video" || t.filename.ends_with(L".mp4") || t.filename.ends_with(L".mkv") || t.filename.ends_with(L".avi")) countVideo++;
        else if (t.category == L"Yazılım" || t.filename.ends_with(L".exe") || t.filename.ends_with(L".msi") || t.filename.ends_with(L".zip")) countSoftware++;
        else if (t.category == L"Belgeler" || t.filename.ends_with(L".pdf") || t.filename.ends_with(L".doc") || t.filename.ends_with(L".txt")) countDocs++;
        else if (t.category == LStr(StrId::CatAI) || t.category == L"Yapay Zeka" || t.category == L"AI" || t.filename.ends_with(L".gguf") || t.filename.ends_with(L".safetensors") || t.filename.ends_with(L".onnx")) countAI++;
    }

    D2D1_RECT_F catClipRect = D2D1::RectF(
        m_rcCategoryCard.left + 2.0f * m_dpiScale,
        m_rcCategoryCard.top + 26.0f * m_dpiScale,
        m_rcCategoryCard.right - 2.0f * m_dpiScale,
        m_rcCategoryCard.bottom - 4.0f * m_dpiScale
    );

    float totalCatHeight = m_categoryDefs.size() * (26.0f * m_dpiScale);
    float catViewportH = catClipRect.bottom - catClipRect.top;
    float maxCatScroll = (std::max)(0.0f, totalCatHeight - catViewportH);
    if (m_categoryScrollY > maxCatScroll) m_categoryScrollY = static_cast<int>(maxCatScroll);
    if (m_categoryScrollY < 0) m_categoryScrollY = 0;

    m_pCurrentRT->PushAxisAlignedClip(&catClipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    for (const auto& item : m_categoryDefs) {
        float itemTop = item.rect.top - static_cast<float>(m_categoryScrollY);
        float itemBottom = item.rect.bottom - static_cast<float>(m_categoryScrollY);
        D2D1_RECT_F itemR_scrolled = D2D1::RectF(item.rect.left, itemTop, item.rect.right, itemBottom);

        if (itemBottom < catClipRect.top || itemTop > catClipRect.bottom) continue;

        bool isAct = (item.type == activeCategory);
        bool isHov = (m_hasHoveredCategory && item.type == m_hoveredCategory);

        float itemR = 5.0f * m_dpiScale;
        D2D1_ROUNDED_RECT itemRR = D2D1::RoundedRect(itemR_scrolled, itemR, itemR);

        if (isAct) {
            m_pCurrentRT->FillRoundedRectangle(&itemRR, m_pActivePillBrush);
            m_pCurrentRT->DrawRoundedRectangle(&itemRR, m_pCyanBrush, 1.0f);
        } else if (isHov) {
            m_pCurrentRT->FillRoundedRectangle(&itemRR, m_pHoverBrush);
        }

        D2D1_RECT_F textR = D2D1::RectF(itemR_scrolled.left + 8.0f * m_dpiScale, itemR_scrolled.top, itemR_scrolled.right - 28.0f * m_dpiScale, itemR_scrolled.bottom);
        std::wstring itemText = item.icon.empty() ? item.label : (item.icon + L" " + item.label);
        DrawTextLeft(itemText, textR, m_pFormatRegular, isAct ? m_pCyanBrush : m_pTextBrush);

        int cnt = 0;
        switch (item.type) {
            case CategoryFilterType::All: cnt = countAll; break;
            case CategoryFilterType::Unfinished: cnt = countUnfinished; break;
            case CategoryFilterType::Downloading: cnt = countDownloading; break;
            case CategoryFilterType::Paused: cnt = countPaused; break;
            case CategoryFilterType::Downloaded: cnt = countCompleted; break;
            case CategoryFilterType::Trash: cnt = countTrash; break;
            case CategoryFilterType::Music: cnt = countMusic; break;
            case CategoryFilterType::Video: cnt = countVideo; break;
            case CategoryFilterType::Software: cnt = countSoftware; break;
            case CategoryFilterType::Documents: cnt = countDocs; break;
            case CategoryFilterType::AI: cnt = countAI; break;
            default: cnt = 0; break;
        }

        if (cnt > 0) {
            D2D1_RECT_F badgeR = D2D1::RectF(itemR_scrolled.right - 24.0f * m_dpiScale, itemR_scrolled.top + 5.0f * m_dpiScale, itemR_scrolled.right - 6.0f * m_dpiScale, itemR_scrolled.bottom - 5.0f * m_dpiScale);
            float bRadius = (badgeR.bottom - badgeR.top) / 2.0f;
            D2D1_ROUNDED_RECT badgeRR = D2D1::RoundedRect(badgeR, bRadius, bRadius);
            m_pCurrentRT->FillRoundedRectangle(&badgeRR, isAct ? m_pCyanGlowBrush : m_pHeaderBrush);
            DrawTextCentered(std::to_wstring(cnt), badgeR, m_pFormatSmall, isAct ? m_pCyanBrush : m_pMutedTextBrush);
        }
    }

    m_pCurrentRT->PopAxisAlignedClip();

    // Draw sleek scrollbar if needed
    if (totalCatHeight > catViewportH && catViewportH > 0) {
        D2D1_RECT_F sbTrack = D2D1::RectF(
            m_rcCategoryCard.right - 6.0f * m_dpiScale,
            catClipRect.top + 2.0f * m_dpiScale,
            m_rcCategoryCard.right - 2.0f * m_dpiScale,
            catClipRect.bottom - 2.0f * m_dpiScale
        );
        DrawModernScrollbar(sbTrack, totalCatHeight, catViewportH, static_cast<float>(m_categoryScrollY), false, false);
    }
}

void Direct2DRenderer::RenderSplitters(bool isVHovered, bool isHHovered) {
    m_pCurrentRT->FillRectangle(&m_rcSplitterV, isVHovered ? m_pSplitterHoverBrush : m_pBgBrush);
    m_pCurrentRT->FillRectangle(&m_rcSplitterH, isHHovered ? m_pSplitterHoverBrush : m_pBgBrush);
}

void Direct2DRenderer::RenderTaskListCard(
    const std::vector<DownloadTaskInfo>& tasks,
    const std::unordered_set<std::wstring>& selectedTaskIds,
    const std::wstring& selectedTaskId,
    CategoryFilterType activeCategory
) {
    std::vector<const DownloadTaskInfo*> filteredTasks;
    for (const auto& t : tasks) {
        bool match = false;
        switch (activeCategory) {
            case CategoryFilterType::All: match = true; break;
            case CategoryFilterType::Unfinished: match = (t.state != DownloadState::Completed && t.state != DownloadState::Deleted); break;
            case CategoryFilterType::Downloading: match = (t.state == DownloadState::Downloading || t.state == DownloadState::Connecting); break;
            case CategoryFilterType::Paused: match = (t.state == DownloadState::Paused); break;
            case CategoryFilterType::Downloaded: match = (t.state == DownloadState::Completed); break;
            case CategoryFilterType::Trash: match = (t.state == DownloadState::Failed); break;
            case CategoryFilterType::Music: match = (t.category == L"Müzik" || t.filename.ends_with(L".mp3") || t.filename.ends_with(L".flac")); break;
            case CategoryFilterType::Video: match = (t.category == L"Video" || t.filename.ends_with(L".mp4") || t.filename.ends_with(L".mkv")); break;
            case CategoryFilterType::Software: match = (t.category == L"Yazılım" || t.filename.ends_with(L".exe") || t.filename.ends_with(L".zip")); break;
            case CategoryFilterType::Documents: match = (t.category == L"Belgeler" || t.filename.ends_with(L".pdf") || t.filename.ends_with(L".txt")); break;
            case CategoryFilterType::AI: match = (t.category == LStr(StrId::CatAI) || t.category == L"Yapay Zeka" || t.category == L"AI" || t.filename.ends_with(L".gguf") || t.filename.ends_with(L".safetensors") || t.filename.ends_with(L".onnx")); break;
            default: match = true; break;
        }
        if (match) filteredTasks.push_back(&t);
    }

    D2D1_ROUNDED_RECT cardRR = D2D1::RoundedRect(m_rcTaskListCard, 8.0f * m_dpiScale, 8.0f * m_dpiScale);
    m_pCurrentRT->FillRoundedRectangle(&cardRR, m_pCardBrush);
    m_pCurrentRT->DrawRoundedRectangle(&cardRR, m_pCardBorderBrush, 1.0f);

    m_pCurrentRT->PushAxisAlignedClip(&m_rcTaskListCard, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    float headerH = 28.0f * m_dpiScale;
    D2D1_RECT_F headerR = D2D1::RectF(m_rcTaskListCard.left, m_rcTaskListCard.top, m_rcTaskListCard.right, m_rcTaskListCard.top + headerH);
    m_pCurrentRT->FillRectangle(&headerR, m_pHeaderBrush);

    float cardW = m_rcTaskListCard.right - m_rcTaskListCard.left;
    float colSizeW = 115.0f * m_dpiScale;
    float colProgW = 140.0f * m_dpiScale;
    float colSpeedW = 90.0f * m_dpiScale;
    float colEtaW = 85.0f * m_dpiScale;
    float colStatusW = 95.0f * m_dpiScale;
    float fixedW = colSizeW + colProgW + colSpeedW + colEtaW + colStatusW;

    float colFileW = (std::max)(130.0f * m_dpiScale, cardW - fixedW - 20.0f * m_dpiScale);

    float curColX = m_rcTaskListCard.left + 8.0f * m_dpiScale;
    auto drawColHeader = [&](const std::wstring& title, float w) {
        D2D1_RECT_F colR = D2D1::RectF(curColX, headerR.top, curColX + w, headerR.bottom);
        DrawTextLeft(title, colR, m_pFormatSmall, m_pMutedTextBrush);
        curColX += w;
    };

    drawColHeader(LStr(StrId::ColFilename), colFileW);
    drawColHeader(LStr(StrId::ColSize), colSizeW);
    drawColHeader(LStr(StrId::ColProgress), colProgW);
    drawColHeader(LStr(StrId::ColSpeed), colSpeedW);
    drawColHeader(LStr(StrId::ColTimeLeft), colEtaW);
    drawColHeader(LStr(StrId::ColStatus), colStatusW);

    D2D1_POINT_2F p0 = D2D1::Point2F(m_rcTaskListCard.left, headerR.bottom);
    D2D1_POINT_2F p1 = D2D1::Point2F(m_rcTaskListCard.right, headerR.bottom);
    m_pCurrentRT->DrawLine(p0, p1, m_pCardBorderBrush, 1.0f);

    m_visibleTaskIds.clear();
    m_visibleTaskRowRects.clear();

    float rowH = 30.0f * m_dpiScale;
    float rowY = headerR.bottom + 2.0f * m_dpiScale - static_cast<float>(m_taskListScrollY);

    for (size_t i = 0; i < filteredTasks.size(); ++i) {
        const auto* pT = filteredTasks[i];
        D2D1_RECT_F rowR = D2D1::RectF(m_rcTaskListCard.left + 4.0f * m_dpiScale, rowY, m_rcTaskListCard.right - 4.0f * m_dpiScale, rowY + rowH);

        m_visibleTaskIds.push_back(pT->id);
        m_visibleTaskRowRects.push_back(rowR);

        if (rowR.bottom >= headerR.bottom && rowR.top <= m_rcTaskListCard.bottom) {
            bool isSelected = (selectedTaskIds.find(pT->id) != selectedTaskIds.end() || pT->id == selectedTaskId);
            bool isHov = (static_cast<int>(i) == m_hoveredTaskIndex);

            D2D1_ROUNDED_RECT rowRR = D2D1::RoundedRect(rowR, 4.0f * m_dpiScale, 4.0f * m_dpiScale);
            if (isSelected) {
                m_pCurrentRT->FillRoundedRectangle(&rowRR, m_pActivePillBrush);
                m_pCurrentRT->DrawRoundedRectangle(&rowRR, m_pCyanBrush, 1.0f);
            } else if (isHov) {
                m_pCurrentRT->FillRoundedRectangle(&rowRR, m_pHoverBrush);
            }

            float cx = rowR.left + 6.0f * m_dpiScale;
            D2D1_RECT_F iconR = D2D1::RectF(cx, rowR.top, cx + 18.0f * m_dpiScale, rowR.bottom);
            DrawTextCentered(L"📄", iconR, m_pFormatIcons, m_pCyanBrush);
            cx += 22.0f * m_dpiScale;

            D2D1_RECT_F nameR = D2D1::RectF(cx, rowR.top, rowR.left + 8.0f * m_dpiScale + colFileW - 6.0f * m_dpiScale, rowR.bottom);
            m_pCurrentRT->PushAxisAlignedClip(&nameR, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            DrawTextLeft(pT->filename, nameR, m_pFormatMedium, m_pTextBrush);
            m_pCurrentRT->PopAxisAlignedClip();
            cx = rowR.left + 8.0f * m_dpiScale + colFileW;

            D2D1_RECT_F sizeR = D2D1::RectF(cx, rowR.top, cx + colSizeW - 4.0f * m_dpiScale, rowR.bottom);
            wchar_t szBuf[64];
            if (pT->totalBytes >= 1024ULL * 1024 * 1024) {
                double dlGB = (double)pT->downloadedBytes / (1024.0 * 1024.0 * 1024.0);
                double totGB = (double)pT->totalBytes / (1024.0 * 1024.0 * 1024.0);
                swprintf_s(szBuf, L"%.2f / %.2f GB", dlGB, totGB);
            } else if (pT->totalBytes > 0) {
                double dlMB = (double)pT->downloadedBytes / (1024.0 * 1024.0);
                double totMB = (double)pT->totalBytes / (1024.0 * 1024.0);
                swprintf_s(szBuf, L"%.1f / %.1f MB", dlMB, totMB);
            } else {
                double dlMB = (double)pT->downloadedBytes / (1024.0 * 1024.0);
                swprintf_s(szBuf, L"%.1f MB", dlMB);
            }
            m_pCurrentRT->PushAxisAlignedClip(&sizeR, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            DrawTextLeft(szBuf, sizeR, m_pFormatRegular, m_pMutedTextBrush);
            m_pCurrentRT->PopAxisAlignedClip();
            cx += colSizeW;

            // Side-by-side Progress Bar + Bold Percentage Text (Never clipped!)
            float pctW = 46.0f * m_dpiScale;
            float barW = colProgW - pctW - 8.0f * m_dpiScale;
            float progH = 8.0f * m_dpiScale;
            float progY = rowR.top + (rowH - progH) / 2.0f;
            D2D1_RECT_F progTrackR = D2D1::RectF(cx, progY, cx + barW, progY + progH);
            float progRadius = progH / 2.0f;
            D2D1_ROUNDED_RECT progTrackRR = D2D1::RoundedRect(progTrackR, progRadius, progRadius);
            m_pCurrentRT->FillRoundedRectangle(&progTrackRR, m_pProgressTrackBrush);

            double frac = (pT->totalBytes > 0) ? ((double)pT->downloadedBytes / (double)pT->totalBytes) : 0.0;
            if (frac > 1.0) frac = 1.0;
            if (frac < 0.0) frac = 0.0;

            if (frac > 0.001) {
                float fillW = (progTrackR.right - progTrackR.left) * static_cast<float>(frac);
                D2D1_RECT_F progFillR = D2D1::RectF(progTrackR.left, progTrackR.top, progTrackR.left + fillW, progTrackR.bottom);
                D2D1_ROUNDED_RECT progFillRR = D2D1::RoundedRect(progFillR, progRadius, progRadius);
                m_pCurrentRT->FillRoundedRectangle(&progFillRR, (pT->state == DownloadState::Completed) ? m_pGreenBrush : m_pCyanBrush);
            }

            wchar_t pctBuf[16];
            swprintf_s(pctBuf, L"%%%0.1f", frac * 100.0);
            D2D1_RECT_F pctR = D2D1::RectF(cx + barW + 4.0f * m_dpiScale, rowR.top, cx + colProgW - 2.0f * m_dpiScale, rowR.bottom);
            ID2D1Brush* pctBrush = (pT->state == DownloadState::Completed) ? m_pGreenBrush : 
                                   ((pT->state == DownloadState::Downloading) ? m_pCyanBrush : m_pMutedTextBrush);
            DrawTextLeft(pctBuf, pctR, m_pFormatSemiBold, pctBrush);
            cx += colProgW;

            D2D1_RECT_F speedR = D2D1::RectF(cx, rowR.top, cx + colSpeedW, rowR.bottom);
            if (pT->state == DownloadState::Downloading && pT->currentSpeedBps > 10.0) {
                double speedMB = pT->currentSpeedBps / (1024.0 * 1024.0);
                wchar_t spdBuf[32];
                swprintf_s(spdBuf, L"%.2f MB/s", speedMB);
                DrawTextLeft(spdBuf, speedR, m_pFormatSemiBold, m_pCyanBrush);
            } else {
                DrawTextLeft(L"-", speedR, m_pFormatRegular, m_pDimTextBrush);
            }
            cx += colSpeedW;

            D2D1_RECT_F etaR = D2D1::RectF(cx, rowR.top, cx + colEtaW, rowR.bottom);
            if (pT->state == DownloadState::Downloading && pT->currentSpeedBps > 100.0 && pT->totalBytes > pT->downloadedBytes) {
                uint64_t rem = pT->totalBytes - pT->downloadedBytes;
                uint64_t sec = static_cast<uint64_t>(rem / pT->currentSpeedBps);
                int h = (int)(sec / 3600);
                int m = (int)((sec % 3600) / 60);
                int s = (int)(sec % 60);
                wchar_t etaBuf[32];
                swprintf_s(etaBuf, L"%02d:%02d:%02d", h, m, s);
                DrawTextLeft(etaBuf, etaR, m_pFormatRegular, m_pMutedTextBrush);
            } else {
                DrawTextLeft(L"-", etaR, m_pFormatRegular, m_pDimTextBrush);
            }
            cx += colEtaW;

            float badgeH = 17.0f * m_dpiScale;
            float badgeY = rowR.top + (rowH - badgeH) / 2.0f;
            D2D1_RECT_F badgeR = D2D1::RectF(cx, badgeY, cx + 78.0f * m_dpiScale, badgeY + badgeH);
            float bRadius = badgeH / 2.0f;
            D2D1_ROUNDED_RECT badgeRR = D2D1::RoundedRect(badgeR, bRadius, bRadius);

            if (pT->state == DownloadState::Completed) {
                m_pCurrentRT->FillRoundedRectangle(&badgeRR, m_pGreenGlowBrush);
                DrawTextCentered(LStr(StrId::StateCompleted), badgeR, m_pFormatSmall, m_pGreenBrush);
            } else if (pT->state == DownloadState::Downloading) {
                m_pCurrentRT->FillRoundedRectangle(&badgeRR, m_pCyanGlowBrush);
                DrawTextCentered(LStr(StrId::StateDownloading), badgeR, m_pFormatSmall, m_pCyanBrush);
            } else if (pT->state == DownloadState::Paused) {
                m_pCurrentRT->FillRoundedRectangle(&badgeRR, m_pHeaderBrush);
                DrawTextCentered(LStr(StrId::StatePaused), badgeR, m_pFormatSmall, m_pAmberBrush);
            } else if (pT->state == DownloadState::Failed) {
                m_pCurrentRT->FillRoundedRectangle(&badgeRR, m_pHeaderBrush);
                DrawTextCentered(LStr(StrId::StateFailed), badgeR, m_pFormatSmall, m_pRedBrush);
            } else {
                m_pCurrentRT->FillRoundedRectangle(&badgeRR, m_pHeaderBrush);
                DrawTextCentered(LStr(StrId::StateQueued), badgeR, m_pFormatSmall, m_pMutedTextBrush);
            }
        }
        rowY += rowH + 1.0f * m_dpiScale;
    }

    if (filteredTasks.empty()) {
        D2D1_RECT_F emptyR = D2D1::RectF(
            m_rcTaskListCard.left + 20.0f * m_dpiScale,
            headerR.bottom + 40.0f * m_dpiScale,
            m_rcTaskListCard.right - 20.0f * m_dpiScale,
            headerR.bottom + 80.0f * m_dpiScale
        );
        DrawTextCentered(L"İndirme listesi boş. Yeni bir indirme başlatmak için üstteki ＋ Yeni butonuna tıklayın.", emptyR, m_pFormatMedium, m_pDimTextBrush);
    }

    float totalTaskHeight = filteredTasks.size() * (rowH + 1.0f * m_dpiScale);
    float taskViewportH = m_rcTaskListCard.bottom - headerR.bottom - 4.0f * m_dpiScale;
    float maxTaskScroll = (std::max)(0.0f, totalTaskHeight - taskViewportH);
    if (m_taskListScrollY > maxTaskScroll) m_taskListScrollY = static_cast<int>(maxTaskScroll);
    if (m_taskListScrollY < 0) m_taskListScrollY = 0;

    if (totalTaskHeight > taskViewportH && taskViewportH > 0) {
        D2D1_RECT_F sbTrack = D2D1::RectF(
            m_rcTaskListCard.right - 6.0f * m_dpiScale,
            headerR.bottom + 4.0f * m_dpiScale,
            m_rcTaskListCard.right - 2.0f * m_dpiScale,
            m_rcTaskListCard.bottom - 4.0f * m_dpiScale
        );
        DrawModernScrollbar(sbTrack, totalTaskHeight, taskViewportH, static_cast<float>(m_taskListScrollY), false, false);
    }

    m_pCurrentRT->PopAxisAlignedClip();
}

void Direct2DRenderer::RenderDetailCard(const DownloadTaskInfo* pSelectedTask, DetailTab activeTab) {
    D2D1_ROUNDED_RECT cardRR = D2D1::RoundedRect(m_rcDetailCard, 8.0f * m_dpiScale, 8.0f * m_dpiScale);
    m_pCurrentRT->FillRoundedRectangle(&cardRR, m_pCardBrush);
    m_pCurrentRT->DrawRoundedRectangle(&cardRR, m_pCardBorderBrush, 1.0f);

    for (const auto& tab : m_detailTabs) {
        bool isAct = (tab.tab == activeTab);
        bool isHov = (m_hasHoveredDetailTab && tab.tab == m_hoveredDetailTab);

        float tabR = 4.0f * m_dpiScale;
        D2D1_ROUNDED_RECT tabRR = D2D1::RoundedRect(tab.rect, tabR, tabR);

        if (isAct) {
            m_pCurrentRT->FillRoundedRectangle(&tabRR, m_pActivePillBrush);
            m_pCurrentRT->DrawRoundedRectangle(&tabRR, m_pCyanBrush, 1.0f);
        } else if (isHov) {
            m_pCurrentRT->FillRoundedRectangle(&tabRR, m_pHoverBrush);
        }

        std::wstring fullLabel = tab.icon + L" " + tab.label;
        DrawTextCentered(fullLabel, tab.rect, isAct ? m_pFormatSemiBold : m_pFormatMedium, isAct ? m_pCyanBrush : m_pMutedTextBrush);
    }

    D2D1_RECT_F contentR = D2D1::RectF(
        m_rcDetailCard.left + 10.0f * m_dpiScale,
        m_rcDetailCard.top + 32.0f * m_dpiScale,
        m_rcDetailCard.right - 10.0f * m_dpiScale,
        m_rcDetailCard.bottom - 6.0f * m_dpiScale
    );

    if (!pSelectedTask) {
        DrawTextCentered(L"İncelemek için yukarıdaki listeden bir görev seçin", contentR, m_pFormatMedium, m_pDimTextBrush);
        return;
    }

    switch (activeTab) {
        case DetailTab::Matrix:
            RenderChunkMatrix(*pSelectedTask, contentR);
            break;
        case DetailTab::Segments:
            RenderSegments(*pSelectedTask, contentR);
            break;
        case DetailTab::Logs:
            RenderLogs(*pSelectedTask, contentR);
            break;
        case DetailTab::Info:
            RenderFileInfo(*pSelectedTask, contentR);
            break;
    }
}

void Direct2DRenderer::RenderChunkMatrix(const DownloadTaskInfo& task, const D2D1_RECT_F& contentRect) {
    float totalW = contentRect.right - contentRect.left;
    float totalH = contentRect.bottom - contentRect.top;
    if (totalW <= 40.0f || totalH <= 50.0f) return;

    float statsH = 22.0f * m_dpiScale;
    float legendH = 20.0f * m_dpiScale;
    float gridTop = contentRect.top + statsH + 4.0f * m_dpiScale;
    float gridBottom = contentRect.bottom - legendH - 2.0f * m_dpiScale;
    float gridH = gridBottom - gridTop;
    if (gridH <= 20.0f) return;

    // Block configuration: Crisp square blocks
    float blockSize = 16.0f * m_dpiScale;
    float spacing = 2.5f * m_dpiScale;
    float availableW = totalW - 16.0f * m_dpiScale; // margin for scrollbar
    int cols = static_cast<int>((availableW + spacing) / (blockSize + spacing));
    if (cols < 8) cols = 8;

    // Total blocks: scale with number of columns to fill at least 8 rows
    int totalBlocks = cols * 8;
    if (task.totalBytes > 100ULL * 1024ULL * 1024ULL) {
        totalBlocks = cols * 12;
    } else if (task.totalBytes > 500ULL * 1024ULL * 1024ULL) {
        totalBlocks = cols * 16;
    }

    uint64_t bytesPerBlock = 1;
    if (task.totalBytes > 0) {
        bytesPerBlock = (task.totalBytes + totalBlocks - 1) / totalBlocks;
        if (bytesPerBlock == 0) bytesPerBlock = 1;
    }

    // First pass: count finished and active blocks
    int doneCount = 0;
    int activeCount = 0;

    if (task.state == DownloadState::Completed) {
        doneCount = totalBlocks;
    } else if (task.totalBytes > 0) {
        for (int i = 0; i < totalBlocks; ++i) {
            uint64_t bStart = i * bytesPerBlock;
            uint64_t bEnd = (std::min)(task.totalBytes - 1, bStart + bytesPerBlock - 1);
            for (const auto& seg : task.segments) {
                uint64_t segCur = seg.startByte + seg.downloadedBytes;
                if (bEnd < segCur) {
                    doneCount++;
                    break;
                } else if (bStart < segCur && bEnd >= segCur) {
                    activeCount++;
                    break;
                } else if (seg.status == 2 && bStart >= segCur && bStart <= seg.endByte) {
                    if (bStart < segCur + bytesPerBlock * 2) {
                        activeCount++;
                        break;
                    }
                }
            }
        }
    }

    // 1. Top Stats Bar
    D2D1_RECT_F statsLeftR = D2D1::RectF(contentRect.left + 4.0f * m_dpiScale, contentRect.top,
                                         contentRect.left + totalW * 0.5f, contentRect.top + statsH);
    D2D1_RECT_F statsRightR = D2D1::RectF(contentRect.left + totalW * 0.45f, contentRect.top,
                                          contentRect.right - 8.0f * m_dpiScale, contentRect.top + statsH);

    bool isTr = (I18n::Instance().GetCurrentLanguage() == LangId::Turkish);
    wchar_t statsLeftBuf[128];
    if (task.totalBytes > 0) {
        swprintf_s(statsLeftBuf, isTr ? L"■ %d Blok  (1 Blok = %s)" : L"■ %d Blocks  (1 Block = %s)", totalBlocks, FormatBytes(bytesPerBlock).c_str());
    } else {
        swprintf_s(statsLeftBuf, isTr ? L"■ %d Blok  (Boyut hesaplanıyor...)" : L"■ %d Blocks  (Calculating size...)", totalBlocks);
    }
    DrawTextLeft(statsLeftBuf, statsLeftR, m_pFormatSmall, m_pMutedTextBrush);

    wchar_t statsRightBuf[128];
    double pct = (totalBlocks > 0) ? ((double)doneCount * 100.0 / totalBlocks) : 0.0;
    swprintf_s(statsRightBuf, isTr ? L"Biten: %d/%d (%%%0.1f)  •  %d Aktif Parça" : L"Done: %d/%d (%.1f%%)  •  %d Active Parts", doneCount, totalBlocks, pct, activeCount);
    DrawTextRight(statsRightBuf, statsRightR, m_pFormatSmall, m_pCyanBrush);

    // Separator line
    m_pCurrentRT->DrawLine(
        D2D1::Point2F(contentRect.left, contentRect.top + statsH + 1.0f * m_dpiScale),
        D2D1::Point2F(contentRect.right, contentRect.top + statsH + 1.0f * m_dpiScale),
        m_pCardBorderBrush, 1.0f
    );

    // 2. Viewport & Scrolling for Blocks
    int rows = (totalBlocks + cols - 1) / cols;
    float totalContentH = rows * (blockSize + spacing) + 8.0f * m_dpiScale;
    float maxScroll = (std::max)(0.0f, totalContentH - gridH);
    if (m_matrixScrollY > maxScroll) m_matrixScrollY = static_cast<int>(maxScroll);
    if (m_matrixScrollY < 0) m_matrixScrollY = 0;

    D2D1_RECT_F gridClip = D2D1::RectF(contentRect.left, gridTop, contentRect.right - 10.0f * m_dpiScale, gridBottom);
    m_pCurrentRT->PushAxisAlignedClip(&gridClip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    float startY = gridTop + 3.0f * m_dpiScale - static_cast<float>(m_matrixScrollY);

    for (int r = 0; r < rows; ++r) {
        float by = startY + r * (blockSize + spacing);
        if (by + blockSize < gridTop || by > gridBottom) continue; // Culling

        for (int c = 0; c < cols; ++c) {
            int blockIdx = r * cols + c;
            if (blockIdx >= totalBlocks) break;

            float bx = contentRect.left + 4.0f * m_dpiScale + c * (blockSize + spacing);
            D2D1_RECT_F bRect = D2D1::RectF(bx, by, bx + blockSize, by + blockSize);
            D2D1_ROUNDED_RECT bRR = D2D1::RoundedRect(bRect, 2.5f, 2.5f);

            bool isDone = false;
            bool isActive = false;

            if (task.state == DownloadState::Completed) {
                isDone = true;
            } else if (task.totalBytes > 0) {
                uint64_t bStart = blockIdx * bytesPerBlock;
                uint64_t bEnd = (std::min)(task.totalBytes - 1, bStart + bytesPerBlock - 1);
                for (const auto& seg : task.segments) {
                    uint64_t segCur = seg.startByte + seg.downloadedBytes;
                    if (bEnd < segCur) {
                        isDone = true;
                        break;
                    } else if (bStart < segCur && bEnd >= segCur) {
                        isActive = true;
                        break;
                    } else if (seg.status == 2 && bStart >= segCur && bStart <= seg.endByte) {
                        if (bStart < segCur + bytesPerBlock * 2) {
                            isActive = true;
                            break;
                        }
                    }
                }
            }

            ID2D1Brush* fill = isDone ? m_pGreenBrush : (isActive ? m_pCyanBrush : m_pPendingBlockBrush);
            m_pCurrentRT->FillRoundedRectangle(&bRR, fill);

            if (isDone) {
                m_pCurrentRT->DrawRoundedRectangle(&bRR, m_pGreenGlowBrush, 1.0f);
            } else if (isActive) {
                m_pCurrentRT->DrawRoundedRectangle(&bRR, m_pCyanGlowBrush, 1.0f);
            } else {
                m_pCurrentRT->DrawRoundedRectangle(&bRR, m_pCardBorderBrush, 0.8f);
            }
        }
    }

    m_pCurrentRT->PopAxisAlignedClip();

    // Scrollbar if needed
    if (totalContentH > gridH) {
        D2D1_RECT_F sbTrack = D2D1::RectF(contentRect.right - 8.0f * m_dpiScale, gridTop,
                                          contentRect.right - 2.0f * m_dpiScale, gridBottom);
        DrawModernScrollbar(sbTrack, totalContentH, gridH, static_cast<float>(m_matrixScrollY), false, false);
    }

    // 3. Bottom Legend Bar
    float legY = contentRect.bottom - legendH;
    auto drawLegendItem = [&](float x, ID2D1Brush* color, const std::wstring& label) {
        float dotSize = 8.0f * m_dpiScale;
        float dotY = legY + (legendH - dotSize) / 2.0f;
        D2D1_RECT_F dotR = D2D1::RectF(x, dotY, x + dotSize, dotY + dotSize);
        D2D1_ROUNDED_RECT dotRR = D2D1::RoundedRect(dotR, 2.0f, 2.0f);
        m_pCurrentRT->FillRoundedRectangle(&dotRR, color);
        D2D1_RECT_F textR = D2D1::RectF(x + dotSize + 6.0f * m_dpiScale, legY, x + 150.0f * m_dpiScale, legY + legendH);
        DrawTextLeft(label, textR, m_pFormatSmall, m_pMutedTextBrush);
    };

    float legX = contentRect.left + 4.0f * m_dpiScale;
    drawLegendItem(legX, m_pGreenBrush, isTr ? L"Tamamlandı" : L"Completed");
    drawLegendItem(legX + 120.0f * m_dpiScale, m_pCyanBrush, isTr ? L"İndiriliyor (Aktif)" : L"Downloading (Active)");
    drawLegendItem(legX + 265.0f * m_dpiScale, m_pPendingBlockBrush, isTr ? L"Bekliyor (Kuyrukta)" : L"Pending (Queued)");
}

void Direct2DRenderer::RenderSegments(const DownloadTaskInfo& task, const D2D1_RECT_F& contentRect) {
    if (task.segments.empty()) {
        DrawTextCentered(L"Parça bilgisi bulunamadı", contentRect, m_pFormatRegular, m_pDimTextBrush);
        return;
    }

    m_pCurrentRT->PushAxisAlignedClip(&contentRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    float itemH = 26.0f * m_dpiScale;
    float totalContentHeight = task.segments.size() * itemH;
    float viewportHeight = contentRect.bottom - contentRect.top;
    float maxSegScroll = (std::max)(0.0f, totalContentHeight - viewportHeight);
    if (m_segmentsScrollY > maxSegScroll) m_segmentsScrollY = static_cast<int>(maxSegScroll);
    if (m_segmentsScrollY < 0) m_segmentsScrollY = 0;

    float curY = contentRect.top - static_cast<float>(m_segmentsScrollY);

    for (size_t i = 0; i < task.segments.size(); ++i) {
        const auto& seg = task.segments[i];
        D2D1_RECT_F itemR = D2D1::RectF(contentRect.left, curY, contentRect.right - 10.0f * m_dpiScale, curY + itemH);

        if (itemR.bottom >= contentRect.top && itemR.top <= contentRect.bottom) {
            wchar_t buf[128];
            swprintf_s(buf, L"Parça #%d  |  Ofset: %llu - %llu  |  İndirilen: %llu KB  |  Hız: %.1f KB/s",
                seg.id + 1,
                seg.startByte, seg.endByte,
                seg.downloadedBytes / 1024,
                seg.speedBps / 1024.0
            );

            DrawTextLeft(buf, itemR, m_pFormatMono, (seg.status == 2) ? m_pCyanBrush : m_pMutedTextBrush);
        }
        curY += itemH;
    }

    m_pCurrentRT->PopAxisAlignedClip();

    if (totalContentHeight > viewportHeight && viewportHeight > 0) {
        D2D1_RECT_F sbTrack = D2D1::RectF(
            contentRect.right - 6.0f * m_dpiScale,
            contentRect.top + 2.0f * m_dpiScale,
            contentRect.right - 2.0f * m_dpiScale,
            contentRect.bottom - 2.0f * m_dpiScale
        );
        DrawModernScrollbar(sbTrack, totalContentHeight, viewportHeight, static_cast<float>(m_segmentsScrollY), false, false);
    }
}

void Direct2DRenderer::RenderLogs(const DownloadTaskInfo& task, const D2D1_RECT_F& contentRect) {
    if (task.logs.empty()) {
        DrawTextCentered(L"Günlük kaydı bulunamadı", contentRect, m_pFormatRegular, m_pDimTextBrush);
        return;
    }

    m_pCurrentRT->PushAxisAlignedClip(&contentRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    float lineH = 20.0f * m_dpiScale;
    float totalContentHeight = task.logs.size() * lineH;
    float viewportHeight = contentRect.bottom - contentRect.top;
    float maxLogScroll = (std::max)(0.0f, totalContentHeight - viewportHeight);
    if (m_logsScrollY > maxLogScroll) m_logsScrollY = static_cast<int>(maxLogScroll);
    if (m_logsScrollY < 0) m_logsScrollY = 0;

    float curY = contentRect.top - static_cast<float>(m_logsScrollY);

    for (size_t i = 0; i < task.logs.size(); ++i) {
        D2D1_RECT_F lineR = D2D1::RectF(contentRect.left, curY, contentRect.right - 10.0f * m_dpiScale, curY + lineH);
        if (lineR.bottom >= contentRect.top && lineR.top <= contentRect.bottom) {
            std::wstring logLine = task.logs[i].timestamp + L" " + task.logs[i].message;
            ID2D1SolidColorBrush* br = m_pMutedTextBrush;
            if (task.logs[i].level == 1) br = m_pGreenBrush;
            else if (task.logs[i].level == 2) br = m_pAmberBrush;
            else if (task.logs[i].level == 3) br = m_pRedBrush;

            DrawTextLeft(logLine, lineR, m_pFormatMono, br);
        }
        curY += lineH;
    }

    m_pCurrentRT->PopAxisAlignedClip();

    if (totalContentHeight > viewportHeight && viewportHeight > 0) {
        D2D1_RECT_F sbTrack = D2D1::RectF(
            contentRect.right - 6.0f * m_dpiScale,
            contentRect.top + 2.0f * m_dpiScale,
            contentRect.right - 2.0f * m_dpiScale,
            contentRect.bottom - 2.0f * m_dpiScale
        );
        DrawModernScrollbar(sbTrack, totalContentHeight, viewportHeight, static_cast<float>(m_logsScrollY), false, false);
    }
}

void Direct2DRenderer::RenderFileInfo(const DownloadTaskInfo& task, const D2D1_RECT_F& contentRect) {
    m_pCurrentRT->PushAxisAlignedClip(&contentRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    float lineH = 26.0f * m_dpiScale;
    float curY = contentRect.top - static_cast<float>(m_fileInfoScrollY);

    auto drawRow = [&](const std::wstring& label, const std::wstring& val) {
        D2D1_RECT_F lblR = D2D1::RectF(contentRect.left, curY, contentRect.left + 120.0f * m_dpiScale, curY + lineH);
        D2D1_RECT_F valR = D2D1::RectF(contentRect.left + 130.0f * m_dpiScale, curY, contentRect.right - 10.0f * m_dpiScale, curY + lineH);
        if (lblR.bottom >= contentRect.top && lblR.top <= contentRect.bottom) {
            DrawTextLeft(label, lblR, m_pFormatSemiBold, m_pMutedTextBrush);
            DrawTextLeft(val, valR, m_pFormatRegular, m_pTextBrush);
        }
        curY += lineH;
    };

    drawRow(L"Dosya Adı:", task.filename);
    drawRow(L"İndirme URL:", task.url);
    drawRow(L"Kayıt Yolu:", task.fullPath);
    wchar_t szSz[64];
    swprintf_s(szSz, L"%.2f MB (%llu bayt)", (double)task.totalBytes / (1024.0 * 1024.0), task.totalBytes);
    drawRow(L"Toplam Boyut:", szSz);
    drawRow(L"Segment Sayısı:", std::to_wstring(task.splitCount) + L" Eşzamanlı Bağlantı");
    drawRow(L"Resume Desteği:", task.supportsResume ? L"Evet (HTTP 206 Partial Content)" : L"Bilinmiyor");

    m_pCurrentRT->PopAxisAlignedClip();
}

void Direct2DRenderer::RenderBottomBar(
    double totalSpeedBps,
    int activeTasksCount,
    uint64_t totalBytesDownloaded,
    uint64_t totalBytesAll
) {
    float r = (m_rcBottomBar.bottom - m_rcBottomBar.top) / 2.0f;
    D2D1_ROUNDED_RECT pillRR = D2D1::RoundedRect(m_rcBottomBar, r, r);
    m_pCurrentRT->FillRoundedRectangle(&pillRR, m_pCardBrush);
    m_pCurrentRT->DrawRoundedRectangle(&pillRR, m_pCardBorderBrush, 1.0f);

    wchar_t statusBuf[256];
    double speedMB = totalSpeedBps / (1024.0 * 1024.0);
    double dlMB = (double)totalBytesDownloaded / (1024.0 * 1024.0);
    double totMB = (double)totalBytesAll / (1024.0 * 1024.0);

    bool isTr = (I18n::Instance().GetCurrentLanguage() == LangId::Turkish);
    swprintf_s(statusBuf, isTr ? L"⚡ Gety v1.0   |   ● %d Aktif Görev   |   ⬇ %.2f MB/s   |   💾 %.1f / %.1f MB" :
                                 L"⚡ Gety v1.0   |   ● %d Active Task   |   ⬇ %.2f MB/s   |   💾 %.1f / %.1f MB",
        activeTasksCount,
        speedMB,
        dlMB,
        totMB
    );

    float rightWidth = 165.0f * m_dpiScale;
    D2D1_RECT_F leftTextR = D2D1::RectF(m_rcBottomBar.left + 12.0f * m_dpiScale, m_rcBottomBar.top, m_rcBottomBar.right - rightWidth - 8.0f * m_dpiScale, m_rcBottomBar.bottom);
    DrawTextLeft(statusBuf, leftTextR, m_pFormatSmall, m_pTextBrush);

    D2D1_RECT_F rightTextR = D2D1::RectF(m_rcBottomBar.right - rightWidth, m_rcBottomBar.top, m_rcBottomBar.right - 12.0f * m_dpiScale, m_rcBottomBar.bottom);
    DrawTextRight(isTr ? L"🎛 Sınırsız Hız Modu" : L"🎛 Unlimited Speed", rightTextR, m_pFormatSemiBold, m_pCyanBrush);
}

bool Direct2DRenderer::SaveSnapshot(
    const std::wstring& filePath,
    const std::vector<DownloadTaskInfo>& tasks,
    const std::unordered_set<std::wstring>& selectedTaskIds,
    const std::wstring& selectedTaskId,
    CategoryFilterType activeCategory,
    DetailTab activeTab
) {
    if (!m_pD2DFactory) return false;

    IWICImagingFactory* pWicFactory = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pWicFactory)
    );
    if (FAILED(hr)) return false;

    UINT w = (m_width >= 960) ? static_cast<UINT>(m_width) : 1000;
    UINT h = (m_height >= 600) ? static_cast<UINT>(m_height) : 650;
    UpdateLayout(static_cast<int>(w), static_cast<int>(h), m_splitterX, m_splitterY);
    m_mouseNearTop = true; // Always show top menus and window buttons in snapshot!

    IWICBitmap* pWicBitmap = nullptr;
    hr = pWicFactory->CreateBitmap(w, h, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &pWicBitmap);
    if (FAILED(hr)) { pWicFactory->Release(); return false; }

    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f
    );

    ID2D1RenderTarget* pWicRT = nullptr;
    hr = m_pD2DFactory->CreateWicBitmapRenderTarget(pWicBitmap, rtProps, &pWicRT);
    if (FAILED(hr)) { pWicBitmap->Release(); pWicFactory->Release(); return false; }
    pWicRT->SetDpi(96.0f, 96.0f);

    ID2D1RenderTarget* pOldRT = m_pCurrentRT;
    m_pCurrentRT = pWicRT;
    CreateBrushesFor(pWicRT);

    double totalSpeed = 0.0;
    int activeCount = 0;
    uint64_t totalBytesDl = 0;
    uint64_t totalBytes = 0;
    for (const auto& t : tasks) {
        if (t.state == DownloadState::Downloading || t.state == DownloadState::Connecting) {
            activeCount++;
            totalSpeed += t.currentSpeedBps;
        }
        totalBytesDl += t.downloadedBytes;
        totalBytes += t.totalBytes;
    }
    if (activeCount == 0 && totalSpeed == 0.0) {
        totalSpeed = 14.8 * 1024 * 1024;
        activeCount = 1;
    }

    std::unordered_set<std::wstring> selIds = selectedTaskIds;
    if (selIds.empty() && !selectedTaskId.empty()) selIds.insert(selectedTaskId);

    pWicRT->BeginDraw();
    RenderAll(tasks, selIds, selectedTaskId, activeCategory, activeTab, m_splitterX, m_splitterY, false, false, totalSpeed, activeCount, totalBytesDl, totalBytes);
    pWicRT->EndDraw();

    IWICStream* pStream = nullptr;
    hr = pWicFactory->CreateStream(&pStream);
    if (FAILED(hr) || !pStream) {
        pWicRT->Release();
        pWicBitmap->Release();
        pWicFactory->Release();
        return false;
    }

    hr = pStream->InitializeFromFilename(filePath.c_str(), GENERIC_WRITE);
    if (FAILED(hr)) {
        pStream->Release();
        pWicRT->Release();
        pWicBitmap->Release();
        pWicFactory->Release();
        return false;
    }

    IWICBitmapEncoder* pEncoder = nullptr;
    hr = pWicFactory->CreateEncoder(GUID_ContainerFormatPng, NULL, &pEncoder);
    if (FAILED(hr) || !pEncoder) {
        pStream->Release();
        pWicRT->Release();
        pWicBitmap->Release();
        pWicFactory->Release();
        return false;
    }

    hr = pEncoder->Initialize(pStream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) {
        pEncoder->Release();
        pStream->Release();
        pWicRT->Release();
        pWicBitmap->Release();
        pWicFactory->Release();
        return false;
    }

    IWICBitmapFrameEncode* pFrame = nullptr;
    hr = pEncoder->CreateNewFrame(&pFrame, NULL);
    if (FAILED(hr) || !pFrame) {
        pEncoder->Release();
        pStream->Release();
        pWicRT->Release();
        pWicBitmap->Release();
        pWicFactory->Release();
        return false;
    }

    hr = pFrame->Initialize(NULL);
    if (SUCCEEDED(hr)) hr = pFrame->SetSize(w, h);
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppPBGRA;
    if (SUCCEEDED(hr)) hr = pFrame->SetPixelFormat(&format);
    if (SUCCEEDED(hr)) hr = pFrame->WriteSource(pWicBitmap, NULL);
    if (SUCCEEDED(hr)) hr = pFrame->Commit();
    if (SUCCEEDED(hr)) hr = pEncoder->Commit();

    pFrame->Release();
    pEncoder->Release();
    pStream->Release();
    pWicRT->Release();
    pWicBitmap->Release();
    pWicFactory->Release();

    m_pCurrentRT = pOldRT;
    if (m_pRenderTarget) {
        CreateBrushesFor(m_pRenderTarget);
    }
    return true;
}

} // namespace Gety
