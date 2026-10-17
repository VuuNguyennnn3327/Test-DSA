#ifndef GORIDE_UTILS_JSON_HPP
#define GORIDE_UTILS_JSON_HPP

#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace goride {
namespace utils {

enum class JsonType {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

class JsonValue {
public:
    using ArrayType = std::vector<JsonValue>;
    using ObjectType = std::unordered_map<std::string, JsonValue>;

private:
    JsonType type_{JsonType::Null};
    std::variant<std::monostate, bool, double, std::string, ArrayType, ObjectType> value_;
    int64_t int_val_{0};
    bool is_int_{false};

public:
    JsonValue() : type_(JsonType::Null), value_(std::monostate()) {}
    JsonValue(std::nullptr_t) : type_(JsonType::Null), value_(std::monostate()) {}
    JsonValue(bool b) : type_(JsonType::Boolean), value_(b) {}
    JsonValue(int i) : type_(JsonType::Number), value_(static_cast<double>(i)), int_val_(i), is_int_(true) {}
    JsonValue(int64_t i) : type_(JsonType::Number), value_(static_cast<double>(i)), int_val_(i), is_int_(true) {}
    JsonValue(uint64_t u) : type_(JsonType::Number), value_(static_cast<double>(u)), int_val_(static_cast<int64_t>(u)), is_int_(true) {}
    JsonValue(double d) : type_(JsonType::Number), value_(d), int_val_(static_cast<int64_t>(d)), is_int_(false) {}
    JsonValue(std::string s) : type_(JsonType::String), value_(std::move(s)) {}
    JsonValue(const char* s) : type_(JsonType::String), value_(std::string(s)) {}
    JsonValue(ArrayType a) : type_(JsonType::Array), value_(std::move(a)) {}
    JsonValue(ObjectType o) : type_(JsonType::Object), value_(std::move(o)) {}

    JsonType type() const { return type_; }
    bool is_null() const { return type_ == JsonType::Null; }
    bool is_bool() const { return type_ == JsonType::Boolean; }
    bool is_number() const { return type_ == JsonType::Number; }
    bool is_string() const { return type_ == JsonType::String; }
    bool is_array() const { return type_ == JsonType::Array; }
    bool is_object() const { return type_ == JsonType::Object; }

    bool as_bool(bool default_val = false) const {
        if (is_bool()) return std::get<bool>(value_);
        return default_val;
    }

    double as_double(double default_val = 0.0) const {
        if (is_number()) return std::get<double>(value_);
        if (is_string()) {
            try {
                return std::stod(std::get<std::string>(value_));
            } catch (...) {}
        }
        return default_val;
    }

    int64_t as_int64(int64_t default_val = 0) const {
        if (is_number()) {
            if (is_int_) return int_val_;
            return static_cast<int64_t>(std::get<double>(value_));
        }
        if (is_string()) {
            try {
                return std::stoll(std::get<std::string>(value_));
            } catch (...) {}
        }
        return default_val;
    }

    const std::string& as_string(const std::string& default_val = "") const {
        if (is_string()) return std::get<std::string>(value_);
        static const std::string empty;
        return default_val.empty() ? empty : default_val;
    }

    const ArrayType& as_array() const {
        if (is_array()) return std::get<ArrayType>(value_);
        static const ArrayType empty;
        return empty;
    }

    const ObjectType& as_object() const {
        if (is_object()) return std::get<ObjectType>(value_);
        static const ObjectType empty;
        return empty;
    }

    bool has_key(const std::string& key) const {
        if (!is_object()) return false;
        const auto& obj = std::get<ObjectType>(value_);
        return obj.find(key) != obj.end();
    }

    const JsonValue& operator[](const std::string& key) const {
        static const JsonValue null_value;
        if (!is_object()) return null_value;
        const auto& obj = std::get<ObjectType>(value_);
        auto it = obj.find(key);
        if (it != obj.end()) return it->second;
        return null_value;
    }

