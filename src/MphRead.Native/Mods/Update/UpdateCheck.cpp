/*
 * Native counterpart of MphRead/Mods/Update/UpdateCheck.cs.
 *
 * This unit owns the release JSON contract and the decision whether a
 * package is newer.  Downloading and process replacement stay in their own
 * source files, so a metadata check cannot accidentally write to an install.
 */
#include "Mods/branding.hpp"
#include "Mods/Update/update.hpp"

#include "update_http.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fruityprime::update {
namespace {

struct JsonValue {
    enum class Kind { Null, Boolean, Number, String, Array, Object };
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;

    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    Array array;
    Object object;
};

class JsonParser final {
public:
    explicit JsonParser(std::string_view input) : input_(input) {}

    [[nodiscard]] JsonValue parse() {
        skip_space();
        JsonValue value = parse_value();
        skip_space();
        if (position_ != input_.size()) {
            fail("unexpected data after JSON document");
        }
        return value;
    }

private:
    [[noreturn]] void fail(std::string_view message) const {
        throw std::runtime_error("release JSON error at byte "
                                 + std::to_string(position_) + ": "
                                 + std::string(message));
    }

    void skip_space() noexcept {
        while (position_ < input_.size()
               && std::isspace(static_cast<unsigned char>(input_[position_]))
                      != 0) {
            ++position_;
        }
    }

    [[nodiscard]] bool consume(char value) noexcept {
        if (position_ < input_.size() && input_[position_] == value) {
            ++position_;
            return true;
        }
        return false;
    }

    void expect(std::string_view word) {
        if (input_.compare(position_, word.size(), word) != 0) {
            fail("invalid literal");
        }
        position_ += word.size();
    }

    [[nodiscard]] JsonValue parse_value() {
        skip_space();
        if (position_ >= input_.size()) {
            fail("expected a value");
        }
        switch (input_[position_]) {
        case '{': return parse_object();
        case '[': return parse_array();
        case '"': {
            JsonValue value;
            value.kind = JsonValue::Kind::String;
            value.text = parse_string();
            return value;
        }
        case 't': {
            expect("true");
            JsonValue value;
            value.kind = JsonValue::Kind::Boolean;
            value.boolean = true;
            return value;
        }
        case 'f': {
            expect("false");
            JsonValue value;
            value.kind = JsonValue::Kind::Boolean;
            value.boolean = false;
            return value;
        }
        case 'n':
            expect("null");
            return {};
        default:
            return parse_number();
        }
    }

    [[nodiscard]] JsonValue parse_object() {
        JsonValue value;
        value.kind = JsonValue::Kind::Object;
        ++position_; // {
        skip_space();
        if (consume('}')) {
            return value;
        }
        for (;;) {
            skip_space();
            if (position_ >= input_.size() || input_[position_] != '"') {
                fail("object key must be a string");
            }
            std::string key = parse_string();
            skip_space();
            if (!consume(':')) {
                fail("expected ':' after object key");
            }
            value.object.insert_or_assign(std::move(key), parse_value());
            skip_space();
            if (consume('}')) {
                return value;
            }
            if (!consume(',')) {
                fail("expected ',' or '}' in object");
            }
        }
    }

    [[nodiscard]] JsonValue parse_array() {
        JsonValue value;
        value.kind = JsonValue::Kind::Array;
        ++position_; // [
        skip_space();
        if (consume(']')) {
            return value;
        }
        for (;;) {
            value.array.push_back(parse_value());
            skip_space();
            if (consume(']')) {
                return value;
            }
            if (!consume(',')) {
                fail("expected ',' or ']' in array");
            }
        }
    }

    static void append_utf8(std::string& output, unsigned value) {
        if (value <= 0x7f) {
            output.push_back(static_cast<char>(value));
        } else if (value <= 0x7ff) {
            output.push_back(static_cast<char>(0xc0 | (value >> 6)));
            output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        } else {
            output.push_back(static_cast<char>(0xe0 | (value >> 12)));
            output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        }
    }

    [[nodiscard]] unsigned hex_digit(char value) const {
        if (value >= '0' && value <= '9') {
            return static_cast<unsigned>(value - '0');
        }
        if (value >= 'a' && value <= 'f') {
            return static_cast<unsigned>(value - 'a' + 10);
        }
        if (value >= 'A' && value <= 'F') {
            return static_cast<unsigned>(value - 'A' + 10);
        }
        fail("invalid unicode escape");
    }

