#include "DownloadTask.h"
#include "WinHttpUtils.h"
#include "MediaExtractor.h"
#include "../core/Config.h"
#include <shlwapi.h>
#include <mmsystem.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shlwapi.lib")

namespace Gety {

static uint64_t ParseMediaBytes(const std::string& str) {
    if (str.empty() || str == "NA") return 0;
    double val = 0.0;
    char unit[16] = { 0 };
    if (sscanf_s(str.c_str(), "%lf%15s", &val, unit, (unsigned)_countof(unit)) >= 1) {
        std::string u = unit;
        std::transform(u.begin(), u.end(), u.begin(), ::toupper);
        if (u.find("G") != std::string::npos) return static_cast<uint64_t>(val * 1024.0 * 1024.0 * 1024.0);
        if (u.find("M") != std::string::npos) return static_cast<uint64_t>(val * 1024.0 * 1024.0);
        if (u.find("K") != std::string::npos) return static_cast<uint64_t>(val * 1024.0);
        return static_cast<uint64_t>(val);
    }
    return 0;
}

static double ParseMediaSpeed(const std::string& str) {
    if (str.empty() || str == "NA") return 0.0;
    return static_cast<double>(ParseMediaBytes(str));
}

static std::wstring GenerateTaskId() {
    GUID guid;
    CoCreateGuid(&guid);
    wchar_t buf[64];
    swprintf_s(buf, L"%08x%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",
              guid.Data1, guid.Data2, guid.Data3,
              guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
              guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return buf;
}

static std::wstring GetCurrentTimeString() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[32];
    swprintf_s(buf, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    return buf;
}

static std::wstring GetCurrentDateString() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[32];
    swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    return buf;
}

DownloadTask::DownloadTask(const std::wstring& url,
                           const std::wstring& saveDir,
                           const std::wstring& category,
                           int splitParts,
                           const std::wstring& customFilename,
                           bool isMediaTask,
                           const std::wstring& mediaFormat)
    : m_isMediaTask(isMediaTask), m_mediaFormat(mediaFormat)
{
    m_info.id = GenerateTaskId();
    m_info.url = url;
    m_info.saveDirectory = saveDir.empty() ? Config::Instance().defaultSavePath : saveDir;
    m_info.splitCount = (splitParts > 0) ? splitParts : Config::Instance().defaultSplitParts;
    if (!customFilename.empty()) {
        m_info.filename = customFilename;
        m_customFilenameSet = true;
    } else {
        m_info.filename = WinHttpUtils::ExtractFilenameFromUrl(url);
        m_customFilenameSet = false;
    }

    if (m_isMediaTask) {
        if (m_mediaFormat == L"mp3" || m_mediaFormat == L"m4a") {
            m_info.category = LStr(StrId::CatMusic);
        } else {
            m_info.category = LStr(StrId::CatVideo);
        }
    } else if (category.empty() || category == L"Genel" || category == L"General") {
        StrId detectedCat = WinHttpUtils::DetectCategoryFromFilename(m_info.filename);
        m_info.category = LStr(detectedCat);
    } else {
        m_info.category = category;
    }

    m_info.fullPath = m_info.saveDirectory + L"\\" + m_info.filename;
    m_info.addedDate = GetCurrentDateString();
    m_info.state = DownloadState::Queued;
    m_state = DownloadState::Queued;

    AddLog(L"Görev oluşturuldu: " + m_info.url, 0);
}

bool DownloadTask::UpdateUrl(const std::wstring& newUrl) {
    std::wstring trimmed = WinHttpUtils::TrimUrl(newUrl);
    if (trimmed.empty()) return false;

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);

        // If active, stop current workers & close file handle safely
        DownloadState curState = m_state.load();
        if (curState == DownloadState::Downloading || curState == DownloadState::Connecting) {
            for (auto& w : m_workers) {
                if (w) w->Stop();
            }
            m_workers.clear();
            if (m_hFile != INVALID_HANDLE_VALUE) {
                CloseHandle(m_hFile);
                m_hFile = INVALID_HANDLE_VALUE;
            }
            m_state = DownloadState::Paused;
        }

        m_info.url = trimmed;
        m_resolvedUrl = trimmed;
        m_retryCount = 0;
        m_info.errorMsg.clear();
    }

    AddLog(L"İndirme bağlantısı güncellendi: " + trimmed, 1);
    SaveState();

    if (m_eventCb) {
        m_eventCb(m_info.id);
    }
    return true;
}

DownloadTask::~DownloadTask() {
    Stop();
    if (m_probeThread.joinable()) {
        m_probeThread.detach();
    }
    if (m_mediaThread.joinable()) {
        m_mediaThread.detach();
    }
}

void DownloadTask::SetCategory(const std::wstring& cat) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_info.category = cat;
}

std::wstring DownloadTask::GetTempFilePath() const {
    return m_info.saveDirectory + L"\\" + m_info.filename + L".gety";
}

std::wstring DownloadTask::GetStateFilePath() const {
    return m_info.saveDirectory + L"\\" + m_info.filename + L".gety.state";
}

void DownloadTask::AddLog(const std::wstring& msg, int level) {
    TaskLogEntry entry;
    entry.timestamp = GetCurrentTimeString();
    entry.message = msg;
    entry.level = level;

    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_info.logs.push_back(entry);
    // Keep max 200 logs
    if (m_info.logs.size() > 200) {
        m_info.logs.erase(m_info.logs.begin());
    }
}

