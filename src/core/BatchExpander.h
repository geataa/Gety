#pragma once

#include <string>
#include <vector>

namespace Gety {

class BatchExpander {
public:
    // Expands numeric range e.g. base="http://example.com/file(*).zip", from=1, to=10, digits=2 -> file01.zip .. file10.zip
    static std::vector<std::wstring> ExpandNumeric(const std::wstring& pattern, int from, int to, int digits);

    // Expands alphabetic range e.g. base="http://example.com/part_(*).rar", fromChar='a', toChar='z'
    static std::vector<std::wstring> ExpandAlpha(const std::wstring& pattern, wchar_t fromChar, wchar_t toChar);
};

} // namespace Gety
