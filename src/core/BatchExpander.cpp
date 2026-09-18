#include "BatchExpander.h"
#include <sstream>
#include <iomanip>

namespace Gety {

std::vector<std::wstring> BatchExpander::ExpandNumeric(const std::wstring& pattern, int from, int to, int digits) {
    std::vector<std::wstring> result;
    size_t wildcardPos = pattern.find(L"(*)");
    if (wildcardPos == std::wstring::npos) {
        // Fallback: look for (*) or (*) format
        result.push_back(pattern);
        return result;
    }

    int step = (from <= to) ? 1 : -1;
    for (int i = from; (step > 0) ? (i <= to) : (i >= to); i += step) {
        std::wostringstream ss;
        if (digits > 1) {
            ss << std::setw(digits) << std::setfill(L'0') << i;
        } else {
            ss << i;
        }

        std::wstring url = pattern.substr(0, wildcardPos) + ss.str() + pattern.substr(wildcardPos + 3);
        result.push_back(url);
    }

    return result;
}

std::vector<std::wstring> BatchExpander::ExpandAlpha(const std::wstring& pattern, wchar_t fromChar, wchar_t toChar) {
    std::vector<std::wstring> result;
    size_t wildcardPos = pattern.find(L"(*)");
    if (wildcardPos == std::wstring::npos) {
        result.push_back(pattern);
        return result;
    }

    int step = (fromChar <= toChar) ? 1 : -1;
    for (wchar_t c = fromChar; (step > 0) ? (c <= toChar) : (c >= toChar); c += step) {
        std::wstring replacement(1, c);
        std::wstring url = pattern.substr(0, wildcardPos) + replacement + pattern.substr(wildcardPos + 3);
        result.push_back(url);
    }

    return result;
}

} // namespace Gety
