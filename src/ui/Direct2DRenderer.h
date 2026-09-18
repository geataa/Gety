#pragma once

#include "../core/Types.h"
#include "../engine/DownloadTask.h"
#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <memory>
#include <functional>

namespace Gety {

enum class DetailTab {
    Matrix = 0,
    Segments = 1,
    Logs = 2,
    Info = 3
};

struct Direct2DTopButton {
    int id = 0;
    D2D1_RECT_F rect = {};
    std::wstring label;
    std::wstring icon;
    bool isHovered = false;
    bool isPressed = false;
    bool isActive = false;
};

struct CategoryItemDef {
    CategoryFilterType type;
    std::wstring label;
    std::wstring icon;
    int count = 0;
    D2D1_RECT_F rect = {};
    bool isHovered = false;
};

class Direct2DRenderer {
public:
    Direct2DRenderer();
    ~Direct2DRenderer();

    bool Initialize(HWND hwnd);
    void DiscardDeviceResources();
    HRESULT CreateDeviceResources();
    void OnResize(UINT width, UINT height);
    void OnDpiChanged(UINT newDpi);
    void CreateTextFormats();
    void DiscardTextFormats();

    ID2D1HwndRenderTarget* GetRenderTarget() const { return m_pRenderTarget; }
    float GetDpiScale() const { return m_dpiScale; }

    // Hit Testing
    bool IsOverTopRightButtons(int x, int y, int& outBtnId) const;
    bool IsOverActionBar(int x, int y, int& outBtnId) const;
    bool IsOverCategories(int x, int y, CategoryFilterType& outCat) const;
    bool IsOverTaskList(int x, int y, int& outTaskRowIndex, std::wstring& outTaskId) const;
    bool IsOverDetailTabs(int x, int y, DetailTab& outTab) const;
    bool IsOverSplitterV(int x, int y) const;
    bool IsOverSplitterH(int x, int y) const;
    bool IsInDraggableHeader(int x, int y) const;

    // Layout Calculations
    void UpdateLayout(int width, int height, int splitterX, int splitterY);

    // Mouse Event Handlers
    void OnMouseMove(int x, int y);
    void OnLButtonDown(int x, int y);
    void OnLButtonUp(int x, int y);
    void OnMouseWheel(int x, int y, int delta);
    bool IsDraggingScrollbar() const { return m_isDraggingScrollbar; }
    bool IsMouseNearTop() const { return m_mouseNearTop; }
    void SetMouseNearTop(bool nearTop) { m_mouseNearTop = nearTop; }

    // Rendering
    void BeginDraw();
    HRESULT EndDraw();

    void RenderAll(
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
    );

    bool SaveSnapshot(
        const std::wstring& filePath,
        const std::vector<DownloadTaskInfo>& tasks,
        const std::unordered_set<std::wstring>& selectedTaskIds,
        const std::wstring& selectedTaskId,
        CategoryFilterType activeCategory,
        DetailTab activeTab
    );

    // Getters for bounds
    D2D1_RECT_F GetTaskListRect() const { return m_rcTaskListCard; }
    int GetScrollOffset() const { return m_taskListScrollY; }
    const std::vector<std::wstring>& GetVisibleTaskIds() const { return m_visibleTaskIds; }
    void ResetHoverStates();

    // Top Button IDs
    static const int BTN_MINIMIZE = 9001;
    static const int BTN_MAXIMIZE = 9002;
    static const int BTN_CLOSE = 9003;

