#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <cstdint>
#include <algorithm>

namespace Gety {

enum class JType { Null, Bool, Number, String, Array, Object };

struct JValue {
    JType type = JType::Null;
    bool bVal = false;
    double nVal = 0.0;
    std::string sVal;
    std::vector<JValue> arr;
    std::unordered_map<std::string, JValue> obj;

    std::string getString(const std::string& k, const std::string& def = "") const {
        if (type != JType::Object) return def;
        auto it = obj.find(k);
        if (it != obj.end() && it->second.type == JType::String) return it->second.sVal;
        return def;
    }

    int64_t getInt(const std::string& k, int64_t def = 0) const {
        if (type != JType::Object) return def;
        auto it = obj.find(k);
        if (it != obj.end() && it->second.type == JType::Number) return static_cast<int64_t>(it->second.nVal);
        return def;
    }

    double getDouble(const std::string& k, double def = 0.0) const {
        if (type != JType::Object) return def;
        auto it = obj.find(k);
        if (it != obj.end() && it->second.type == JType::Number) return it->second.nVal;
        return def;
    }

    bool getBool(const std::string& k, bool def = false) const {
        if (type != JType::Object) return def;
        auto it = obj.find(k);
        if (it != obj.end() && it->second.type == JType::Bool) return it->second.bVal;
        return def;
    }

    bool has(const std::string& k) const {
        if (type != JType::Object) return false;
        return obj.find(k) != obj.end();
    }

    const JValue& getObject(const std::string& k) const {
        static const JValue emptyObj{ JType::Object };
        if (type != JType::Object) return emptyObj;
        auto it = obj.find(k);
        if (it != obj.end() && it->second.type == JType::Object) return it->second;
        return emptyObj;
    }

    const std::vector<JValue>& getArray(const std::string& k) const {
        static const std::vector<JValue> emptyArr;
        if (type != JType::Object) return emptyArr;
        auto it = obj.find(k);
        if (it != obj.end() && it->second.type == JType::Array) return it->second.arr;
        return emptyArr;
    }
};

class JsonParser {
public:
    explicit JsonParser(const std::string& json) : m_src(json), m_pos(0) {}

    JValue Parse() {
        SkipWhitespace();
        return ParseValue();
    }

private:
    void SkipWhitespace() {
        while (m_pos < m_src.length()) {
            char c = m_src[m_pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                m_pos++;
            } else {
                break;
            }
        }
    }

    JValue ParseValue() {
        SkipWhitespace();
        if (m_pos >= m_src.length()) return JValue();

        char c = m_src[m_pos];
        if (c == '{') return ParseObject();
        if (c == '[') return ParseArray();
        if (c == '"') return ParseString();
        if (c == 't' || c == 'f') return ParseBool();
        if (c == 'n') return ParseNull();
        if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber();

        return JValue();
    }

    JValue ParseObject() {
        JValue val;
        val.type = JType::Object;
        m_pos++; // skip '{'

        while (m_pos < m_src.length()) {
            SkipWhitespace();
            if (m_pos >= m_src.length()) break;
            if (m_src[m_pos] == '}') {
                m_pos++;
                break;
            }

            if (m_src[m_pos] == ',') {
                m_pos++;
                continue;
            }

            if (m_src[m_pos] != '"') break;
            JValue kVal = ParseString();
            std::string key = kVal.sVal;

            SkipWhitespace();
            if (m_pos < m_src.length() && m_src[m_pos] == ':') {
                m_pos++;
            }

            val.obj[key] = ParseValue();
        }
        return val;
    }

    JValue ParseArray() {
        JValue val;
        val.type = JType::Array;
        m_pos++; // skip '['

        while (m_pos < m_src.length()) {
            SkipWhitespace();
            if (m_pos >= m_src.length()) break;
            if (m_src[m_pos] == ']') {
                m_pos++;
                break;
            }
            if (m_src[m_pos] == ',') {
                m_pos++;
                continue;
            }
            val.arr.push_back(ParseValue());
        }
        return val;
    }

    JValue ParseString() {
        JValue val;
        val.type = JType::String;
        m_pos++; // skip '"'
        std::string s;

        while (m_pos < m_src.length()) {
            char c = m_src[m_pos++];
            if (c == '"') {
                break;
            }
            if (c == '\\' && m_pos < m_src.length()) {
                char esc = m_src[m_pos++];
                switch (esc) {
                    case '"': s += '"'; break;
                    case '\\': s += '\\'; break;
                    case '/': s += '/'; break;
                    case 'b': s += '\b'; break;
                    case 'f': s += '\f'; break;
                    case 'n': s += '\n'; break;
                    case 'r': s += '\r'; break;
                    case 't': s += '\t'; break;
                    case 'u': {
                        if (m_pos + 4 <= m_src.length()) {
                            std::string hexStr = m_src.substr(m_pos, 4);
                            m_pos += 4;
                            unsigned int codepoint = 0;
                            std::stringstream ss;
                            ss << std::hex << hexStr;
                            ss >> codepoint;
                            // Convert codepoint to UTF-8
                            if (codepoint <= 0x7F) {
                                s += static_cast<char>(codepoint);
                            } else if (codepoint <= 0x7FF) {
                                s += static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F));
                                s += static_cast<char>(0x80 | (codepoint & 0x3F));
                            } else {
                                s += static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F));
                                s += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                                s += static_cast<char>(0x80 | (codepoint & 0x3F));
                            }
                        }
                        break;
                    }
                    default: s += esc; break;
                }
            } else {
                s += c;
            }
        }
        val.sVal = s;
        return val;
    }

    JValue ParseNumber() {
        JValue val;
        val.type = JType::Number;
        size_t start = m_pos;
        if (m_pos < m_src.length() && m_src[m_pos] == '-') m_pos++;
        while (m_pos < m_src.length()) {
            char c = m_src[m_pos];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
                m_pos++;
            } else {
                break;
            }
        }
        std::string numStr = m_src.substr(start, m_pos - start);
        try {
            val.nVal = std::stod(numStr);
        } catch (...) {
            val.nVal = 0.0;
        }
        return val;
    }

    JValue ParseBool() {
        JValue val;
        val.type = JType::Bool;
        if (m_src.compare(m_pos, 4, "true") == 0) {
            val.bVal = true;
            m_pos += 4;
        } else if (m_src.compare(m_pos, 5, "false") == 0) {
            val.bVal = false;
            m_pos += 5;
        }
        return val;
    }

    JValue ParseNull() {
        JValue val;
        val.type = JType::Null;
        if (m_src.compare(m_pos, 4, "null") == 0) {
            m_pos += 4;
        }
        return val;
    }

    const std::string& m_src;
    size_t m_pos;

public:
    static std::string EscapeString(const std::string& str) {
        std::string out;
        out.reserve(str.size() + 16);
        for (char c : str) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                        out += buf;
                    } else {
                        out += c;
                    }
                    break;
            }
        }
        return out;
    }

    static std::string WideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        if (len <= 0) return "";
        std::string str(len - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], len, NULL, NULL);
        return str;
    }

    static std::wstring Utf8ToWide(const std::string& str) {
        if (str.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
        if (len <= 0) return L"";
        std::wstring wstr(len - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
        return wstr;
    }
};

} // namespace Gety