DownloadTaskInfo DownloadTask::GetSnapshot() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    DownloadTaskInfo snap = m_info;
    snap.state = m_state.load();

    if (snap.state == DownloadState::Downloading) {
        double instantSum = 0.0;
        if (m_isMediaTask) {
            snap.downloadedBytes = m_info.downloadedBytes;
            snap.totalBytes = m_info.totalBytes;
            instantSum = m_info.currentSpeedBps;

            if (snap.segments.empty()) {
                SegmentInfo si;
                si.id = 0;
                si.startByte = 0;
                si.endByte = (snap.totalBytes > 0) ? snap.totalBytes - 1 : 0;
                si.downloadedBytes = snap.downloadedBytes;
                si.speedBps = snap.currentSpeedBps;
                si.status = 2; // downloading
                snap.segments.push_back(si);
            } else {
                snap.segments[0].downloadedBytes = snap.downloadedBytes;
                snap.segments[0].speedBps = snap.currentSpeedBps;
                snap.segments[0].endByte = (snap.totalBytes > 0) ? snap.totalBytes - 1 : 0;
            }
        } else {
            snap.downloadedBytes = 0;
            snap.segments.clear();

            for (const auto& w : m_workers) {
                if (w) {
                    SegmentInfo si = w->GetInfo();
                    snap.downloadedBytes += si.downloadedBytes;
                    instantSum += si.speedBps;
                    snap.segments.push_back(si);
                }
            }
        }

        // Rolling 3-second moving average speed calculation
        auto now = std::chrono::steady_clock::now();
        m_speedSamples.push_back({ now, snap.downloadedBytes });

        // Remove samples older than 3.0 seconds
        while (m_speedSamples.size() > 1) {
            double age = std::chrono::duration<double>(now - m_speedSamples.front().time).count();
            if (age > 3.0) {
                m_speedSamples.pop_front();
            } else {
                break;
            }
        }

        if (m_speedSamples.size() >= 2) {
            double dt = std::chrono::duration<double>(now - m_speedSamples.front().time).count();
            if (dt >= 0.25) {
                uint64_t db = (snap.downloadedBytes >= m_speedSamples.front().downloadedBytes)
                    ? (snap.downloadedBytes - m_speedSamples.front().downloadedBytes) : 0;
                m_speed3sBps = static_cast<double>(db) / dt;
            }
        } else {
            m_speed3sBps = instantSum;
        }

        snap.currentSpeedBps = m_speed3sBps;
    } else {
        m_speedSamples.clear();
        m_speed3sBps = 0.0;
        snap.currentSpeedBps = 0.0;
    }
    return snap;
}

void DownloadTask::Start() {
    DownloadState curState = m_state.load();
    if (curState == DownloadState::Downloading || curState == DownloadState::Connecting) {
        return;
    }
    if (curState == DownloadState::Completed) {
        return;
    }

    if (m_isMediaTask) {
        m_retryCount = 0;
        m_isFinishing = false;
        m_isMediaCancelled = false;
        m_state = DownloadState::Connecting;
        AddLog(L"Video indirme işlemi başlatılıyor...", 0);

        if (m_mediaThread.joinable() && m_mediaThread.get_id() != std::this_thread::get_id()) {
            m_mediaThread.join();
        }
        m_mediaThread = std::thread(&DownloadTask::RunMediaDownload, this);
        return;
    }

    m_retryCount = 0;
    m_isFinishing = false;
    m_state = DownloadState::Connecting;
    AddLog(L"İndirme başlatılıyor...", 0);

    // Try loading existing state file first
    LoadState();

    if (m_probeThread.joinable()) {
        m_probeThread.join();
    }
    m_probeThread = std::thread(&DownloadTask::ProbeAndStart, this);
}

