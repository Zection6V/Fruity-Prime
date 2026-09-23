#include "Json.hpp"

#include <cstdlib>

namespace MphRead::NativeRuntime
{
    namespace
    {
        class Parser final
        {
        public:
            explicit Parser(const std::string& text) noexcept
                : _text(text)
            {
            }

            [[nodiscard]] JsonPtr Parse()
            {
                SkipWhite();
                JsonPtr value = ParseValue();
                SkipWhite();
                return _failed || _index != _text.size() ? nullptr : value;
            }

        private:
            void SkipWhite() noexcept
            {
                while (_index < _text.size()
                    && (_text[_index] == ' ' || _text[_index] == '\t'
                        || _text[_index] == '\r' || _text[_index] == '\n'))
                {
                    ++_index;
                }
            }

            [[nodiscard]] bool Match(char value) noexcept
            {
                if (_index < _text.size() && _text[_index] == value)
                {
                    ++_index;
                    return true;
                }
                return false;
            }

            [[nodiscard]] bool Literal(const char* word) noexcept
            {
                const std::size_t length = std::char_traits<char>::length(word);
                if (_text.compare(_index, length, word) != 0)
                {
                    return false;
                }
                _index += length;
                return true;
            }

            [[nodiscard]] JsonPtr ParseValue()
            {
                if (_failed || _index >= _text.size())
                {
                    _failed = true;
                    return nullptr;
                }
                const char first = _text[_index];
                if (first == '{')
                {
                    return ParseObject();
                }
                if (first == '[')
                {
                    return ParseArray();
                }
                if (first == '"')
                {
                    const std::optional<std::string> text = ParseString();
                    if (!text.has_value())
                    {
                        return nullptr;
                    }
                    return JsonValue::MakeString(*text);
                }
                if (Literal("true"))
                {
                    return JsonValue::MakeBoolean(true);
                }
                if (Literal("false"))
                {
                    return JsonValue::MakeBoolean(false);
                }
                if (Literal("null"))
                {
                    return JsonValue::MakeNull();
                }
                return ParseNumber();
            }

            [[nodiscard]] JsonPtr ParseObject()
            {
                if (!Match('{'))
                {
                    _failed = true;
                    return nullptr;
                }
                JsonPtr object = JsonValue::MakeObject();
                SkipWhite();
                if (Match('}'))
                {
                    return object;
                }
                while (true)
                {
                    SkipWhite();
                    const std::optional<std::string> name = ParseString();
                    if (!name.has_value())
                    {
                        _failed = true;
                        return nullptr;
                    }
                    SkipWhite();
                    if (!Match(':'))
                    {
                        _failed = true;
                        return nullptr;
                    }
                    SkipWhite();
                    JsonPtr value = ParseValue();
                    if (_failed)
                    {
                        return nullptr;
                    }
                    object->Set(*name, std::move(value));
                    SkipWhite();
                    if (Match(','))
                    {
                        continue;
                    }
                    if (Match('}'))
                    {
                        return object;
                    }
                    _failed = true;
                    return nullptr;
                }
            }

            [[nodiscard]] JsonPtr ParseArray()
            {
                if (!Match('['))
                {
                    _failed = true;
                    return nullptr;
                }
                JsonPtr array = JsonValue::MakeArray();
                SkipWhite();
                if (Match(']'))
                {
                    return array;
                }
                while (true)
                {
                    SkipWhite();
                    JsonPtr value = ParseValue();
                    if (_failed)
                    {
                        return nullptr;
                    }
                    array->Items().push_back(std::move(value));
                    SkipWhite();
                    if (Match(','))
                    {
                        continue;
                    }
                    if (Match(']'))
                    {
                        return array;
                    }
                    _failed = true;
                    return nullptr;
                }
            }

            [[nodiscard]] std::optional<std::string> ParseString()
            {
                if (!Match('"'))
                {
                    return std::nullopt;
                }
                std::string result;
                while (_index < _text.size())
                {
                    const char value = _text[_index++];
                    if (value == '"')
                    {
                        return result;
                    }
                    if (value != '\\')
                    {
                        result += value;
                        continue;
                    }
                    if (_index >= _text.size())
                    {
                        break;
                    }
                    const char escape = _text[_index++];
                    switch (escape)
                    {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u':
                    {
                        if (_index + 4 > _text.size())
                        {
                            return std::nullopt;
                        }
                        const std::string digits = _text.substr(_index, 4);
                        _index += 4;
                        const auto code = static_cast<char32_t>(
                            std::strtoul(digits.c_str(), nullptr, 16));
                        // The writer escapes only ASCII, so this is the only
                        // range that needs decoding back.
                        if (code < 0x80)
                        {
                            result += static_cast<char>(code);
                        }
                        else if (code < 0x800)
                        {
                            result += static_cast<char>(0xC0U | (code >> 6));
                            result += static_cast<char>(0x80U | (code & 0x3FU));
                        }
                        else
                        {
                            result += static_cast<char>(0xE0U | (code >> 12));
                            result += static_cast<char>(0x80U | ((code >> 6) & 0x3FU));
                            result += static_cast<char>(0x80U | (code & 0x3FU));
                        }
                        break;
                    }
                    default: return std::nullopt;
                    }
                }
                return std::nullopt;
            }