    [[nodiscard]] unsigned read_unicode_escape() {
        if (position_ + 4 > input_.size()) {
            fail("short unicode escape");
        }
        unsigned value = 0;
        for (int index = 0; index < 4; ++index) {
            value = (value << 4) | hex_digit(input_[position_++]);
        }
        return value;
    }

    [[nodiscard]] std::string parse_string() {
        if (!consume('"')) {
            fail("expected a string");
        }
        std::string result;
        while (position_ < input_.size()) {
            const char value = input_[position_++];
            if (value == '"') {
                return result;
            }
            if (value != '\\') {
                if (static_cast<unsigned char>(value) < 0x20) {
                    fail("control character in string");
                }
                result.push_back(value);
                continue;
            }
            if (position_ >= input_.size()) {
                fail("unterminated string escape");
            }
            const char escaped = input_[position_++];
            switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u': append_utf8(result, read_unicode_escape()); break;
            default: fail("unknown string escape");
            }
        }
        fail("unterminated string");
    }

    [[nodiscard]] JsonValue parse_number() {
        const char* begin = input_.data() + position_;
        char* end = nullptr;
        const double value = std::strtod(begin, &end);
        if (end == begin || !std::isfinite(value)) {
            fail("expected a finite number");
        }
        position_ = static_cast<std::size_t>(end - input_.data());
        JsonValue result;
        result.kind = JsonValue::Kind::Number;
        result.number = value;
        return result;
    }

    std::string_view input_;
    std::size_t position_ = 0;
};

std::string& reason() {
    static std::string value;
    return value;
}

[[nodiscard]] std::string lower(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        result.push_back(static_cast<char>(std::tolower(
            static_cast<unsigned char>(character))));
    }
    return result;
}

[[nodiscard]] const JsonValue* member(const JsonValue& object,
                                      std::string_view name) noexcept {
    if (object.kind != JsonValue::Kind::Object) {
        return nullptr;
    }
    const auto found = object.object.find(std::string(name));
    return found == object.object.end() ? nullptr : &found->second;
}

[[nodiscard]] std::string string_member(const JsonValue& object,
                                        std::string_view name) {
    const JsonValue* value = member(object, name);
    if (value == nullptr || value->kind == JsonValue::Kind::Null) {
        return {};
    }
    if (value->kind != JsonValue::Kind::String) {
        throw std::runtime_error("release field " + std::string(name)
                                 + " must be a string");
    }
    return value->text;
}

[[nodiscard]] std::uint64_t size_member(const JsonValue& object,
                                        std::string_view name) {
    const JsonValue* value = member(object, name);
    if (value == nullptr || value->kind == JsonValue::Kind::Null) {
        return 0;
    }
    if (value->kind != JsonValue::Kind::Number
        || value->number < 0.0
        || value->number
               > static_cast<double>(std::numeric_limits<std::uint64_t>::max())
        || std::floor(value->number) != value->number) {
        throw std::runtime_error("release field " + std::string(name)
                                 + " must be a non-negative integer");
    }
    return static_cast<std::uint64_t>(value->number);
}

struct Asset {
    std::string name;
    std::string url;
    std::uint64_t size = 0;
};

[[nodiscard]] const Asset* pick_asset(const std::vector<Asset>& assets,
                                      bool server_build) noexcept {
    const std::string wanted = UpdateCheck::rid();
    for (const Asset& asset : assets) {
        const std::string name = lower(asset.name);
        if (name.find(wanted) == std::string::npos) {
            continue;
        }
        const bool server_asset = name.find("-server-") != std::string::npos;
        if (server_asset != server_build) {
            continue;
        }
        return &asset;
    }
    return nullptr;
}

} // namespace

const std::string& UpdateCheck::last_reason() noexcept {
    return reason();
}

