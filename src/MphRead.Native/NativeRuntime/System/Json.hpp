#pragma once

// System.Text.Json's document model, as much of it as this program's two
// documents need: a save slot and the settings file. Numbers are held as the
// text they were written with, which is what keeps a round trip byte-for-byte.

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MphRead::NativeRuntime
{
    class JsonValue;
    using JsonPtr = std::shared_ptr<JsonValue>;

    class JsonValue final
    {
    public:
        enum class Kind : std::int32_t
        {
            Null,
            Boolean,
            Number,
            String,
            Array,
            Object
        };

        [[nodiscard]] static JsonPtr MakeObject();
        [[nodiscard]] static JsonPtr MakeArray();
        [[nodiscard]] static JsonPtr MakeString(std::string value);
        [[nodiscard]] static JsonPtr MakeNumber(std::string text);
        [[nodiscard]] static JsonPtr MakeBoolean(bool value);
        [[nodiscard]] static JsonPtr MakeNull();

        [[nodiscard]] Kind Type() const noexcept { return _kind; }
        [[nodiscard]] const std::string& Text() const noexcept { return _text; }
        [[nodiscard]] bool Boolean() const noexcept { return _boolean; }
        [[nodiscard]] std::vector<JsonPtr>& Items() noexcept { return _items; }
        [[nodiscard]] const std::vector<JsonPtr>& Items() const noexcept { return _items; }

        // JsonObject's members, in the order they were written.
        void Set(const std::string& name, JsonPtr value);
        [[nodiscard]] JsonPtr Get(const std::string& name) const;
        [[nodiscard]] const std::vector<std::pair<std::string, JsonPtr>>& Members() const noexcept
        {
            return _members;
        }

        [[nodiscard]] std::int64_t AsInt64(std::int64_t fallback = 0) const;

    private:
        Kind _kind = Kind::Null;
        std::string _text;
        bool _boolean = false;
        std::vector<JsonPtr> _items;
        std::vector<std::pair<std::string, JsonPtr>> _members;
    };

    // JsonSerializer.Deserialize: null when the text is not valid JSON.
    [[nodiscard]] JsonPtr JsonParse(const std::string& text);
    // JsonSerializer.Serialize with WriteIndented left off.
    [[nodiscard]] std::string JsonWrite(const JsonPtr& value);
    // The same with WriteIndented = true.
    [[nodiscard]] std::string JsonWriteIndented(const JsonPtr& value);
    // JsonNamingPolicy.CamelCase.ConvertName(name).
    [[nodiscard]] std::string JsonNamingPolicyCamelCase(std::string_view name);
}