void DownloadTask::ProbeAndStart() {
    m_isProbing = true;

    // Check if we need to probe server for size/ranges
    if (m_info.totalBytes == 0 || m_info.segments.empty()) {
        AddLog(L"Sunucu bilgileri sorgulanıyor (HEAD / Range test)...", 0);

        std::wstring probeUrl = WinHttpUtils::TrimUrl(m_info.url);
        m_resolvedUrl = probeUrl;

        int maxRedirects = 10;
        uint64_t totalLength = 0;
        bool supportsRange = false;
        bool probeSuccess = false;

        while (maxRedirects-- > 0) {
            UrlParts parts;
            if (!WinHttpUtils::ParseUrl(probeUrl, parts)) {
                m_state = DownloadState::Failed;
                m_info.errorMsg = L"Geçersiz URL formatı: " + probeUrl;
                AddLog(m_info.errorMsg, 3);
                m_isProbing = false;
                if (m_eventCb) m_eventCb(m_info.id);
                return;
            }

            HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36",
                                            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                            WINHTTP_NO_PROXY_NAME,
                                            WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSession) {
                m_state = DownloadState::Failed;
                m_info.errorMsg = L"WinHttp başlatılamadı";
                AddLog(m_info.errorMsg, 3);
                m_isProbing = false;
                if (m_eventCb) m_eventCb(m_info.id);
                return;
            }

            DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
            WinHttpSetOption(hSession, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

            DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
            WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));

            WinHttpSetTimeouts(hSession, 15000, 15000, 20000, 20000);

            HINTERNET hConnect = WinHttpConnect(hSession, parts.host.c_str(), parts.port, 0);
            if (!hConnect) {
                WinHttpCloseHandle(hSession);
                int maxRetries = Config::Instance().maxRetryAttempts;
                if (m_retryCount < maxRetries) {
                    m_retryCount++;
                    AddLog(L"Sunucuya bağlanılamadı (" + parts.host + L"), yeniden deneniyor (" +
                           std::to_wstring(m_retryCount) + L"/" + std::to_wstring(maxRetries) + L")...", 2);
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    continue;
                }
                m_state = DownloadState::Failed;
                m_info.errorMsg = L"Sunucuya bağlanılamadı: " + parts.host;
                AddLog(m_info.errorMsg, 3);
                m_isProbing = false;
                if (m_eventCb) m_eventCb(m_info.id);
                return;
            }

            DWORD reqFlags = parts.isHttps ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", parts.path.c_str(), NULL,
                                                   WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, reqFlags);
            if (!hRequest) {
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                int maxRetries = Config::Instance().maxRetryAttempts;
                if (m_retryCount < maxRetries) {
                    m_retryCount++;
                    AddLog(L"İstek açılamadı, yeniden deneniyor (" +
                           std::to_wstring(m_retryCount) + L"/" + std::to_wstring(maxRetries) + L")...", 2);
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    continue;
                }
                m_state = DownloadState::Failed;
                m_info.errorMsg = L"İstek açılamadı";
                AddLog(m_info.errorMsg, 3);
                m_isProbing = false;
                if (m_eventCb) m_eventCb(m_info.id);
                return;
            }

            if (parts.isHttps) {
                DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                                 SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                                 SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                                 SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
                WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
            }

            // Browser headers
            WinHttpAddRequestHeaders(hRequest, L"Accept: */*", (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);
            WinHttpAddRequestHeaders(hRequest, L"Accept-Language: en-US,en;q=0.9,tr;q=0.8", (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);
            WinHttpAddRequestHeaders(hRequest, L"Connection: Keep-Alive", (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);

            // Test range support with Range: bytes=0-0
            WinHttpAddRequestHeaders(hRequest, L"Range: bytes=0-0", (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);

            if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
                !WinHttpReceiveResponse(hRequest, NULL))
            {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                int maxRetries = Config::Instance().maxRetryAttempts;
                if (m_retryCount < maxRetries) {
                    m_retryCount++;
                    AddLog(L"Sunucu yanıt vermedi, yeniden deneniyor (" +
                           std::to_wstring(m_retryCount) + L"/" + std::to_wstring(maxRetries) + L")...", 2);
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    continue;
                }
                m_state = DownloadState::Failed;
                m_info.errorMsg = L"Sunucu yanıt vermedi";
                AddLog(m_info.errorMsg, 3);
                m_isProbing = false;
                if (m_eventCb) m_eventCb(m_info.id);
                return;
            }

            DWORD statusCode = 0;
            DWORD statusSize = sizeof(statusCode);
            WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

            wchar_t resolvedUrlBuf[8192] = {0};
            DWORD resolvedUrlSize = sizeof(resolvedUrlBuf);
            if (WinHttpQueryOption(hRequest, WINHTTP_OPTION_URL, resolvedUrlBuf, &resolvedUrlSize)) {
                m_resolvedUrl = resolvedUrlBuf;
            }

            // Redirect handling (301, 302, 303, 307, 308)
            if (statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307 || statusCode == 308) {
                wchar_t locBuf[4096] = { 0 };
                DWORD locSize = sizeof(locBuf);
                if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                                        locBuf, &locSize, WINHTTP_NO_HEADER_INDEX)) {
                    std::wstring nextUrl = WinHttpUtils::CombineUrl(probeUrl, locBuf);
                    AddLog(L"Yönlendirme takip ediliyor: " + nextUrl, 0);
                    probeUrl = nextUrl;
                    m_resolvedUrl = nextUrl;

                    if (!m_customFilenameSet) {
                        std::wstring newFn = WinHttpUtils::ExtractFilenameFromUrl(nextUrl);
                        if (!newFn.empty() && newFn != L"download.dat") {
                            std::lock_guard<std::recursive_mutex> lock(m_mutex);
                            m_info.filename = newFn;
                            m_info.fullPath = m_info.saveDirectory + L"\\" + m_info.filename;
                        }
                    }

                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    continue; // Loop to probe the redirect target!
                }
            }

            // Check Content-Disposition for filename
            if (!m_customFilenameSet) {
                wchar_t dispBuf[1024] = { 0 };
                DWORD dispSize = sizeof(dispBuf);
                if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_DISPOSITION, WINHTTP_HEADER_NAME_BY_INDEX,
                                        dispBuf, &dispSize, WINHTTP_NO_HEADER_INDEX)) {
                    std::wstring cdFn = WinHttpUtils::ExtractFilenameFromHeader(dispBuf);
                    if (!cdFn.empty()) {
                        std::lock_guard<std::recursive_mutex> lock(m_mutex);
                        m_info.filename = cdFn;
                        m_info.fullPath = m_info.saveDirectory + L"\\" + m_info.filename;
                    }
                }
            }

            if (statusCode == 206) {
                supportsRange = true;
                wchar_t rangeBuf[256] = { 0 };
                DWORD rangeSize = sizeof(rangeBuf);
                if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_RANGE, WINHTTP_HEADER_NAME_BY_INDEX,
                                        rangeBuf, &rangeSize, WINHTTP_NO_HEADER_INDEX)) {
                    std::wstring cr = rangeBuf;
                    size_t slashPos = cr.find(L'/');
                    if (slashPos != std::wstring::npos) {
                        totalLength = _wcstoui64(cr.c_str() + slashPos + 1, NULL, 10);
                    }
                }
                probeSuccess = true;
            } else if (statusCode == 200) {
                supportsRange = false;
                wchar_t clBuf[128] = { 0 };
                DWORD clSize = sizeof(clBuf);
                if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX,
                                        clBuf, &clSize, WINHTTP_NO_HEADER_INDEX)) {
                    totalLength = _wcstoui64(clBuf, NULL, 10);
                }
                probeSuccess = true;
            } else {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                m_state = DownloadState::Failed;
                m_info.errorMsg = L"HTTP Hatası: " + std::to_wstring(statusCode);
                AddLog(m_info.errorMsg, 3);
                m_isProbing = false;
                if (m_eventCb) m_eventCb(m_info.id);
                return;
            }

            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            break;
        }

        if (!probeSuccess && totalLength == 0 && !supportsRange) {
            m_state = DownloadState::Failed;
            m_info.errorMsg = L"Sunucuya ulaşılamadı veya çok fazla yönlendirme";
            AddLog(m_info.errorMsg, 3);
            m_isProbing = false;
            if (m_eventCb) m_eventCb(m_info.id);
            return;
        }

        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_info.totalBytes = totalLength;
            m_info.supportsResume = supportsRange;

            // Divide into segments
            m_info.segments.clear();
            int numParts = supportsRange ? m_info.splitCount : 1;
            if (totalLength < 256 * 1024) {
                numParts = 1; // Small file: 1 part is enough
            }

            if (numParts > 1 && totalLength > 0) {
                uint64_t partSize = totalLength / numParts;
                for (int i = 0; i < numParts; ++i) {
                    SegmentInfo si;
                    si.id = i;
                    si.startByte = i * partSize;
                    si.endByte = (i == numParts - 1) ? (totalLength - 1) : ((i + 1) * partSize - 1);
                    si.currentOffset = si.startByte;
                    si.downloadedBytes = 0;
                    si.status = 0;
                    m_info.segments.push_back(si);
                }
            } else {
                SegmentInfo si;
                si.id = 0;
                si.startByte = 0;
                si.endByte = (totalLength > 0) ? (totalLength - 1) : 0;
                si.currentOffset = 0;
                si.downloadedBytes = 0;
                si.status = 0;
                m_info.segments.push_back(si);
            }
        }

        AddLog(L"Dosya boyutu: " + FormatBytes(totalLength) + 
               (supportsRange ? (L" (" + std::to_wstring(m_info.segments.size()) + L" parça)") : L" (Tek parça)"), 1);
    }

    // Ensure save directory exists
    CreateDirectoryW(m_info.saveDirectory.c_str(), NULL);

    // Open target .gety file and pre-allocate
    std::wstring tempFile = GetTempFilePath();
    m_hFile = CreateFileW(tempFile.c_str(),
                          GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          NULL,
                          OPEN_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL,
                          NULL);

    if (m_hFile == INVALID_HANDLE_VALUE) {
        m_state = DownloadState::Failed;
        m_info.errorMsg = L"Hedef dosya açılamadı: " + tempFile;
        AddLog(m_info.errorMsg, 3);
        m_isProbing = false;
        if (m_eventCb) m_eventCb(m_info.id);
        return;
    }

    // Pre-allocate file size
    if (m_info.totalBytes > 0) {
        LARGE_INTEGER li;
        li.QuadPart = m_info.totalBytes;
        SetFilePointerEx(m_hFile, li, NULL, FILE_BEGIN);
        SetEndOfFile(m_hFile);
    }

    m_isProbing = false;
    LaunchWorkers();
}