std::optional<UpdateInfo> UpdateCheck::latest(bool server_build) {
    reason().clear();
    const auto installed = BuildVersion::current();
    if (!installed) {
        reason() = "this is a local build, so it is left alone";
        return std::nullopt;
    }
    const std::string endpoint = "https://api.github.com/repos/"
        + std::string(branding::Repository) + "/releases/latest";
    const auto response = http::get(endpoint, 20'000,
                                    std::string(branding::FileName) + "/"
                                        + BuildVersion::display(installed));
    if (!response.error.empty()) {
        reason() = "could not reach GitHub (" + response.error + ")";
        return std::nullopt;
    }
    if (response.status_code == 404) {
        reason() = "no releases have been published yet";
        return std::nullopt;
    }
    if (response.status_code == 403 || response.status_code == 429) {
        reason() = "GitHub is rate-limiting this address; try later";
        return std::nullopt;
    }
    if (!response.success()) {
        reason() = "GitHub answered " + std::to_string(response.status_code);
        return std::nullopt;
    }
    return parse(response.body, installed, server_build);
}

std::optional<UpdateInfo> UpdateCheck::parse(std::string_view json,
                                             std::optional<Version> installed,
                                             bool server_build) {
    reason().clear();
    if (!installed) {
        installed = BuildVersion::current();
    }
    if (!installed) {
        reason() = "this is a local build, so it is left alone";
        return std::nullopt;
    }

    try {
        const JsonValue root = JsonParser(json).parse();
        if (root.kind != JsonValue::Kind::Object) {
            throw std::runtime_error("release JSON root must be an object");
        }
        const std::string tag = string_member(root, "tag_name");
        const std::string notes = string_member(root, "body");
        const std::string page = string_member(root, "html_url");
        std::vector<Asset> assets;
        if (const JsonValue* list = member(root, "assets"); list != nullptr) {
            if (list->kind != JsonValue::Kind::Array) {
                throw std::runtime_error("release field assets must be an array");
            }
            assets.reserve(list->array.size());
            for (const JsonValue& item : list->array) {
                if (item.kind != JsonValue::Kind::Object) {
                    throw std::runtime_error("release asset must be an object");
                }
                Asset asset;
                asset.name = string_member(item, "name");
                asset.url = string_member(item, "browser_download_url");
                asset.size = size_member(item, "size");
                if (!asset.name.empty()) {
                    assets.push_back(std::move(asset));
                }
            }
        }

        const auto published = BuildVersion::parse(tag);
        if (!published) {
            reason() = "the latest release (" + tag
                + ") is not a plain version tag";
            return std::nullopt;
        }
        const Version current = BuildVersion::normalize(*installed);
        if (!(current < *published)) {
            reason() = BuildVersion::display(current)
                + " is already the latest";
            return std::nullopt;
        }
        const Asset* package = pick_asset(assets, server_build);
        UpdateInfo result;
        result.tag = tag;
        result.version = *published;
        result.page_url = page.empty() ? releases_page() : page;
        result.notes = notes;
        if (package != nullptr) {
            result.asset_name = package->name;
            result.asset_url = package->url;
            result.asset_size = package->size;
        }
        return result;
    } catch (const std::exception&) {
        reason() = "GitHub's answer could not be read";
        return std::nullopt;
    }
}

std::string UpdateCheck::rid() {
#if defined(__ANDROID__)
    return "android";
#elif defined(_WIN32)
    const char* os = "win";
#elif defined(__APPLE__)
    const char* os = "osx";
#else
    const char* os = "linux";
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
    const char* arch = "arm64";
#elif defined(__arm__) || defined(_M_ARM)
    const char* arch = "arm";
#elif defined(__i386__) || defined(_M_IX86)
    const char* arch = "x86";
#else
    const char* arch = "x64";
#endif
#if defined(__ANDROID__)
    return "android";
#else
    return std::string(os) + "-" + arch;
#endif
}

std::string UpdateCheck::releases_page() {
    return "https://github.com/" + std::string(branding::Repository)
        + "/releases";
}

bool UpdateCheck::is_allowed_url(std::string_view url) noexcept {
    const std::string value = lower(url);
    if (value.rfind("https://", 0) != 0) {
        return false;
    }
    const std::size_t begin = 8;
    const std::size_t end = value.find_first_of("/?#", begin);
    const std::string authority = value.substr(
        begin, end == std::string::npos ? std::string::npos : end - begin);
    if (authority.empty() || authority.find('@') != std::string::npos) {
        return false;
    }
    const std::size_t colon = authority.find(':');
    const std::string host = authority.substr(0, colon);
    if (host.empty()) {
        return false;
    }
    return host == "github.com"
        || (host.size() > 11
            && host.ends_with(".github.com"))
        || host == "objects.githubusercontent.com"
        || (host.size() > 22
            && host.ends_with(".githubusercontent.com"));
}

} // namespace fruityprime::update
