#pragma once

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <cstdint>
#include <algorithm>
#include "../core/I18n.h"

#pragma comment(lib, "winhttp.lib")

namespace Gety {

struct UrlParts {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTP_PORT;
    bool isHttps = false;
    std::wstring username;
    std::wstring password;
};

class WinHttpUtils {
public:
    static bool ParseUrl(const std::wstring& url, UrlParts& parts) {
        URL_COMPONENTS urlComp = { 0 };
        urlComp.dwStructSize = sizeof(urlComp);

        wchar_t hostName[512] = { 0 };
        wchar_t urlPath[8192] = { 0 };
        wchar_t userName[256] = { 0 };
        wchar_t password[256] = { 0 };

        urlComp.lpszHostName = hostName;
        urlComp.dwHostNameLength = _countof(hostName);
        urlComp.lpszUrlPath = urlPath;
        urlComp.dwUrlPathLength = _countof(urlPath);
        urlComp.lpszUserName = userName;
        urlComp.dwUserNameLength = _countof(userName);
        urlComp.lpszPassword = password;
        urlComp.dwPasswordLength = _countof(password);

        if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp)) {
            return false;
        }

        parts.host = hostName;
        parts.path = urlPath;
        parts.port = urlComp.nPort;
        parts.isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
        parts.username = userName;
        parts.password = password;