    const JsonValue& operator[](size_t index) const {
        static const JsonValue null_value;
        if (!is_array()) return null_value;
        const auto& arr = std::get<ArrayType>(value_);
        if (index < arr.size()) return arr[index];
        return null_value;
    }

    static JsonValue parse(std::string_view src, std::string& error_out) {
        size_t pos = 0;
        skip_whitespace(src, pos);
        if (pos >= src.size()) {
            error_out = "Empty JSON input";
            return JsonValue();
        }
        JsonValue result = parse_value(src, pos, error_out);
        if (!error_out.empty()) return JsonValue();
        skip_whitespace(src, pos);
        if (pos < src.size()) {
            error_out = "Extra characters after JSON end at pos " + std::to_string(pos);
            return JsonValue();
        }
        return result;
    }

    static JsonValue parse_file(const std::string& filepath, std::string& error_out) {
        std::ifstream fin(filepath, std::ios::binary);
        if (!fin.is_open()) {
            error_out = "Cannot open file: " + filepath;
            return JsonValue();
        }
        std::ostringstream ss;
        ss << fin.rdbuf();
        return parse(ss.str(), error_out);
    }

private:
    static void skip_whitespace(std::string_view src, size_t& pos) {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' || src[pos] == '\r')) {
            ++pos;
        }
    }

    static JsonValue parse_value(std::string_view src, size_t& pos, std::string& error_out) {
        skip_whitespace(src, pos);
        if (pos >= src.size()) {
            error_out = "Unexpected end of input";
            return JsonValue();
        }

        char c = src[pos];
        if (c == '{') return parse_object(src, pos, error_out);
        if (c == '[') return parse_array(src, pos, error_out);
        if (c == '"') return parse_string(src, pos, error_out);
        if (c == 't' || c == 'f') return parse_bool(src, pos, error_out);
        if (c == 'n') return parse_null(src, pos, error_out);
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number(src, pos, error_out);

        error_out = std::string("Unexpected token '") + c + "' at position " + std::to_string(pos);
        return JsonValue();
    }

    static JsonValue parse_object(std::string_view src, size_t& pos, std::string& error_out) {
        ++pos; // skip '{'
        ObjectType obj;
        skip_whitespace(src, pos);
        if (pos < src.size() && src[pos] == '}') {
            ++pos;
            return JsonValue(std::move(obj));
        }

        while (pos < src.size()) {
            skip_whitespace(src, pos);
            if (pos >= src.size() || src[pos] != '"') {
                error_out = "Expected string key in object at pos " + std::to_string(pos);
                return JsonValue();
            }
            JsonValue key_val = parse_string(src, pos, error_out);
            if (!error_out.empty()) return JsonValue();
            std::string key = key_val.as_string();

            skip_whitespace(src, pos);
            if (pos >= src.size() || src[pos] != ':') {
                error_out = "Expected ':' after key at pos " + std::to_string(pos);
                return JsonValue();
            }
            ++pos; // skip ':'

            JsonValue val = parse_value(src, pos, error_out);
            if (!error_out.empty()) return JsonValue();
            obj.emplace(std::move(key), std::move(val));

            skip_whitespace(src, pos);
            if (pos < src.size() && src[pos] == ',') {
                ++pos;
                continue;
            } else if (pos < src.size() && src[pos] == '}') {
                ++pos;
                return JsonValue(std::move(obj));
            } else {
                error_out = "Expected ',' or '}' in object at pos " + std::to_string(pos);
                return JsonValue();
            }
        }
        error_out = "Unterminated object";
        return JsonValue();
    }

