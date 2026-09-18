#include "MediaExtractor.h"
#include "../core/Config.h"
#include "../core/JsonParser.h"
#include <shlwapi.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unordered_map>
#include <map>
#include <set>

#pragma comment(lib, "shlwapi.lib")

namespace Gety {

std::wstring MediaExtractor::Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int count = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0);
    if (count <= 0) return L"";
    std::wstring result(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &result[0], count);
    return result;
}

std::string MediaExtractor::WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int count = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
    if (count <= 0) return "";
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &result[0], count, NULL, NULL);
    return result;
}

std::wstring MediaExtractor::FormatDuration(int seconds) {
    if (seconds <= 0) return L"00:00";
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    wchar_t buf[32];
    if (h > 0) {
        swprintf_s(buf, L"%d:%02d:%02d", h, m, s);
    } else {
        swprintf_s(buf, L"%02d:%02d", m, s);
    }
    return buf;
}

std::wstring MediaExtractor::FormatBytes(uint64_t bytes) {
    if (bytes == 0) return L"Bilinmiyor";
    static const wchar_t* units[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    int idx = 0;
    double dBytes = static_cast<double>(bytes);
    while (dBytes >= 1024.0 && idx < 4) {
        dBytes /= 1024.0;
        idx++;
    }
    wchar_t buf[32];
    if (idx == 0) {
        swprintf_s(buf, L"%llu B", bytes);
    } else {
        swprintf_s(buf, L"%.1f %s", dBytes, units[idx]);
    }
    return buf;
}

std::wstring MediaExtractor::GetYtDlpPath() {
    // 1. Check exe directory \tools\yt-dlp.exe
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);

    std::wstring candidate1 = std::wstring(exePath) + L"\\tools\\yt-dlp.exe";
    if (GetFileAttributesW(candidate1.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return candidate1;
    }

    // 2. Check exe directory \yt-dlp.exe
    std::wstring candidate2 = std::wstring(exePath) + L"\\yt-dlp.exe";
    if (GetFileAttributesW(candidate2.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return candidate2;
    }

    // 3. Check dev parent directory (e.g. build\Release -> root)
    std::wstring candidateDev1 = std::wstring(exePath) + L"\\..\\..\\tools\\yt-dlp.exe";
    if (GetFileAttributesW(candidateDev1.c_str()) != INVALID_FILE_ATTRIBUTES) {
        wchar_t fullPath[MAX_PATH];
        if (GetFullPathNameW(candidateDev1.c_str(), MAX_PATH, fullPath, NULL)) {
            return fullPath;
        }
    }

    std::wstring candidateDev2 = std::wstring(exePath) + L"\\..\\tools\\yt-dlp.exe";
    if (GetFileAttributesW(candidateDev2.c_str()) != INVALID_FILE_ATTRIBUTES) {
        wchar_t fullPath[MAX_PATH];
        if (GetFullPathNameW(candidateDev2.c_str(), MAX_PATH, fullPath, NULL)) {
            return fullPath;
        }
    }

    // 4. Check current working directory \tools\yt-dlp.exe
    std::wstring candidate3 = L"tools\\yt-dlp.exe";
    if (GetFileAttributesW(candidate3.c_str()) != INVALID_FILE_ATTRIBUTES) {
        wchar_t fullPath[MAX_PATH];
        if (GetFullPathNameW(candidate3.c_str(), MAX_PATH, fullPath, NULL)) {
            return fullPath;
        }
        return candidate3;
    }

    // 5. Check winget directory in AppData
    wchar_t localAppData[MAX_PATH];
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH) > 0) {
        std::wstring candidate4 = std::wstring(localAppData) + L"\\Microsoft\\WinGet\\Links\\yt-dlp.exe";
        if (GetFileAttributesW(candidate4.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return candidate4;
        }
    }

    // 6. Search PATH
    wchar_t foundInPath[MAX_PATH];
    if (SearchPathW(NULL, L"yt-dlp.exe", NULL, MAX_PATH, foundInPath, NULL) > 0) {
        return foundInPath;
    }

    return L"";
}

std::wstring MediaExtractor::GetFfmpegPath() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);

    std::wstring candidate1 = std::wstring(exePath) + L"\\tools\\ffmpeg.exe";
    if (GetFileAttributesW(candidate1.c_str()) != INVALID_FILE_ATTRIBUTES) return candidate1;

    std::wstring candidate2 = std::wstring(exePath) + L"\\ffmpeg.exe";
    if (GetFileAttributesW(candidate2.c_str()) != INVALID_FILE_ATTRIBUTES) return candidate2;

    std::wstring candidateDev1 = std::wstring(exePath) + L"\\..\\..\\tools\\ffmpeg.exe";
    if (GetFileAttributesW(candidateDev1.c_str()) != INVALID_FILE_ATTRIBUTES) {
        wchar_t fullPath[MAX_PATH];
        if (GetFullPathNameW(candidateDev1.c_str(), MAX_PATH, fullPath, NULL)) return fullPath;
    }

    wchar_t localAppData[MAX_PATH];
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH) > 0) {
        std::wstring candidateWinget = std::wstring(localAppData) + L"\\Microsoft\\WinGet\\Links\\ffmpeg.exe";
        if (GetFileAttributesW(candidateWinget.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return candidateWinget;
        }
    }

    wchar_t foundInPath[MAX_PATH];
    if (SearchPathW(NULL, L"ffmpeg.exe", NULL, MAX_PATH, foundInPath, NULL) > 0) {
        return foundInPath;
    }

    return L"";
}