    static const int BTN_NEW = 9101;
    static const int BTN_BATCH = 9102;
    static const int BTN_VIDEO_LINK = 9110;
    static const int BTN_OLLAMA_MODEL = 9111;
    static const int BTN_HF_MODEL = 9112;
    static const int BTN_START = 9103;
    static const int BTN_PAUSE = 9104;
    static const int BTN_DELETE = 9105;
    static const int BTN_SPEED_MODE = 9106;
    static const int BTN_LANGUAGE = 9107;
    static const int BTN_SETTINGS = 9108;
    static const int BTN_ABOUT = 9109;

private:
    void RenderBackground();
    void RenderTopBar();
    void RenderCategoryCard(const std::vector<DownloadTaskInfo>& tasks, CategoryFilterType activeCategory);
    void RenderTaskListCard(
        const std::vector<DownloadTaskInfo>& tasks,
        const std::unordered_set<std::wstring>& selectedTaskIds,
        const std::wstring& selectedTaskId,
        CategoryFilterType activeCategory
    );
    void RenderSplitters(bool isVHovered, bool isHHovered);
    void RenderDetailCard(const DownloadTaskInfo* pSelectedTask, DetailTab activeTab);
    void RenderChunkMatrix(const DownloadTaskInfo& task, const D2D1_RECT_F& contentRect);
    void RenderSegments(const DownloadTaskInfo& task, const D2D1_RECT_F& contentRect);
    void RenderLogs(const DownloadTaskInfo& task, const D2D1_RECT_F& contentRect);
    void RenderFileInfo(const DownloadTaskInfo& task, const D2D1_RECT_F& contentRect);
    void RenderBottomBar(double totalSpeedBps, int activeTasksCount, uint64_t totalBytesDownloaded, uint64_t totalBytesAll);

    // Helpers
    void DrawRoundedPill(const D2D1_RECT_F& rect, ID2D1Brush* fill, ID2D1Brush* stroke = nullptr, float strokeWidth = 1.0f);
    void DrawTextCentered(const std::wstring& text, const D2D1_RECT_F& rect, IDWriteTextFormat* format, ID2D1Brush* brush);
    void DrawTextLeft(const std::wstring& text, const D2D1_RECT_F& rect, IDWriteTextFormat* format, ID2D1Brush* brush);
    void DrawTextRight(const std::wstring& text, const D2D1_RECT_F& rect, IDWriteTextFormat* format, ID2D1Brush* brush);

    HWND m_hwnd = NULL;
    int m_width = 0;
    int m_height = 0;
    int m_splitterX = 230;
    int m_splitterY = 320;
    float m_dpiScale = 1.0f;

    // Factories
    ID2D1Factory* m_pD2DFactory = nullptr;
    IDWriteFactory* m_pDWriteFactory = nullptr;
    ID2D1HwndRenderTarget* m_pRenderTarget = nullptr;
    ID2D1RenderTarget* m_pCurrentRT = nullptr;

    void CreateBrushesFor(ID2D1RenderTarget* pRT);
    void DiscardBrushes();

    // Brushes (GhostView Dark Obsidian Palette)
    ID2D1SolidColorBrush* m_pBgBrush = nullptr;             // #0d0f14
    ID2D1SolidColorBrush* m_pCardBrush = nullptr;           // #141720
    ID2D1SolidColorBrush* m_pCardBorderBrush = nullptr;     // #232734
    ID2D1SolidColorBrush* m_pHeaderBrush = nullptr;         // #181c27
    ID2D1SolidColorBrush* m_pTextBrush = nullptr;           // #f1f5f9
    ID2D1SolidColorBrush* m_pMutedTextBrush = nullptr;      // #8e96a8
    ID2D1SolidColorBrush* m_pDimTextBrush = nullptr;        // #5c6374
    ID2D1SolidColorBrush* m_pCyanBrush = nullptr;           // #00a2ff
    ID2D1SolidColorBrush* m_pCyanGlowBrush = nullptr;       // rgba(0, 162, 255, 0.22)
    ID2D1SolidColorBrush* m_pGreenBrush = nullptr;          // #22c55e
    ID2D1SolidColorBrush* m_pGreenGlowBrush = nullptr;      // rgba(34, 197, 94, 0.22)
    ID2D1SolidColorBrush* m_pAmberBrush = nullptr;          // #f59e0b
    ID2D1SolidColorBrush* m_pRedBrush = nullptr;            // #ef4444
    ID2D1SolidColorBrush* m_pHoverBrush = nullptr;          // rgba(255, 255, 255, 0.08)
    ID2D1SolidColorBrush* m_pPressedBrush = nullptr;        // rgba(0, 162, 255, 0.40)
    ID2D1SolidColorBrush* m_pActivePillBrush = nullptr;     // rgba(0, 162, 255, 0.25)
    ID2D1SolidColorBrush* m_pPillBorderBrush = nullptr;     // rgba(255, 255, 255, 0.14)
    ID2D1SolidColorBrush* m_pProgressTrackBrush = nullptr;  // #1c202d
    ID2D1SolidColorBrush* m_pSplitterHoverBrush = nullptr;  // rgba(0, 162, 255, 0.65)
    ID2D1SolidColorBrush* m_pPendingBlockBrush = nullptr;   // #202432