    static JsonValue parse_array(std::string_view src, size_t& pos, std::string& error_out) {
        ++pos; // skip '['
        ArrayType arr;
        skip_whitespace(src, pos);
        if (pos < src.size() && src[pos] == ']') {
            ++pos;
            return JsonValue(std::move(arr));
        }

        while (pos < src.size()) {
            JsonValue val = parse_value(src, pos, error_out);
            if (!error_out.empty()) return JsonValue();
            arr.push_back(std::move(val));

            skip_whitespace(src, pos);
            if (pos < src.size() && src[pos] == ',') {
                ++pos;
                continue;
            } else if (pos < src.size() && src[pos] == ']') {
                ++pos;
                return JsonValue(std::move(arr));
            } else {
                error_out = "Expected ',' or ']' in array at pos " + std::to_string(pos);
                return JsonValue();
            }
        }
        error_out = "Unterminated array";
        return JsonValue();
    }

    static JsonValue parse_string(std::string_view src, size_t& pos, std::string& error_out) {
        ++pos; // skip opening '"'
        std::string s;
        while (pos < src.size()) {
            char c = src[pos++];
            if (c == '"') {
                return JsonValue(std::move(s));
            }
            if (c == '\\') {
                if (pos >= src.size()) {
                    error_out = "Unterminated escape sequence";
                    return JsonValue();
                }
                char esc = src[pos++];
                switch (esc) {
                    case '"': s.push_back('"'); break;
                    case '\\': s.push_back('\\'); break;
                    case '/': s.push_back('/'); break;
                    case 'b': s.push_back('\b'); break;
                    case 'f': s.push_back('\f'); break;
                    case 'n': s.push_back('\n'); break;
                    case 'r': s.push_back('\r'); break;
                    case 't': s.push_back('\t'); break;
                    case 'u': {
                        if (pos + 4 > src.size()) {
                            error_out = "Incomplete unicode escape";
                            return JsonValue();
                        }
                        pos += 4;
                        s.push_back('?'); // Simplified unicode placeholder
                        break;
                    }
                    default:
                        s.push_back(esc);
                        break;
                }
            } else {
                s.push_back(c);
            }
        }
        error_out = "Unterminated string";
        return JsonValue();
    }

    static JsonValue parse_number(std::string_view src, size_t& pos, std::string& error_out) {
        size_t start = pos;
        bool has_dot_or_e = false;

        if (pos < src.size() && src[pos] == '-') ++pos;

        while (pos < src.size() && (src[pos] >= '0' && src[pos] <= '9')) ++pos;

        if (pos < src.size() && src[pos] == '.') {
            has_dot_or_e = true;
            ++pos;
            while (pos < src.size() && (src[pos] >= '0' && src[pos] <= '9')) ++pos;
        }

        if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
            has_dot_or_e = true;
            ++pos;
            if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) ++pos;
            while (pos < src.size() && (src[pos] >= '0' && src[pos] <= '9')) ++pos;
        }

        std::string num_str(src.substr(start, pos - start));
        try {
            if (has_dot_or_e) {
                double d = std::stod(num_str);
                return JsonValue(d);
            } else {
                int64_t i = std::stoll(num_str);
                return JsonValue(i);
            }
        } catch (...) {
            error_out = "Failed to parse number: " + num_str;
            return JsonValue();
        }
    }

    static JsonValue parse_bool(std::string_view src, size_t& pos, std::string& error_out) {
        if (src.substr(pos, 4) == "true") {
            pos += 4;
            return JsonValue(true);
        }
        if (src.substr(pos, 5) == "false") {
            pos += 5;
            return JsonValue(false);
        }
        error_out = "Invalid boolean at pos " + std::to_string(pos);
        return JsonValue();
    }

    static JsonValue parse_null(std::string_view src, size_t& pos, std::string& error_out) {
        if (src.substr(pos, 4) == "null") {
            pos += 4;
            return JsonValue(nullptr);
        }
        error_out = "Invalid null at pos " + std::to_string(pos);
        return JsonValue();
    }
};

} // namespace utils
} // namespace goride

#endif // GORIDE_UTILS_JSON_HPP