void DownloadTask::LaunchWorkers() {
    AddLog(L"Parça indirme iş parçacıkları başlatılıyor...", 0);

    std::vector<std::unique_ptr<SegmentWorker>> workers;
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (const auto& seg : m_info.segments) {
            auto worker = std::make_unique<SegmentWorker>(
                seg.id,
                m_info.url,
                seg.startByte,
                seg.endByte,
                seg.downloadedBytes,
                m_hFile,
                [this](const std::wstring& msg, int level) {
                    AddLog(msg, level);
                },
                [this](int segId, uint64_t bytes) {
                    OnSegmentProgress(segId, bytes);
                },
                [this](int segId, bool success, const std::wstring& err) {
                    OnSegmentFinished(segId, success, err);
                },
                m_info.supportsResume,
                m_resolvedUrl
            );
            workers.push_back(std::move(worker));
        }
        m_workers = std::move(workers);
        m_state = DownloadState::Downloading;
    }

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        for (auto& w : m_workers) {
            if (w) w->Start();
        }
    }

    if (m_eventCb) m_eventCb(m_info.id);
}

void DownloadTask::OnSegmentProgress(int segId, uint64_t bytes) {
    // Progress callback from worker
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (segId >= 0 && segId < (int)m_info.segments.size()) {
        m_info.segments[segId].downloadedBytes = bytes;
        m_info.segments[segId].currentOffset = m_info.segments[segId].startByte + bytes;
    }
}

void DownloadTask::OnSegmentFinished(int segId, bool success, const std::wstring& err) {
    if (!success) {
        AddLog(L"Parça " + std::to_wstring(segId + 1) + L" başarısız: " + err, 3);
    }
    CheckAllFinished();
}