    // Text Formats
    IDWriteTextFormat* m_pFormatRegular = nullptr;  // Segoe UI 12.5px
    IDWriteTextFormat* m_pFormatMedium = nullptr;   // Segoe UI Medium 13.5px
    IDWriteTextFormat* m_pFormatSemiBold = nullptr; // Segoe UI SemiBold 14px
    IDWriteTextFormat* m_pFormatBold = nullptr;     // Segoe UI Bold 16px
    IDWriteTextFormat* m_pFormatSmall = nullptr;    // Segoe UI 11px
    IDWriteTextFormat* m_pFormatIcons = nullptr;    // Segoe UI Symbol 16px
    IDWriteTextFormat* m_pFormatMono = nullptr;     // Consolas 11.5px
    IDWriteTextFormat* m_pFormatTopBtn = nullptr;   // Segoe UI Symbol 15px

    // Layout Rectangles
    D2D1_RECT_F m_rcTopBar = {};
    D2D1_RECT_F m_rcBrand = {};
    D2D1_RECT_F m_rcActionPill = {};
    D2D1_RECT_F m_rcCategoryCard = {};
    D2D1_RECT_F m_rcSplitterV = {};
    D2D1_RECT_F m_rcTaskListCard = {};
    D2D1_RECT_F m_rcSplitterH = {};
    D2D1_RECT_F m_rcDetailCard = {};
    D2D1_RECT_F m_rcBottomBar = {};

    // Interactive Top Right Buttons (Minimize, Maximize, Close)
    Direct2DTopButton m_btnMin;
    Direct2DTopButton m_btnMax;
    Direct2DTopButton m_btnClose;

    // Action Pill Buttons
    std::vector<Direct2DTopButton> m_actionButtons;

    // Categories
    std::vector<CategoryItemDef> m_categoryDefs;

    // Detail Tabs
    struct DetailTabBtn {
        DetailTab tab;
        std::wstring label;
        std::wstring icon;
        D2D1_RECT_F rect = {};
        bool isHovered = false;
    };
    std::vector<DetailTabBtn> m_detailTabs;

    // Scroll States for Sub-widgets
    int m_categoryScrollY = 0;
    int m_taskListScrollY = 0;
    int m_matrixScrollY = 0;
    int m_logsScrollY = 0;
    int m_segmentsScrollY = 0;
    int m_fileInfoScrollY = 0;

    // Scrollbar Drag Tracking
    enum class ScrollArea { None, Category, TaskList, Matrix, Logs, Segments, FileInfo };
    ScrollArea m_dragScrollArea = ScrollArea::None;
    bool m_isDraggingScrollbar = false;
    float m_dragStartMouseY = 0.0f;
    int m_dragStartScrollY = 0;

    void DrawModernScrollbar(const D2D1_RECT_F& trackRect, float contentHeight, float viewportHeight, float scrollY, bool isHovered, bool isDragging);

    // Task List Internal State
    int m_hoveredTaskIndex = -1;
    std::vector<std::wstring> m_visibleTaskIds;
    std::vector<D2D1_RECT_F> m_visibleTaskRowRects;

    // Hover Tracking
    int m_hoveredActionBtnId = -1;
    int m_pressedActionBtnId = -1;
    int m_hoveredTopRightId = -1;
    int m_pressedTopRightId = -1;
    CategoryFilterType m_hoveredCategory = CategoryFilterType::All;
    bool m_hasHoveredCategory = false;
    DetailTab m_hoveredDetailTab = DetailTab::Matrix;
    bool m_hasHoveredDetailTab = false;
    bool m_mouseNearTop = true;
};

} // namespace Gety
