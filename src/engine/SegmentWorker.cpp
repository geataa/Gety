#include "SegmentWorker.h"
#include "WinHttpUtils.h"
#include "../core/SpeedLimiter.h"
#include <vector>
#include <chrono>

namespace Gety {

SegmentWorker::SegmentWorker(int id,
                             const std::wstring& url,
                             uint64_t startByte,
                             uint64_t endByte,
                             uint64_t alreadyDownloaded,
                             HANDLE hFile,
                             LogCallback logCb,
                             ProgressCallback progCb,
                             FinishedCallback finishCb,
                             bool supportsRange,
                             const std::wstring& resolvedUrl)
    : m_id(id)
    , m_url(url)
    , m_resolvedUrl(resolvedUrl)
    , m_supportsRange(supportsRange)
    , m_startByte(startByte)
    , m_endByte(endByte)
    , m_downloadedBytes(alreadyDownloaded)
    , m_hFile(hFile)
    , m_logCb(logCb)
    , m_progCb(progCb)
    , m_finishCb(finishCb)
{
    if (m_downloadedBytes >= (m_endByte - m_startByte + 1) && m_endByte >= m_startByte) {
        m_status = 3; // Done
    }
}

SegmentWorker::~SegmentWorker() {
    Stop();
}

void SegmentWorker::Start() {
    if (m_isRunning.load()) return;
    m_stopRequested = false;
    m_isRunning = true;
    m_thread = std::thread(&SegmentWorker::WorkerProc, this);
}

void SegmentWorker::Stop() {
    m_stopRequested = true;
    HINTERNET req = m_hRequest.exchange(nullptr);
    if (req) WinHttpCloseHandle(req);
    HINTERNET conn = m_hConnect.exchange(nullptr);
    if (conn) WinHttpCloseHandle(conn);
    HINTERNET sess = m_hSession.exchange(nullptr);
    if (sess) WinHttpCloseHandle(sess);

    if (m_thread.joinable()) {
        m_thread.detach();
    }
    m_isRunning = false;
}

SegmentInfo SegmentWorker::GetInfo() const {
    SegmentInfo info;
    info.id = m_id;
    info.startByte = m_startByte;
    info.endByte = m_endByte;
    info.currentOffset = m_startByte + m_downloadedBytes.load();
    info.downloadedBytes = m_downloadedBytes.load();
    info.status = m_status.load();
    info.speedBps = m_speedBps.load();
    return info;
}

void SegmentWorker::WorkerProc() {
    uint64_t totalSegmentBytes = (m_endByte >= m_startByte) ? (m_endByte - m_startByte + 1) : 0;
    if (totalSegmentBytes > 0 && m_downloadedBytes.load() >= totalSegmentBytes) {
        m_status = 3; // done
        m_isRunning = false;
        if (m_finishCb) m_finishCb(m_id, true, L"");
        return;
    }

    m_status = 1; // connecting
    if (m_logCb) {
        m_logCb(L"Parça " + std::to_wstring(m_id + 1) + L" bağlanıyor...", 0);
    }

    std::wstring urlToUse = m_resolvedUrl.empty() ? m_url : m_resolvedUrl;
    int maxWorkerRedirects = 10;
    bool connected = false;
    DWORD statusCode = 0;

    auto closeAllHandles = [this]() {
        HINTERNET r = m_hRequest.exchange(nullptr);
        if (r) WinHttpCloseHandle(r);
        HINTERNET c = m_hConnect.exchange(nullptr);
        if (c) WinHttpCloseHandle(c);
        HINTERNET s = m_hSession.exchange(nullptr);
        if (s) WinHttpCloseHandle(s);
    };

    while (maxWorkerRedirects-- > 0) {
        if (m_stopRequested.load()) {
            m_status = 0;
            m_isRunning = false;
            return;
        }

        UrlParts parts;
        if (!WinHttpUtils::ParseUrl(urlToUse, parts)) {
            m_status = 4;
            m_isRunning = false;
            if (m_finishCb) m_finishCb(m_id, false, L"Geçersiz URL: " + urlToUse);
            return;
        }

        HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            m_status = 4;
            m_isRunning = false;
            if (m_finishCb) m_finishCb(m_id, false, L"WinHttpOpen başarısız");
            return;
        }
        m_hSession = hSession;

        DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(hSession, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

        DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
        WinHttpSetOption(hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));

        WinHttpSetTimeouts(hSession, 15000, 15000, 20000, 20000);

        HINTERNET hConnect = WinHttpConnect(hSession, parts.host.c_str(), parts.port, 0);
        if (!hConnect) {
            closeAllHandles();
            m_status = 4;
            m_isRunning = false;
            if (m_finishCb) m_finishCb(m_id, false, L"Sunucuya bağlanılamadı: " + parts.host);
            return;
        }
        m_hConnect = hConnect;

        DWORD reqFlags = parts.isHttps ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", parts.path.c_str(), NULL,
                                               WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, reqFlags);
        if (!hRequest) {
            closeAllHandles();
            m_status = 4;
            m_isRunning = false;
            if (m_finishCb) m_finishCb(m_id, false, L"İstek oluşturulamadı");
            return;
        }
        m_hRequest = hRequest;

        if (parts.isHttps) {
            DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                             SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                             SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                             SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
        }

        if (!parts.username.empty()) {
            WinHttpSetCredentials(hRequest, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_BASIC,
                                  parts.username.c_str(), parts.password.c_str(), NULL);
        }