void DownloadTask::CheckAllFinished() {
    if (m_state.load() != DownloadState::Downloading) {
        return;
    }

    bool allDone = true;
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_workers.empty()) return;
        for (const auto& w : m_workers) {
            if (w) {
                SegmentInfo si = w->GetInfo();
                uint64_t totalPartBytes = si.totalBytes();
                if (totalPartBytes > 0 && si.downloadedBytes < totalPartBytes) {
                    allDone = false;
                    break;
                }
            }
        }
    }

    if (!allDone) {
        bool allStopped = true;
        bool anyFailed = false;
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            for (const auto& w : m_workers) {
                if (w && w->IsRunning()) {
                    allStopped = false;
                    break;
                }
                if (w && w->GetInfo().status == 4) {
                    anyFailed = true;
                }
            }
        }
        if (allStopped && anyFailed) {
            if (m_hFile != INVALID_HANDLE_VALUE) {
                CloseHandle(m_hFile);
                m_hFile = INVALID_HANDLE_VALUE;
            }

            int maxRetries = Config::Instance().maxRetryAttempts;
            if (m_retryCount < maxRetries) {
                m_retryCount++;
                AddLog(L"Parça hatası oluştu, yeniden deneniyor (" +
                       std::to_wstring(m_retryCount) + L"/" + std::to_wstring(maxRetries) + L")...", 2);
                m_state = DownloadState::Connecting;
                if (m_eventCb) m_eventCb(m_info.id);

                std::thread([this]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    if (m_state.load() == DownloadState::Connecting) {
                        ProbeAndStart();
                    }
                }).detach();
                return;
            }

            m_state = DownloadState::Failed;
            m_info.errorMsg = L"İndirme başarısız oldu - parçalar hata verdi (Yeniden deneme sınırı aşıldı)";
            AddLog(m_info.errorMsg, 3);
            if (m_eventCb) m_eventCb(m_info.id);
        }
    }

    if (allDone) {
        m_retryCount = 0;
        bool expected = false;
        if (!m_isFinishing.compare_exchange_strong(expected, true)) {
            return;
        }

        // Close file handle
        if (m_hFile != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(m_hFile);
            CloseHandle(m_hFile);
            m_hFile = INVALID_HANDLE_VALUE;
        }

        // Move .gety to final filename
        std::wstring tempFile = GetTempFilePath();
        std::wstring finalFile = m_info.fullPath;

        // If target exists, delete or replace
        DeleteFileW(finalFile.c_str());
        if (MoveFileW(tempFile.c_str(), finalFile.c_str())) {
            // Remove state file
            DeleteFileW(GetStateFilePath().c_str());
        }

        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_state = DownloadState::Completed;
            m_info.completedDate = GetCurrentDateString();
            m_info.downloadedBytes = m_info.totalBytes;
            m_info.currentSpeedBps = 0.0;
        }

        AddLog(L"Tebrikler! İndirme başarıyla tamamlandı: " + m_info.filename, 1);

        // Sound alert
        if (Config::Instance().playSoundOnComplete) {
            PlaySoundW(L"SystemAsterisk", NULL, SND_ALIAS | SND_ASYNC);
        }

        SaveState();

        if (m_eventCb) m_eventCb(m_info.id);
    }
}

void DownloadTask::Pause() {
    if (m_state.load() != DownloadState::Downloading && m_state.load() != DownloadState::Connecting) {
        return;
    }

    if (m_isMediaTask) {
        AddLog(L"Video indirme duraklatılıyor...", 2);
        m_isMediaCancelled = true;
        if (m_hMediaProcess != NULL) {
            TerminateProcess(m_hMediaProcess, 0);
        }
        m_state = DownloadState::Paused;
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_info.currentSpeedBps = 0.0;
        }
        AddLog(L"Video indirme duraklatıldı.", 2);
        if (m_eventCb) m_eventCb(m_info.id);
        return;
    }

    AddLog(L"İndirme duraklatılıyor...", 2);
    m_state = DownloadState::Paused;

    std::vector<std::unique_ptr<SegmentWorker>> workersToStop;
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_info.downloadedBytes = 0;
        for (size_t i = 0; i < m_workers.size() && i < m_info.segments.size(); ++i) {
            if (m_workers[i]) {
                SegmentInfo si = m_workers[i]->GetInfo();
                m_info.segments[i] = si;
                m_info.downloadedBytes += si.downloadedBytes;
            }
        }
        m_info.currentSpeedBps = 0.0;
        workersToStop = std::move(m_workers);
        m_workers.clear();
    }

    for (auto& w : workersToStop) {
        if (w) w->Stop();
    }

    if (m_hFile != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(m_hFile);
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
    }

    SaveState();
    AddLog(L"İndirme duraklatıldı.", 2);

    if (m_eventCb) m_eventCb(m_info.id);
}

void DownloadTask::Stop() {
    if (m_isMediaTask) {
        m_isMediaCancelled = true;
        HANDLE hProc = NULL;
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            hProc = m_hMediaProcess;
        }
        if (hProc != NULL) {
            TerminateProcess(hProc, 0);
        }
    }
    Pause();
}