            [[nodiscard]] JsonPtr ParseNumber()
            {
                const std::size_t start = _index;
                if (_index < _text.size() && (_text[_index] == '-' || _text[_index] == '+'))
                {
                    ++_index;
                }
                while (_index < _text.size()
                    && ((_text[_index] >= '0' && _text[_index] <= '9')
                        || _text[_index] == '.' || _text[_index] == 'e'
                        || _text[_index] == 'E' || _text[_index] == '-'
                        || _text[_index] == '+'))
                {
                    ++_index;
                }
                if (_index == start)
                {
                    _failed = true;
                    return nullptr;
                }
                return JsonValue::MakeNumber(_text.substr(start, _index - start));
            }

            const std::string& _text;
            std::size_t _index = 0;
            bool _failed = false;
        };

        void WriteString(std::string& out, const std::string& value)
        {
            out += '"';
            for (const char item : value)
            {
                switch (item)
                {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(item) < 0x20)
                    {
                        static const char* const digits = "0123456789ABCDEF";
                        out += "\\u00";
                        out += digits[(static_cast<unsigned char>(item) >> 4) & 0xF];
                        out += digits[static_cast<unsigned char>(item) & 0xF];
                    }
                    else
                    {
                        out += item;
                    }
                    break;
                }
            }
            out += '"';
        }

        void Write(std::string& out, const JsonPtr& value)
        {
            if (value == nullptr)
            {
                out += "null";
                return;
            }
            switch (value->Type())
            {
            case JsonValue::Kind::Null: out += "null"; break;
            case JsonValue::Kind::Boolean: out += value->Boolean() ? "true" : "false"; break;
            case JsonValue::Kind::Number: out += value->Text(); break;
            case JsonValue::Kind::String: WriteString(out, value->Text()); break;
            case JsonValue::Kind::Array:
            {
                out += '[';
                bool first = true;
                for (const JsonPtr& item : value->Items())
                {
                    if (!first)
                    {
                        out += ',';
                    }
                    first = false;
                    Write(out, item);
                }
                out += ']';
                break;
            }
            case JsonValue::Kind::Object:
            {
                out += '{';
                bool first = true;
                for (const auto& [name, member] : value->Members())
                {
                    if (!first)
                    {
                        out += ',';
                    }
                    first = false;
                    WriteString(out, name);
                    out += ':';
                    Write(out, member);
                }
                out += '}';
                break;
            }
            }
        }
    }

    JsonPtr JsonValue::MakeObject()
    {
        auto value = std::make_shared<JsonValue>();
        value->_kind = Kind::Object;
        return value;
    }

    JsonPtr JsonValue::MakeArray()
    {
        auto value = std::make_shared<JsonValue>();
        value->_kind = Kind::Array;
        return value;
    }

    JsonPtr JsonValue::MakeString(std::string text)
    {
        auto value = std::make_shared<JsonValue>();
        value->_kind = Kind::String;
        value->_text = std::move(text);
        return value;
    }

    JsonPtr JsonValue::MakeNumber(std::string text)
    {
        auto value = std::make_shared<JsonValue>();
        value->_kind = Kind::Number;
        value->_text = std::move(text);
        return value;
    }

    JsonPtr JsonValue::MakeBoolean(bool boolean)
    {
        auto value = std::make_shared<JsonValue>();
        value->_kind = Kind::Boolean;
        value->_boolean = boolean;
        return value;
    }

    JsonPtr JsonValue::MakeNull()
    {
        return std::make_shared<JsonValue>();
    }

    void JsonValue::Set(const std::string& name, JsonPtr value)
    {
        for (auto& member : _members)
        {
            if (member.first == name)
            {
                member.second = std::move(value);
                return;
            }
        }
        _members.emplace_back(name, std::move(value));
    }

    JsonPtr JsonValue::Get(const std::string& name) const
    {
        for (const auto& member : _members)
        {
            if (member.first == name)
            {
                return member.second;
            }
        }
        return nullptr;
    }

    std::int64_t JsonValue::AsInt64(std::int64_t fallback) const
    {
        if (_kind != Kind::Number)
        {
            return fallback;
        }
        return std::strtoll(_text.c_str(), nullptr, 10);
    }

    JsonPtr JsonParse(const std::string& text)
    {
        Parser parser(text);
        return parser.Parse();
    }

    std::string JsonWrite(const JsonPtr& value)
    {
        std::string out;
        Write(out, value);
        return out;
    }
}
