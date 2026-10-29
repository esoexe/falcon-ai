#pragma once
/**
 * OpenMesh Studio — Self-Contained Modern C++20 JSON Parser & Serializer.
 * Zero external header dependencies; supports Objects, Arrays, Strings, Numbers, Booleans, Null.
 */

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace openmesh {

struct JsonValue;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::vector<std::pair<std::string, JsonValue>>; // preserves insertion order!

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool b_val = false;
    double n_val = 0.0;
    std::string s_val;
    JsonArray a_val;
    JsonObject o_val;

    JsonValue() : type(Type::Null) {}
    JsonValue(std::nullptr_t) : type(Type::Null) {}
    JsonValue(bool b) : type(Type::Bool), b_val(b) {}
    JsonValue(int v) : type(Type::Number), n_val(static_cast<double>(v)) {}
    JsonValue(unsigned int v) : type(Type::Number), n_val(static_cast<double>(v)) {}
    JsonValue(int64_t v) : type(Type::Number), n_val(static_cast<double>(v)) {}
    JsonValue(size_t v) : type(Type::Number), n_val(static_cast<double>(v)) {}
    JsonValue(float v) : type(Type::Number), n_val(static_cast<double>(v)) {}
    JsonValue(double v) : type(Type::Number), n_val(v) {}
    JsonValue(const char* s) : type(Type::String), s_val(s ? s : "") {}
    JsonValue(std::string s) : type(Type::String), s_val(std::move(s)) {}
    JsonValue(JsonArray a) : type(Type::Array), a_val(std::move(a)) {}
    JsonValue(JsonObject o) : type(Type::Object), o_val(std::move(o)) {}

    static JsonValue array(JsonArray a = {}) { return JsonValue(std::move(a)); }
    static JsonValue object(JsonObject o = {}) { return JsonValue(std::move(o)); }

    bool is_null() const { return type == Type::Null; }
    bool is_bool() const { return type == Type::Bool; }
    bool is_number() const { return type == Type::Number; }
    bool is_string() const { return type == Type::String; }
    bool is_array() const { return type == Type::Array; }
    bool is_object() const { return type == Type::Object; }

    void push_back(JsonValue v) {
        if (type != Type::Array) {
            type = Type::Array;
            a_val.clear();
        }
        a_val.push_back(std::move(v));
    }

    JsonValue& operator[](const std::string& key) {
        if (type != Type::Object) {
            type = Type::Object;
            o_val.clear();
        }
        for (auto& kv : o_val) {
            if (kv.first == key) return kv.second;
        }
        o_val.emplace_back(key, JsonValue());
        return o_val.back().second;
    }

    const JsonValue& operator[](const std::string& key) const {
        return at_or_null(key);
    }

    const JsonValue& operator[](int idx) const {
        return (*this)[static_cast<size_t>(idx)];
    }

    JsonValue& operator[](int idx) {
        return (*this)[static_cast<size_t>(idx)];
    }

    const JsonValue& at_or_null(const std::string& key) const {
        static const JsonValue kNull;
        if (type != Type::Object) return kNull;
        for (const auto& kv : o_val) {
            if (kv.first == key) return kv.second;
        }
        return kNull;
    }

    const JsonValue& operator[](size_t idx) const {
        static const JsonValue kNull;
        if (type != Type::Array || idx >= a_val.size()) return kNull;
        return a_val[idx];
    }

    JsonValue& operator[](size_t idx) {
        return a_val[idx];
    }

    size_t size() const {
        if (type == Type::Array) return a_val.size();
        if (type == Type::Object) return o_val.size();
        return 0;
    }

    bool has(const std::string& key) const {
        if (type != Type::Object) return false;
        for (const auto& kv : o_val) {
            if (kv.first == key && !kv.second.is_null()) return true;
        }
        return false;
    }

    std::string get_str(const std::string& key, const std::string& def = "") const {
        const auto& v = at_or_null(key);
        return v.is_string() ? v.s_val : def;
    }

    double get_num(const std::string& key, double def = 0.0) const {
        const auto& v = at_or_null(key);
        if (v.is_number()) return v.n_val;
        if (v.is_string()) {
            try { return std::stod(v.s_val); } catch (...) {}
        }
        return def;
    }

    int get_int(const std::string& key, int def = 0) const {
        return static_cast<int>(std::llround(get_num(key, static_cast<double>(def))));
    }

    bool get_bool(const std::string& key, bool def = false) const {
        const auto& v = at_or_null(key);
        if (v.is_bool()) return v.b_val;
        if (v.is_number()) return v.n_val != 0.0;
        if (v.is_string()) return (v.s_val == "true" || v.s_val == "1");
        return def;
    }

    double as_num(double def = 0.0) const { return is_number() ? n_val : def; }
    std::string as_str(const std::string& def = "") const { return is_string() ? s_val : def; }

    static std::string escape_str(const std::string& s) {
        std::ostringstream oss;
        oss << '"';
        for (unsigned char c : s) {
            switch (c) {
                case '"': oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\b': oss << "\\b"; break;
                case '\f': oss << "\\f"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                default:
                    if (c < 0x20) {
                        oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
                    } else {
                        oss << static_cast<char>(c);
                    }
            }
        }
        oss << '"';
        return oss.str();
    }

    void dump_to(std::ostringstream& oss, int indent, int depth) const {
        const std::string pad = (indent > 0) ? std::string(static_cast<size_t>(indent * depth), ' ') : "";
        const std::string pad_in = (indent > 0) ? std::string(static_cast<size_t>(indent * (depth + 1)), ' ') : "";
        const std::string nl = (indent > 0) ? "\n" : "";
        const std::string sep = (indent > 0) ? ": " : ":";

        switch (type) {
            case Type::Null:
                oss << "null";
                break;
            case Type::Bool:
                oss << (b_val ? "true" : "false");
                break;
            case Type::Number: {
                if (!std::isfinite(n_val)) {
                    oss << "0";
                } else if (std::floor(n_val) == n_val && std::fabs(n_val) < 1e15) {
                    oss << static_cast<int64_t>(n_val);
                } else {
                    std::ostringstream num_ss;
                    num_ss << std::setprecision(7) << n_val;
                    oss << num_ss.str();
                }
                break;
            }
            case Type::String:
                oss << escape_str(s_val);
                break;
            case Type::Array: {
                if (a_val.empty()) { oss << "[]"; break; }
                // Compact numeric arrays onto single line
                bool all_prim = a_val.size() <= 6;
                for (const auto& el : a_val) {
                    if (el.is_array() || el.is_object()) { all_prim = false; break; }
                }
                if (all_prim) {
                    oss << "[";
                    for (size_t i = 0; i < a_val.size(); ++i) {
                        if (i > 0) oss << (indent > 0 ? ", " : ",");
                        a_val[i].dump_to(oss, 0, 0);
                    }
                    oss << "]";
                } else {
                    oss << "[" << nl;
                    for (size_t i = 0; i < a_val.size(); ++i) {
                        oss << pad_in;
                        a_val[i].dump_to(oss, indent, depth + 1);
                        if (i + 1 < a_val.size()) oss << ",";
                        oss << nl;
                    }
                    oss << pad << "]";
                }
                break;
            }
            case Type::Object: {
                if (o_val.empty()) { oss << "{}"; break; }
                oss << "{" << nl;
                for (size_t i = 0; i < o_val.size(); ++i) {
                    oss << pad_in << escape_str(o_val[i].first) << sep;
                    o_val[i].second.dump_to(oss, indent, depth + 1);
                    if (i + 1 < o_val.size()) oss << ",";
                    oss << nl;
                }
                oss << pad << "}";
                break;
            }
        }
    }

    std::string dump(int indent = 0) const {
        std::ostringstream oss;
        dump_to(oss, indent, 0);
        return oss.str();
    }

    // Recursive-descent JSON Parser
    struct Parser {
        std::string_view src;
        size_t pos = 0;

        void skip_ws() {
            while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r' || src[pos] == '\n')) {
                ++pos;
            }
        }

        char peek() {
            skip_ws();
            if (pos >= src.size()) throw std::runtime_error("Unexpected end of JSON");
            return src[pos];
        }

        std::string parse_string() {
            skip_ws();
            if (pos >= src.size() || src[pos] != '"') throw std::runtime_error("Expected '\"'");
            ++pos;
            std::string out;
            while (pos < src.size()) {
                char c = src[pos++];
                if (c == '"') return out;
                if (c == '\\') {
                    if (pos >= src.size()) throw std::runtime_error("Untruncated escape");
                    char esc = src[pos++];
                    switch (esc) {
                        case '"': out.push_back('"'); break;
                        case '\\': out.push_back('\\'); break;
                        case '/': out.push_back('/'); break;
                        case 'b': out.push_back('\b'); break;
                        case 'f': out.push_back('\f'); break;
                        case 'n': out.push_back('\n'); break;
                        case 'r': out.push_back('\r'); break;
                        case 't': out.push_back('\t'); break;
                        case 'u': {
                            if (pos + 4 > src.size()) throw std::runtime_error("Invalid \\uXXXX");
                            unsigned int cp = std::stoul(std::string(src.substr(pos, 4)), nullptr, 16);
                            pos += 4;
                            if (cp < 0x80) {
                                out.push_back(static_cast<char>(cp));
                            } else if (cp < 0x800) {
                                out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                            } else {
                                out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                            }
                            break;
                        }
                        default: out.push_back(esc); break;
                    }
                } else {
                    out.push_back(c);
                }
            }
            throw std::runtime_error("Unterminated string");
        }

        JsonValue parse_value() {
            char c = peek();
            if (c == '"') return JsonValue(parse_string());
            if (c == '{') {
                ++pos;
                JsonObject obj;
                if (peek() == '}') { ++pos; return JsonValue(std::move(obj)); }
                while (true) {
                    std::string k = parse_string();
                    if (peek() != ':') throw std::runtime_error("Expected ':'");
                    ++pos;
                    JsonValue v = parse_value();
                    bool updated = false;
                    for (auto& kv : obj) {
                        if (kv.first == k) { kv.second = std::move(v); updated = true; break; }
                    }
                    if (!updated) obj.emplace_back(std::move(k), std::move(v));
                    char sep = peek();
                    if (sep == '}') { ++pos; break; }
                    if (sep != ',') throw std::runtime_error("Expected ',' or '}'");
                    ++pos;
                }
                return JsonValue(std::move(obj));
            }
            if (c == '[') {
                ++pos;
                JsonArray arr;
                if (peek() == ']') { ++pos; return JsonValue(std::move(arr)); }
                while (true) {
                    arr.push_back(parse_value());
                    char sep = peek();
                    if (sep == ']') { ++pos; break; }
                    if (sep != ',') throw std::runtime_error("Expected ',' or ']'");
                    ++pos;
                }
                return JsonValue(std::move(arr));
            }
            if (src.substr(pos, 4) == "true") { pos += 4; return JsonValue(true); }
            if (src.substr(pos, 5) == "false") { pos += 5; return JsonValue(false); }
            if (src.substr(pos, 4) == "null") { pos += 4; return JsonValue(nullptr); }

            // Parse number
            size_t start = pos;
            if (src[pos] == '-') ++pos;
            while (pos < src.size() && ((src[pos] >= '0' && src[pos] <= '9') || src[pos] == '.' || src[pos] == 'e' || src[pos] == 'E' || src[pos] == '+' || src[pos] == '-')) {
                ++pos;
            }
            if (pos == start) throw std::runtime_error("Invalid token in JSON");
            double d = std::stod(std::string(src.substr(start, pos - start)));
            return JsonValue(d);
        }
    };

    static JsonValue parse(std::string_view text) {
        Parser p{text, 0};
        return p.parse_value();
    }
};

} // namespace openmesh
