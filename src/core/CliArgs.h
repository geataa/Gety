#pragma once

#include <string>
#include <vector>
#include "../engine/WinHttpUtils.h"

namespace Gety {

struct CliLaunchOptions {
    bool startMinimized = false;
    std::vector<std::wstring> urls;
};

/**
 * Splits a Windows command-line argument string (excluding executable name)
 * into unquoted tokens while preserving quoted arguments.
 */
inline std::vector<std::wstring> TokenizeCommandLineArgs(const std::wstring& cmdLineArgs) {
    std::vector<std::wstring> tokens;
    std::wstring current;
    bool inQuotes = false;
    bool tokenStarted = false;

    for (wchar_t ch : cmdLineArgs) {
        if (ch == L'"') {
            inQuotes = !inQuotes;
            tokenStarted = true;
        } else if (!inQuotes && (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n')) {
            if (tokenStarted) {
                tokens.push_back(current);
                current.clear();
                tokenStarted = false;
            }
        } else {
            current.push_back(ch);
            tokenStarted = true;
        }
    }
    if (tokenStarted) {
        tokens.push_back(current);
    }
    return tokens;
}

/**
 * Parses Gety CLI flags and separates them strictly from downloadable URLs
 * so flags such as `--minimized` or `--autostart` are never queued as tasks.
 */
inline CliLaunchOptions ParseCliArgs(const std::wstring& cmdLineArgs) {
    CliLaunchOptions opts;
    auto tokens = TokenizeCommandLineArgs(cmdLineArgs);

    for (const auto& rawTok : tokens) {
        if (rawTok.empty()) continue;

        std::wstring lower = rawTok;
        for (auto& c : lower) {
            if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
        }

        if (lower == L"--minimized" || lower == L"--autostart" || lower == L"--tray" || lower == L"-minimized") {
            opts.startMinimized = true;
            continue;
        }

        // Ignore any other CLI switch / flag (e.g. --snapshot, /minimized, -h)
        if (lower.front() == L'-' || lower.front() == L'/') {
            continue;
        }

        if (lower.rfind(L"http://", 0) == 0 ||
            lower.rfind(L"https://", 0) == 0 ||
            lower.rfind(L"ftp://", 0) == 0 ||
            lower.rfind(L"magnet:", 0) == 0) {
            opts.urls.push_back(WinHttpUtils::TrimUrl(rawTok));
            continue;
        }

        std::wstring extractedUrl = WinHttpUtils::ExtractUrlFromText(rawTok);
        if (!extractedUrl.empty()) {
            opts.urls.push_back(extractedUrl);
        }
    }

    return opts;
}

} // namespace Gety
