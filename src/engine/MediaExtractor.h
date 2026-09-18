#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace Gety {

struct MediaFormat {
    std::wstring formatId;
    std::wstring ext;         // "mp4", "m4a", "webm", "mp3"
    std::wstring resolution;  // "2160p (4K)", "1440p (2K)", "1080p (Full HD)", "720p (HD)", "480p (SD)", "360p (SD)", "Ses (MP3)"
    std::wstring note;        // "60fps", "HDR", "En Yüksek Kalite", etc.
    uint64_t filesize = 0;    // bytes (approx or exact)
    bool isAudioOnly = false;
    int height = 0;
    int fps = 0;
};

struct MediaItem {
    std::wstring id;
    std::wstring title;
    std::wstring url;
    int durationSec = 0;
    std::wstring uploader;
    std::vector<MediaFormat> formats;
    bool isSelected = true;
};

struct MediaExtractionResult {
    bool success = false;
    bool isPlaylist = false;
    std::wstring title;
    std::wstring errorMsg;
    std::vector<MediaItem> items;
};

class MediaExtractor {
public:
    static std::wstring GetYtDlpPath();
    static std::wstring GetFfmpegPath();
    static bool IsAvailable();
    static bool IsMediaUrl(const std::wstring& url);

    // Analyzes the given URL (single video or playlist)
    static MediaExtractionResult ExtractInfo(const std::wstring& url,
                                            std::function<void(const std::wstring&)> statusCb = nullptr);

    // Formats duration in seconds to "MM:SS" or "HH:MM:SS"
    static std::wstring FormatDuration(int seconds);

    // Formats bytes to human readable (e.g. "145.2 MB")
    static std::wstring FormatBytes(uint64_t bytes);

    // Converts UTF-8 string to wide string
    static std::wstring Utf8ToWide(const std::string& str);
    static std::string WideToUtf8(const std::wstring& wstr);
};

} // namespace Gety