void DownloadTask::RunMediaDownload() {
    std::wstring ytDlpPath = MediaExtractor::GetYtDlpPath();
    if (ytDlpPath.empty()) {
        m_state = DownloadState::Failed;
        m_info.errorMsg = L"yt-dlp.exe bulunamadı. Lütfen tools klasörüne yerleştirin.";
        AddLog(m_info.errorMsg, 3);
        if (m_eventCb) m_eventCb(m_info.id);
        return;
    }

    CreateDirectoryW(m_info.saveDirectory.c_str(), NULL);

    // Build command line
    std::wstring cmd = L"\"" + ytDlpPath + L"\" --newline --no-playlist";
    cmd += L" --progress-template \"download:%(progress._percent_str)s|%(progress._speed_str)s|%(progress._eta_str)s|%(progress._total_bytes_str)s|%(progress._downloaded_bytes_str)s\"";

    if (m_mediaFormat == L"mp3") {
        cmd += L" -x --audio-format mp3 -f \"bestaudio/best\"";
    } else if (m_mediaFormat == L"m4a") {
        cmd += L" -x --audio-format m4a -f \"bestaudio/best\"";
    } else if (!m_mediaFormat.empty()) {
        cmd += L" -f \"" + m_mediaFormat + L"\" --merge-output-format mp4";
    } else {
        cmd += L" -f \"bestvideo+bestaudio/best\" --merge-output-format mp4";
    }

    std::wstring ffmpegPath = MediaExtractor::GetFfmpegPath();
    if (!ffmpegPath.empty()) {
        cmd += L" --ffmpeg-location \"" + ffmpegPath + L"\"";
    }

    std::wstring outputTemplate;
    std::wstring chosenFilename;
    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        chosenFilename = m_info.filename;
    }

    if (m_customFilenameSet && !chosenFilename.empty()) {
        std::wstring sanitized = chosenFilename;
        for (auto& c : sanitized) {
            if (c == L'/' || c == L'\\' || c == L':' || c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|') {
                c = L'_';
            }
        }
        while (!sanitized.empty() && (sanitized.back() == L' ' || sanitized.back() == L'.')) {
            sanitized.pop_back();
        }

        size_t dot = sanitized.find_last_of(L'.');
        if (dot != std::wstring::npos && dot > 0) {
            outputTemplate = m_info.saveDirectory + L"\\" + sanitized;
        } else {
            outputTemplate = m_info.saveDirectory + L"\\" + sanitized + L".%(ext)s";
        }
    } else {
        outputTemplate = m_info.saveDirectory + L"\\%(title)s.%(ext)s";
    }

    cmd += L" -o \"" + outputTemplate + L"\"";
    cmd += L" \"" + m_info.url + L"\"";

    HANDLE hStdOutRead = NULL;
    HANDLE hStdOutWrite = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };

    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
        m_state = DownloadState::Failed;
        m_info.errorMsg = L"Boru (pipe) oluşturulamadı.";
        AddLog(m_info.errorMsg, 3);
        if (m_eventCb) m_eventCb(m_info.id);
        return;
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

    CloseHandle(hStdOutWrite);

    if (!created) {
        CloseHandle(hStdOutRead);
        m_state = DownloadState::Failed;
        m_info.errorMsg = L"yt-dlp işlemi başlatılamadı.";
        AddLog(m_info.errorMsg, 3);
        if (m_eventCb) m_eventCb(m_info.id);
        return;
    }

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_hMediaProcess = pi.hProcess;
        m_state = DownloadState::Downloading;
    }
    if (m_eventCb) m_eventCb(m_info.id);

    char buffer[4096];
    DWORD bytesRead = 0;
    std::string lineAcc;

    while (ReadFile(hStdOutRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        for (DWORD i = 0; i < bytesRead; ++i) {
            char c = buffer[i];
            if (c == '\r') continue;
            if (c == '\n') {
                if (!lineAcc.empty()) {
                    bool isProgress = false;
                    std::string progStr;
                    if (lineAcc.find("GetyProgress:") != std::string::npos) {
                        size_t pos = lineAcc.find("GetyProgress:");
                        progStr = lineAcc.substr(pos + 13);
                        isProgress = true;
                    } else if (lineAcc.rfind("download:", 0) == 0) {
                        progStr = lineAcc.substr(9);
                        isProgress = true;
                    } else if (std::count(lineAcc.begin(), lineAcc.end(), '|') >= 4) {
                        progStr = lineAcc;
                        isProgress = true;
                    }

                    if (isProgress) {
                        std::stringstream ss(progStr);
                        std::string pPct, pSpeed, pEta, pTotal, pDown;
                        if (std::getline(ss, pPct, '|') &&
                            std::getline(ss, pSpeed, '|') &&
                            std::getline(ss, pEta, '|') &&
                            std::getline(ss, pTotal, '|') &&
                            std::getline(ss, pDown))
                        {
                            uint64_t curDown = ParseMediaBytes(pDown);
                            uint64_t curTotal = ParseMediaBytes(pTotal);
                            double curSpeed = ParseMediaSpeed(pSpeed);

                            std::lock_guard<std::recursive_mutex> lock(m_mutex);
                            if (curDown > 0) m_info.downloadedBytes = curDown;
                            if (curTotal > 0) m_info.totalBytes = curTotal;
                            if (curSpeed > 0.0) m_info.currentSpeedBps = curSpeed;

                            if (m_info.segments.empty()) {
                                SegmentInfo si;
                                si.id = 0;
                                m_info.segments.push_back(si);
                            }
                            m_info.segments[0].startByte = 0;
                            m_info.segments[0].endByte = (m_info.totalBytes > 0) ? m_info.totalBytes - 1 : 0;
                            m_info.segments[0].downloadedBytes = m_info.downloadedBytes;
                            m_info.segments[0].speedBps = m_info.currentSpeedBps;
                            m_info.segments[0].status = 2;
                        }
                    } else if (lineAcc.find("[download]") != std::string::npos && lineAcc.find("%") != std::string::npos && lineAcc.find(" at ") != std::string::npos) {
                        // Fallback parser for standard yt-dlp progress
                        size_t pctPos = lineAcc.find("%");
                        size_t ofPos = lineAcc.find(" of ", pctPos);
                        size_t atPos = lineAcc.find(" at ", ofPos);
                        size_t etaPos = lineAcc.find(" ETA", atPos);
                        if (pctPos != std::string::npos && ofPos != std::string::npos) {
                            std::string totalStr = (atPos != std::string::npos) ? lineAcc.substr(ofPos + 4, atPos - (ofPos + 4)) : "";
                            std::string speedStr = (atPos != std::string::npos && etaPos != std::string::npos) ? lineAcc.substr(atPos + 4, etaPos - (atPos + 4)) : "";
                            size_t tilde = totalStr.find('~');
                            if (tilde != std::string::npos) totalStr.erase(tilde, 1);

                            uint64_t curTotal = ParseMediaBytes(totalStr);
                            double curSpeed = ParseMediaSpeed(speedStr);

                            size_t spaceBeforePct = lineAcc.rfind(' ', pctPos);
                            double pct = 0.0;
                            if (spaceBeforePct != std::string::npos) {
                                pct = std::atof(lineAcc.substr(spaceBeforePct + 1, pctPos - spaceBeforePct - 1).c_str());
                            }
                            uint64_t curDown = (curTotal > 0 && pct > 0.0) ? static_cast<uint64_t>((pct / 100.0) * curTotal) : 0;

                            std::lock_guard<std::recursive_mutex> lock(m_mutex);
                            if (curDown > 0) m_info.downloadedBytes = curDown;
                            if (curTotal > 0) m_info.totalBytes = curTotal;
                            if (curSpeed > 0.0) m_info.currentSpeedBps = curSpeed;

                            if (m_info.segments.empty()) {
                                SegmentInfo si;
                                si.id = 0;
                                m_info.segments.push_back(si);
                            }
                            m_info.segments[0].startByte = 0;
                            m_info.segments[0].endByte = (m_info.totalBytes > 0) ? m_info.totalBytes - 1 : 0;
                            m_info.segments[0].downloadedBytes = m_info.downloadedBytes;
                            m_info.segments[0].speedBps = m_info.currentSpeedBps;
                            m_info.segments[0].status = 2;
                        }
                    } else if (lineAcc.find("Destination: ") != std::string::npos) {
                        size_t pos = lineAcc.find("Destination: ") + 13;
                        std::string destStr = lineAcc.substr(pos);
                        size_t first = destStr.find_first_not_of(" \t\r\n\"'");
                        size_t last = destStr.find_last_not_of(" \t\r\n\"'");
                        if (first != std::string::npos && last != std::string::npos) {
                            destStr = destStr.substr(first, last - first + 1);
                        }
                        std::wstring wDest = MediaExtractor::Utf8ToWide(destStr);
                        size_t slash = wDest.find_last_of(L"/\\");
                        std::wstring cleanFilename = (slash != std::wstring::npos) ? wDest.substr(slash + 1) : wDest;
                        {
                            std::lock_guard<std::recursive_mutex> lock(m_mutex);
                            if (m_customFilenameSet) {
                                size_t extDot = cleanFilename.find_last_of(L'.');
                                std::wstring ext = (extDot != std::wstring::npos) ? cleanFilename.substr(extDot) : L"";
                                std::wstring baseStem = m_info.filename;
                                size_t stemDot = baseStem.find_last_of(L'.');
                                if (stemDot != std::wstring::npos && stemDot > 0) {
                                    baseStem = baseStem.substr(0, stemDot);
                                }
                                if (!ext.empty()) {
                                    m_info.filename = baseStem + ext;
                                }
                                m_info.fullPath = m_info.saveDirectory + L"\\" + m_info.filename;
                            } else {
                                m_info.filename = cleanFilename;
                                m_info.fullPath = m_info.saveDirectory + L"\\" + m_info.filename;
                            }
                        }
                        AddLog(L"Hedef dosya belirlendi: " + m_info.filename, 0);
                    } else if (lineAcc.find("[Merger] Merging formats into \"") != std::string::npos) {
                        size_t start = lineAcc.find("[Merger] Merging formats into \"") + 31;
                        size_t end = lineAcc.find('"', start);
                        if (end != std::string::npos) {
                            std::string destStr = lineAcc.substr(start, end - start);
                            std::wstring wDest = MediaExtractor::Utf8ToWide(destStr);
                            size_t slash = wDest.find_last_of(L"/\\");
                            std::wstring cleanFilename = (slash != std::wstring::npos) ? wDest.substr(slash + 1) : wDest;
                            {
                                std::lock_guard<std::recursive_mutex> lock(m_mutex);
                                if (m_customFilenameSet) {
                                    size_t extDot = cleanFilename.find_last_of(L'.');
                                    std::wstring ext = (extDot != std::wstring::npos) ? cleanFilename.substr(extDot) : L"";
                                    std::wstring baseStem = m_info.filename;
                                    size_t stemDot = baseStem.find_last_of(L'.');
                                    if (stemDot != std::wstring::npos && stemDot > 0) {
                                        baseStem = baseStem.substr(0, stemDot);
                                    }
                                    if (!ext.empty()) {
                                        m_info.filename = baseStem + ext;
                                    }
                                    m_info.fullPath = m_info.saveDirectory + L"\\" + m_info.filename;
                                } else {
                                    m_info.filename = cleanFilename;
                                    m_info.fullPath = m_info.saveDirectory + L"\\" + m_info.filename;
                                }
                            }
                            AddLog(L"Video ve ses birleştirildi: " + m_info.filename, 1);
                        }
                    } else if (lineAcc.find("[ExtractAudio] Destination: ") != std::string::npos) {
                        size_t pos = lineAcc.find("[ExtractAudio] Destination: ") + 28;
                        std::string destStr = lineAcc.substr(pos);
                        size_t first = destStr.find_first_not_of(" \t\r\n\"'");
                        size_t last = destStr.find_last_not_of(" \t\r\n\"'");
                        if (first != std::string::npos && last != std::string::npos) {
                            destStr = destStr.substr(first, last - first + 1);
                        }
                        std::wstring wDest = MediaExtractor::Utf8ToWide(destStr);
                        size_t slash = wDest.find_last_of(L"/\\");
                        std::wstring cleanFilename = (slash != std::wstring::npos) ? wDest.substr(slash + 1) : wDest;
                        {
                            std::lock_guard<std::recursive_mutex> lock(m_mutex);
                            if (m_customFilenameSet) {
                                size_t extDot = cleanFilename.find_last_of(L'.');
                                std::wstring ext = (extDot != std::wstring::npos) ? cleanFilename.substr(extDot) : L"";
                                std::wstring baseStem = m_info.filename;
                                size_t stemDot = baseStem.find_last_of(L'.');
                                if (stemDot != std::wstring::npos && stemDot > 0) {
                                    baseStem = baseStem.substr(0, stemDot);
                                }
                                if (!ext.empty()) {
                                    m_info.filename = baseStem + ext;
                                }
                                m_info.fullPath = m_info.saveDirectory + L"\\" + m_info.filename;
                            } else {
                                m_info.filename = cleanFilename;
                                m_info.fullPath = m_info.saveDirectory + L"\\" + m_info.filename;
                            }
                        }
                        AddLog(L"Ses dosyası oluşturuldu: " + m_info.filename, 1);
                    } else if (lineAcc.rfind("ERROR:", 0) == 0 || lineAcc.rfind("WARNING:", 0) == 0) {
                        AddLog(MediaExtractor::Utf8ToWide(lineAcc), (lineAcc[0] == 'E') ? 3 : 2);
                    }
                    lineAcc.clear();
                }
            } else {
                lineAcc += c;
            }
        }
    }

    CloseHandle(hStdOutRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_hMediaProcess = NULL;
    }

    if (m_isMediaCancelled.load()) {
        m_state = DownloadState::Paused;
        AddLog(L"Video indirme kullanıcı tarafından duraklatıldı.", 2);
        if (m_eventCb) m_eventCb(m_info.id);
        return;
    }

    if (exitCode == 0) {
        {
            std::lock_guard<std::recursive_mutex> lock(m_mutex);
            m_state = DownloadState::Completed;
            m_info.completedDate = GetCurrentDateString();
            if (m_info.totalBytes > 0) m_info.downloadedBytes = m_info.totalBytes;
            m_info.currentSpeedBps = 0.0;
            if (m_info.category.empty() || m_info.category == L"Genel") {
                m_info.category = (m_mediaFormat == L"mp3" || m_mediaFormat == L"m4a") ? LStr(StrId::CatMusic) : LStr(StrId::CatVideo);
            }
            if (!m_info.segments.empty()) {
                m_info.segments[0].status = 3; // done
                m_info.segments[0].downloadedBytes = m_info.totalBytes;
            }
        }
        AddLog(L"Tebrikler! Video indirme başarıyla tamamlandı: " + m_info.filename, 1);

        if (Config::Instance().playSoundOnComplete) {
            PlaySoundW(L"SystemAsterisk", NULL, SND_ALIAS | SND_ASYNC);
        }
    } else {
        m_state = DownloadState::Failed;
        m_info.errorMsg = L"Video indirme hatası (Kod: " + std::to_wstring(exitCode) + L")";
        AddLog(m_info.errorMsg, 3);
    }

    if (m_eventCb) m_eventCb(m_info.id);
}