        if (parts.path.empty()) {
            parts.path = L"/";
        }
        return true;
    }

    static std::wstring ExtractFilenameFromUrl(const std::wstring& url) {
        UrlParts parts;
        if (!ParseUrl(url, parts)) return L"download.dat";

        std::wstring path = parts.path;
        size_t queryPos = path.find(L'?');
        if (queryPos != std::wstring::npos) {
            path = path.substr(0, queryPos);
        }

        size_t slashPos = path.find_last_of(L"/\\");
        if (slashPos != std::wstring::npos && slashPos + 1 < path.length()) {
            std::wstring filename = path.substr(slashPos + 1);
            if (!filename.empty()) {
                return CleanFilename(filename);
            }
        }
        return L"download.dat";
    }

    static std::wstring ExtractFilenameFromHeader(const std::wstring& disposition) {
        // e.g. Content-Disposition: attachment; filename="example.zip"
        size_t fnPos = disposition.find(L"filename=");
        if (fnPos == std::wstring::npos) return L"";

        std::wstring fn = disposition.substr(fnPos + 9);
        if (fn.front() == L'"' || fn.front() == L'\'') {
            fn.erase(0, 1);
            size_t endQuote = fn.find_first_of(L"\"'");
            if (endQuote != std::wstring::npos) {
                fn = fn.substr(0, endQuote);
            }
        } else {
            size_t semi = fn.find(L';');
            if (semi != std::wstring::npos) {
                fn = fn.substr(0, semi);
            }
        }
        return CleanFilename(fn);
    }

    static std::wstring CleanFilename(const std::wstring& name) {
        std::wstring clean = name;
        const wchar_t invalidChars[] = L"\\/:*?\"<>|";
        for (wchar_t ch : invalidChars) {
            clean.erase(std::remove(clean.begin(), clean.end(), ch), clean.end());
        }
        while (!clean.empty() && (clean.back() == L' ' || clean.back() == L'.')) {
            clean.pop_back();
        }
        if (clean.empty()) clean = L"download.dat";
        return clean;
    }

    static std::wstring TrimUrl(const std::wstring& url) {
        if (url.empty()) return url;
        size_t first = url.find_first_not_of(L" \t\r\n");
        if (first == std::wstring::npos) return L"";
        size_t last = url.find_last_not_of(L" \t\r\n");
        return url.substr(first, (last - first + 1));
    }

    static std::wstring ExtractUrlFromText(const std::wstring& text) {
        if (text.empty()) return L"";

        // 1. Check for embedded URL with standard scheme
        const std::wstring schemes[] = { L"http://", L"https://", L"ftp://" };
        size_t bestPos = std::wstring::npos;
        for (const auto& scheme : schemes) {
            size_t pos = text.find(scheme);
            if (pos != std::wstring::npos && (bestPos == std::wstring::npos || pos < bestPos)) {
                bestPos = pos;
            }
        }

        if (bestPos != std::wstring::npos) {
            size_t endPos = text.find_first_of(L" \t\r\n\"'<>`", bestPos);
            std::wstring url = (endPos == std::wstring::npos) ? text.substr(bestPos) : text.substr(bestPos, endPos - bestPos);
            while (!url.empty() && (url.back() == L')' || url.back() == L']' || url.back() == L'.' || url.back() == L',' || url.back() == L';')) {
                url.pop_back();
            }
            return url;
        }

        // 2. Otherwise, trim the input text
        std::wstring trimmed = TrimUrl(text);
        if (trimmed.empty()) return L"";

        if (trimmed.rfind(L"www.", 0) == 0) {
            return L"https://" + trimmed;
        }

        // Check if single line without whitespace and looks like a domain / link (e.g. youtu.be/xxx or bit.ly/xxx)
        if (trimmed.find_first_of(L" \t\r\n") == std::wstring::npos) {
            size_t dotPos = trimmed.find(L'.');
            if (dotPos != std::wstring::npos && dotPos > 0 && dotPos < trimmed.length() - 1) {
                return L"https://" + trimmed;
            }
            if (trimmed.rfind(L"magnet:", 0) == 0) {
                return trimmed;
            }
        }

        return L"";
    }

    static std::wstring CombineUrl(const std::wstring& baseUrl, const std::wstring& relativeUrl) {
        if (relativeUrl.rfind(L"http://", 0) == 0 || relativeUrl.rfind(L"https://", 0) == 0) {
            return relativeUrl;
        }
        UrlParts baseParts;
        if (!ParseUrl(baseUrl, baseParts)) return relativeUrl;
        std::wstring scheme = baseParts.isHttps ? L"https://" : L"http://";
        std::wstring portStr = (baseParts.port != 80 && baseParts.port != 443) ? (L":" + std::to_wstring(baseParts.port)) : L"";
        if (relativeUrl.rfind(L"//", 0) == 0) {
            return (baseParts.isHttps ? L"https:" : L"http:") + relativeUrl;
        }
        if (!relativeUrl.empty() && relativeUrl.front() == L'/') {
            return scheme + baseParts.host + portStr + relativeUrl;
        }
        std::wstring basePath = baseParts.path;
        size_t slash = basePath.find_last_of(L'/');
        if (slash != std::wstring::npos) {
            basePath = basePath.substr(0, slash + 1);
        } else {
            basePath = L"/";
        }
        return scheme + baseParts.host + portStr + basePath + relativeUrl;
    }

    static StrId DetectCategoryFromFilename(const std::wstring& filename) {
        size_t dotPos = filename.find_last_of(L'.');
        if (dotPos == std::wstring::npos) return StrId::CatGeneral;
        std::wstring ext = filename.substr(dotPos + 1);
        for (auto& c : ext) c = towlower(c);

        // Video / Film
        if (ext == L"mp4" || ext == L"mkv" || ext == L"avi" || ext == L"mov" || ext == L"wmv" ||
            ext == L"flv" || ext == L"webm" || ext == L"m4v" || ext == L"mpg" || ext == L"mpeg" ||
            ext == L"3gp" || ext == L"ts") {
            return StrId::CatVideo;
        }
        // Music / Audio
        if (ext == L"mp3" || ext == L"wav" || ext == L"flac" || ext == L"aac" || ext == L"ogg" ||
            ext == L"wma" || ext == L"m4a" || ext == L"opus" || ext == L"mid" || ext == L"alac") {
            return StrId::CatMusic;
        }
        // Software / Programs
        if (ext == L"exe" || ext == L"msi" || ext == L"iso" || ext == L"img" || ext == L"dmg" ||
            ext == L"pkg" || ext == L"deb" || ext == L"rpm" || ext == L"apk" || ext == L"appx") {
            return StrId::CatSoftware;
        }
        // Documents
        if (ext == L"pdf" || ext == L"doc" || ext == L"docx" || ext == L"xls" || ext == L"xlsx" ||
            ext == L"ppt" || ext == L"pptx" || ext == L"txt" || ext == L"epub" || ext == L"mobi" ||
            ext == L"rtf" || ext == L"csv") {
            return StrId::CatDocuments;
        }
        // Archives
        if (ext == L"zip" || ext == L"rar" || ext == L"7z" || ext == L"tar" || ext == L"gz" ||
            ext == L"bz2" || ext == L"xz" || ext == L"tgz") {
            return StrId::CatArchives;
        }
        // AI / Machine Learning Models
        if (ext == L"gguf" || ext == L"safetensors" || ext == L"onnx" || ext == L"pt") {
            return StrId::CatAI;
        }

        return StrId::CatGeneral;
    }
};

} // namespace Gety