        uint64_t curStart = m_startByte + m_downloadedBytes.load();
        if (m_supportsRange && totalSegmentBytes > 0) {
            std::wstring rangeHdr = L"Range: bytes=" + std::to_wstring(curStart) + L"-" + std::to_wstring(m_endByte);
            WinHttpAddRequestHeaders(hRequest, rangeHdr.c_str(), (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }

        WinHttpAddRequestHeaders(hRequest, L"Accept: */*", (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);
        WinHttpAddRequestHeaders(hRequest, L"Accept-Language: en-US,en;q=0.9,tr;q=0.8", (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);
        WinHttpAddRequestHeaders(hRequest, L"Connection: Keep-Alive", (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);

        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            closeAllHandles();
            m_status = 4;
            m_isRunning = false;
            if (m_finishCb) m_finishCb(m_id, false, L"İstek gönderilemedi");
            return;
        }

        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            closeAllHandles();
            m_status = 4;
            m_isRunning = false;
            if (m_finishCb) m_finishCb(m_id, false, L"Yanıt alınamadı");
            return;
        }

        statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

        if (statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307 || statusCode == 308) {
            wchar_t locBuf[4096] = { 0 };
            DWORD locSize = sizeof(locBuf);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                                    locBuf, &locSize, WINHTTP_NO_HEADER_INDEX)) {
                urlToUse = WinHttpUtils::CombineUrl(urlToUse, locBuf);
                m_resolvedUrl = urlToUse;
                closeAllHandles();
                continue;
            }
        }

        connected = true;
        break;
    }

    if (!connected || (statusCode != 200 && statusCode != 206)) {
        closeAllHandles();
        m_status = 4;
        m_isRunning = false;
        std::wstring err = L"HTTP Durum Kodu: " + std::to_wstring(statusCode);
        if (m_finishCb) m_finishCb(m_id, false, err);
        return;
    }

    m_status = 2; // downloading
    if (m_logCb) {
        m_logCb(L"Parça " + std::to_wstring(m_id + 1) + L" veri akışı başladı (" + 
                std::to_wstring(statusCode) + L")", 1);
    }

    const DWORD BUFFER_SIZE = 64 * 1024; // 64 KB buffer
    std::vector<BYTE> buffer(BUFFER_SIZE);

    auto speedStartTime = std::chrono::steady_clock::now();
    uint64_t bytesSinceSpeedCheck = 0;

    bool errorOccurred = false;
    std::wstring errorStr;

    while (!m_stopRequested.load()) {
        HINTERNET hReq = m_hRequest.load();
        if (!hReq) break;

        DWORD bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(hReq, &bytesAvailable)) {
            break;
        }

        if (bytesAvailable == 0) {
            // EOF reached
            break;
        }

        DWORD bytesToRead = std::min(bytesAvailable, BUFFER_SIZE);
        
        // If we have an endByte, don't read beyond endByte
        if (totalSegmentBytes > 0) {
            uint64_t remainingInSegment = totalSegmentBytes - m_downloadedBytes.load();
            if (bytesToRead > remainingInSegment) {
                bytesToRead = (DWORD)remainingInSegment;
            }
            if (bytesToRead == 0) {
                break;
            }
        }

        // Apply Speed Limiter
        SpeedLimiter::Instance().Throttle(bytesToRead);

        DWORD bytesRead = 0;
        if (!WinHttpReadData(hReq, buffer.data(), bytesToRead, &bytesRead)) {
            errorOccurred = true;
            errorStr = L"Veri okuma hatası";
            break;
        }

        if (bytesRead == 0) {
            break;
        }

        // Write directly to file at current offset
        uint64_t writeOffset = m_startByte + m_downloadedBytes.load();
        OVERLAPPED ov = { 0 };
        ov.Offset = (DWORD)(writeOffset & 0xFFFFFFFF);
        ov.OffsetHigh = (DWORD)(writeOffset >> 32);

        DWORD bytesWritten = 0;
        if (!WriteFile(m_hFile, buffer.data(), bytesRead, &bytesWritten, &ov) || bytesWritten != bytesRead) {
            errorOccurred = true;
            errorStr = L"Diske yazma hatası";
            break;
        }

        m_downloadedBytes += bytesRead;
        bytesSinceSpeedCheck += bytesRead;

        // Speed calculation every 250ms
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - speedStartTime).count();
        if (elapsed >= 0.25) {
            m_speedBps = (double)bytesSinceSpeedCheck / elapsed;
            speedStartTime = now;
            bytesSinceSpeedCheck = 0;
        }

        if (m_progCb) {
            m_progCb(m_id, m_downloadedBytes.load());
        }

        if (totalSegmentBytes > 0 && m_downloadedBytes.load() >= totalSegmentBytes) {
            break;
        }
    }

    if (!m_stopRequested.load() && !errorOccurred && totalSegmentBytes > 0 && m_downloadedBytes.load() < totalSegmentBytes) {
        errorOccurred = true;
        errorStr = L"Bağlantı erken kesildi";
    }

    closeAllHandles();

    m_speedBps = 0.0;
    m_isRunning = false;

    if (m_stopRequested.load()) {
        m_status = 0; // idle/stopped
        if (m_logCb) m_logCb(L"Parça " + std::to_wstring(m_id + 1) + L" duraklatıldı.", 2);
    } else if (errorOccurred) {
        m_status = 4; // error
        if (m_logCb) m_logCb(L"Parça " + std::to_wstring(m_id + 1) + L" hata: " + errorStr, 3);
        if (m_finishCb) m_finishCb(m_id, false, errorStr);
    } else {
        m_status = 3; // done
        if (m_logCb) m_logCb(L"Parça " + std::to_wstring(m_id + 1) + L" başarıyla tamamlandı!", 1);
        if (m_finishCb) m_finishCb(m_id, true, L"");
    }
}

} // namespace Gety