bool MediaExtractor::IsAvailable() {
    return !GetYtDlpPath().empty();
}

bool MediaExtractor::IsMediaUrl(const std::wstring& url) {
    if (url.empty()) return false;
    std::wstring lower = url;
    for (auto& c : lower) c = towlower(c);

    static const wchar_t* mediaDomains[] = {
        L"youtube.com",
        L"youtu.be",
        L"vimeo.com",
        L"dailymotion.com",
        L"tiktok.com",
        L"twitch.tv",
        L"twitter.com",
        L"x.com",
        L"instagram.com",
        L"facebook.com",
        L"fb.watch",
        L"soundcloud.com"
    };

    for (const auto* domain : mediaDomains) {
        if (lower.find(domain) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}


MediaExtractionResult MediaExtractor::ExtractInfo(const std::wstring& url,
                                                  std::function<void(const std::wstring&)> statusCb)
{
    MediaExtractionResult result;
    result.success = false;

    std::wstring ytDlpPath = GetYtDlpPath();
    if (ytDlpPath.empty()) {
        result.errorMsg = L"yt-dlp.exe bulunamadı! Lütfen tools klasörüne yerleştirin.";
        return result;
    }

    if (statusCb) {
        statusCb(L"Medya bilgileri sorgulanıyor...");
    }

    // Prepare command line: yt-dlp.exe -J --flat-playlist --no-warnings "<url>"
    std::wstring cmd = L"\"" + ytDlpPath + L"\" -J --flat-playlist --no-warnings \"" + url + L"\"";

    HANDLE hStdOutRead = NULL;
    HANDLE hStdOutWrite = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };

    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
        result.errorMsg = L"İşlem borusu (pipe) oluşturulamadı.";
        return result;
    }
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdOutWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    BOOL created = CreateProcessW(
        NULL, cmdBuf.data(), NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi
    );

    CloseHandle(hStdOutWrite); // Close write end in parent

    if (!created) {
        CloseHandle(hStdOutRead);
        result.errorMsg = L"yt-dlp başlatılamadı.";
        return result;
    }

    // Read all output from pipe
    std::string jsonOutput;
    char buffer[8192];
    DWORD bytesRead = 0;

    while (ReadFile(hStdOutRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        jsonOutput.append(buffer, bytesRead);
    }

    CloseHandle(hStdOutRead);
    WaitForSingleObject(pi.hProcess, 60000); // Wait up to 60s
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (jsonOutput.empty()) {
        result.errorMsg = L"Medya bilgisi alınamadı veya bağlantı desteklenmiyor.";
        return result;
    }

    size_t firstBrace = jsonOutput.find('{');
    if (firstBrace == std::string::npos) {
        result.errorMsg = L"Medya bilgisi alınamadı veya bağlantı desteklenmiyor.";
        return result;
    }
    if (firstBrace > 0) {
        jsonOutput = jsonOutput.substr(firstBrace);
    }

    if (statusCb) {
        statusCb(L"Formatlar ve video listesi ayrıştırılıyor...");
    }

    // Parse JSON
    JsonParser parser(jsonOutput);
    JValue root = parser.Parse();

    if (root.type != JType::Object) {
        result.errorMsg = L"Geçersiz veri yanıtı.";
        return result;
    }

    std::string jType = root.getString("_type");
    std::string jTitle = root.getString("title");
    result.title = Utf8ToWide(jTitle);

    if (jType == "playlist") {
        result.isPlaylist = true;
        const auto& entries = root.getArray("entries");

        for (const auto& entry : entries) {
            MediaItem item;
            item.id = Utf8ToWide(entry.getString("id"));
            item.title = Utf8ToWide(entry.getString("title"));
            item.durationSec = static_cast<int>(entry.getInt("duration"));
            item.uploader = Utf8ToWide(entry.getString("uploader"));

            std::string entryUrl = entry.getString("url");
            if (entryUrl.empty() && !item.id.empty()) {
                item.url = L"https://www.youtube.com/watch?v=" + item.id;
            } else {
                item.url = Utf8ToWide(entryUrl);
            }
            item.isSelected = true;

            // Add standard resolution profiles for playlist items
            MediaFormat f1080;
            f1080.formatId = L"bestvideo[height<=1080]+bestaudio/best[height<=1080]/best";
            f1080.resolution = L"1080p Full HD";
            f1080.ext = L"mp4";
            f1080.height = 1080;
            item.formats.push_back(f1080);

            MediaFormat f720;
            f720.formatId = L"bestvideo[height<=720]+bestaudio/best[height<=720]/best";
            f720.resolution = L"720p HD";
            f720.ext = L"mp4";
            f720.height = 720;
            item.formats.push_back(f720);

            MediaFormat fAudio;
            fAudio.formatId = L"mp3";
            fAudio.resolution = L"Ses (MP3)";
            fAudio.ext = L"mp3";
            fAudio.isAudioOnly = true;
            item.formats.push_back(fAudio);

            result.items.push_back(item);
        }

        result.success = !result.items.empty();
    } else {
        // Single Video
        result.isPlaylist = false;
        MediaItem item;
        item.id = Utf8ToWide(root.getString("id"));
        item.title = Utf8ToWide(root.getString("title"));
        item.durationSec = static_cast<int>(root.getInt("duration"));
        item.uploader = Utf8ToWide(root.getString("uploader"));
        item.url = url;
        item.isSelected = true;

        const auto& jsonFormats = root.getArray("formats");

        // Collect best audio size to add to approximate video sizes
        uint64_t bestAudioSize = 0;
        for (const auto& fmt : jsonFormats) {
            std::string vcodec = fmt.getString("vcodec");
            std::string acodec = fmt.getString("acodec");
            if (vcodec == "none" && acodec != "none") {
                uint64_t sz = static_cast<uint64_t>(fmt.getInt("filesize"));
                if (sz == 0) sz = static_cast<uint64_t>(fmt.getInt("filesize_approx"));
                if (sz > bestAudioSize) bestAudioSize = sz;
            }
        }
        if (bestAudioSize == 0) bestAudioSize = 8 * 1024 * 1024; // fallback ~8MB

        // Map height -> best MediaFormat
        struct HeightDef {
            int targetHeight;
            const wchar_t* resName;
        };
        static const HeightDef targetHeights[] = {
            { 2160, L"2160p (4K Ultra HD)" },
            { 1440, L"1440p (2K Quad HD)" },
            { 1080, L"1080p (Full HD)" },
            { 720,  L"720p (HD)" },
            { 480,  L"480p (SD)" },
            { 360,  L"360p (SD)" }
        };

        std::map<int, MediaFormat> videoFormats;

        for (const auto& fmt : jsonFormats) {
            int h = static_cast<int>(fmt.getInt("height"));
            if (h <= 0) continue;
            std::string vcodec = fmt.getString("vcodec");
            if (vcodec == "none") continue;

            uint64_t sz = static_cast<uint64_t>(fmt.getInt("filesize"));
            if (sz == 0) sz = static_cast<uint64_t>(fmt.getInt("filesize_approx"));
            int fps = static_cast<int>(fmt.getInt("fps"));

            // Check which standard target height this matches
            for (const auto& th : targetHeights) {
                if (h == th.targetHeight || (h > th.targetHeight - 30 && h <= th.targetHeight)) {
                    int key = th.targetHeight;
                    if (videoFormats.find(key) == videoFormats.end() || fps > videoFormats[key].fps) {
                        MediaFormat mf;
                        mf.height = key;
                        mf.fps = fps;
                        mf.resolution = th.resName;
                        mf.ext = L"mp4";
                        mf.formatId = L"bestvideo[height<=" + std::to_wstring(key) + L"]+bestaudio/best[height<=" + std::to_wstring(key) + L"]/best";
                        mf.filesize = sz + (fmt.getString("acodec") == "none" ? bestAudioSize : 0);
                        if (fps > 30) {
                            mf.note = std::to_wstring(fps) + L"fps";
                        }
                        videoFormats[key] = mf;
                    }
                    break;
                }
            }
        }

        // Add detected video formats in descending resolution
        for (auto it = videoFormats.rbegin(); it != videoFormats.rend(); ++it) {
            item.formats.push_back(it->second);
        }

        // Fallback default format if none found
        if (item.formats.empty()) {
            MediaFormat mfDef;
            mfDef.resolution = L"En İyi Video (MP4)";
            mfDef.formatId = L"bestvideo+bestaudio/best";
            mfDef.ext = L"mp4";
            item.formats.push_back(mfDef);
        }

        // Add Audio Only (MP3 & M4A)
        MediaFormat mfMp3;
        mfMp3.formatId = L"mp3";
        mfMp3.resolution = L"En Yüksek Kalite Ses (MP3)";
        mfMp3.ext = L"mp3";
        mfMp3.isAudioOnly = true;
        mfMp3.filesize = bestAudioSize;
        mfMp3.note = L"320/256 kbps";
        item.formats.push_back(mfMp3);

        MediaFormat mfM4a;
        mfM4a.formatId = L"m4a";
        mfM4a.resolution = L"M4A / AAC Ses";
        mfM4a.ext = L"m4a";
        mfM4a.isAudioOnly = true;
        mfM4a.filesize = bestAudioSize;
        mfM4a.note = L"Orijinal Ses";
        item.formats.push_back(mfM4a);

        result.items.push_back(item);
        result.success = true;
    }

    return result;
}

} // namespace Gety