void DownloadTask::Redownload() {
    Stop();
    
    // Delete target and temp files
    DeleteFileW(GetTempFilePath().c_str());
    DeleteFileW(GetStateFilePath().c_str());
    DeleteFileW(m_info.fullPath.c_str());

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_info.downloadedBytes = 0;
        m_info.currentSpeedBps = 0.0;
        m_info.segments.clear();
        m_state = DownloadState::Queued;
    }

    AddLog(L"Görev sıfırlandı, yeniden indirilecek.", 0);
    Start();
}

void DownloadTask::DeleteTask(bool deleteFile) {
    Stop();

    if (deleteFile) {
        DeleteFileW(GetTempFilePath().c_str());
        DeleteFileW(GetStateFilePath().c_str());
        DeleteFileW(m_info.fullPath.c_str());
    }

    m_state = DownloadState::Deleted;
    if (m_eventCb) m_eventCb(m_info.id);
}

void DownloadTask::SaveState() {
    // State files disabled per user request - clean up any legacy state file if exists
    std::wstring stateFile = GetStateFilePath();
    if (PathFileExistsW(stateFile.c_str())) {
        DeleteFileW(stateFile.c_str());
    }
}

void DownloadTask::LoadState() {
    // State files disabled per user request
}

void DownloadTask::RestoreMetadata(const std::wstring& id,
                                   DownloadState state,
                                   const std::wstring& addedDate,
                                   const std::wstring& completedDate,
                                   const std::wstring& category) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!id.empty()) m_info.id = id;
    if (!category.empty()) m_info.category = category;
    if (!addedDate.empty()) m_info.addedDate = addedDate;
    if (!completedDate.empty()) m_info.completedDate = completedDate;

    if (state == DownloadState::Downloading || state == DownloadState::Connecting) {
        state = DownloadState::Paused;
    }
    m_state = state;
    m_info.state = state;

    if (state == DownloadState::Completed) {
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExW(m_info.fullPath.c_str(), GetFileExInfoStandard, &fad)) {
            LARGE_INTEGER size;
            size.LowPart = fad.nFileSizeLow;
            size.HighPart = fad.nFileSizeHigh;
            m_info.totalBytes = size.QuadPart;
            m_info.downloadedBytes = size.QuadPart;
        }
    }
}

std::vector<TaskLogEntry> DownloadTask::GetLogs() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_info.logs;
}

void DownloadTask::RestoreLogs(const std::vector<TaskLogEntry>& logs) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_info.logs = logs;
    if (m_info.logs.size() > 200) {
        m_info.logs.erase(m_info.logs.begin(), m_info.logs.end() - 200);
    }
}

} // namespace Gety
