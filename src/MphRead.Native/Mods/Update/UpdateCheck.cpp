#include "UpdateCheck.hpp"

#include "../Branding.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MphRead::Mods::Update
{
    namespace
    {
        constexpr std::chrono::seconds Timeout(20);

        [[nodiscard]] std::string ApiUrl()
        {
            return "https://api.github.com/repos/"
                + std::string(Mods::Branding::Repository)
                + "/releases/latest";
        }

        std::mutex LastReasonMutex;
        std::optional<std::string> LastReasonValue;

        class NamedException : public std::runtime_error
        {
        public:
            NamedException(std::string name, std::string message)
                : std::runtime_error(std::move(message)), _name(std::move(name))
            {
            }

            [[nodiscard]] const std::string& Name() const noexcept
            {
                return _name;
            }

        private:
            std::string _name;
        };

        [[noreturn]] void ThrowJsonException(std::string_view message)
        {
            throw NamedException("JsonException", std::string(message));
        }

        [[noreturn]] void ThrowInvalidOperation(std::string_view message)
        {
            throw NamedException("InvalidOperationException", std::string(message));
        }

        enum class JsonKind
        {
            Null,
            Boolean,
            Number,
            String,
            Array,
            Object
        };

        struct JsonValue
        {
            JsonKind Kind = JsonKind::Null;
            bool Boolean = false;
            std::string Text;
            std::vector<JsonValue> Array;
            std::vector<std::pair<std::string, JsonValue>> Object;
        };

        [[nodiscard]] bool IsJsonWhitespace(char value) noexcept
        {
            return value == ' ' || value == '\t' || value == '\r' || value == '\n';
        }

        [[nodiscard]] bool IsUtf8Continuation(unsigned char value) noexcept
        {
            return (value & 0xC0U) == 0x80U;
        }

        [[nodiscard]] std::size_t Utf8SequenceLength(std::string_view input,
            std::size_t offset)
        {
            const auto first = static_cast<unsigned char>(input[offset]);
            if (first <= 0x7FU)
            {
                return 1;
            }
            if (first >= 0xC2U && first <= 0xDFU)
            {
                if (offset + 1 >= input.size()
                    || !IsUtf8Continuation(static_cast<unsigned char>(input[offset + 1])))
                {
                    ThrowJsonException("Invalid UTF-8 in JSON string.");
                }
                return 2;
            }
            if (first >= 0xE0U && first <= 0xEFU)
            {
                if (offset + 2 >= input.size())
                {
                    ThrowJsonException("Invalid UTF-8 in JSON string.");
                }
                const auto second = static_cast<unsigned char>(input[offset + 1]);
                const auto third = static_cast<unsigned char>(input[offset + 2]);
                if (!IsUtf8Continuation(second) || !IsUtf8Continuation(third)
                    || (first == 0xE0U && second < 0xA0U)
                    || (first == 0xEDU && second >= 0xA0U))
                {
                    ThrowJsonException("Invalid UTF-8 in JSON string.");
                }
                return 3;
            }
            if (first >= 0xF0U && first <= 0xF4U)
            {
                if (offset + 3 >= input.size())
                {
                    ThrowJsonException("Invalid UTF-8 in JSON string.");
                }
                const auto second = static_cast<unsigned char>(input[offset + 1]);
                const auto third = static_cast<unsigned char>(input[offset + 2]);
                const auto fourth = static_cast<unsigned char>(input[offset + 3]);
                if (!IsUtf8Continuation(second) || !IsUtf8Continuation(third)
                    || !IsUtf8Continuation(fourth)
                    || (first == 0xF0U && second < 0x90U)
                    || (first == 0xF4U && second > 0x8FU))
                {
                    ThrowJsonException("Invalid UTF-8 in JSON string.");
                }
                return 4;
            }
            ThrowJsonException("Invalid UTF-8 in JSON string.");
        }

        void AppendUtf8(std::string& output, char32_t value)
        {
            if (value <= 0x7FU)
            {
                output.push_back(static_cast<char>(value));
            }
            else if (value <= 0x7FFU)
            {
                output.push_back(static_cast<char>(0xC0U | (value >> 6)));
                output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
            }
            else if (value <= 0xFFFFU)
            {
                output.push_back(static_cast<char>(0xE0U | (value >> 12)));
                output.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
                output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
            }
            else
            {
                output.push_back(static_cast<char>(0xF0U | (value >> 18)));
                output.push_back(static_cast<char>(0x80U | ((value >> 12) & 0x3FU)));
                output.push_back(static_cast<char>(0x80U | ((value >> 6) & 0x3FU)));
                output.push_back(static_cast<char>(0x80U | (value & 0x3FU)));
            }
        }

        class JsonParser final
        {
        public:
            explicit JsonParser(std::string_view input) : _input(input)
            {
            }

            [[nodiscard]] JsonValue ParseDocument()
            {
                SkipWhitespace();
                JsonValue value = ParseValue();
                SkipWhitespace();
                if (_offset != _input.size())
                {
                    ThrowJsonException("Additional text encountered after finished reading JSON content.");
                }
                return value;
            }

        private:
            [[nodiscard]] JsonValue ParseValue()
            {
                if (_offset >= _input.size())
                {
                    ThrowJsonException("Expected a JSON value.");
                }
                switch (_input[_offset])
                {
                case 'n':
                {
                    ReadLiteral("null");
                    JsonValue value;
                    value.Kind = JsonKind::Null;
                    return value;
                }
                case 't':
                {
                    ReadLiteral("true");
                    JsonValue value;
                    value.Kind = JsonKind::Boolean;
                    value.Boolean = true;
                    return value;
                }
                case 'f':
                {
                    ReadLiteral("false");
                    JsonValue value;
                    value.Kind = JsonKind::Boolean;
                    value.Boolean = false;
                    return value;
                }
                case '"':
                {
                    JsonValue value;
                    value.Kind = JsonKind::String;
                    value.Text = ParseString();
                    return value;
                }
                case '[':
                    return ParseArray();
                case '{':
                    return ParseObject();
                default:
                    if (_input[_offset] == '-' || (_input[_offset] >= '0' && _input[_offset] <= '9'))
                    {
                        JsonValue value;
                        value.Kind = JsonKind::Number;
                        value.Text = ParseNumber();
                        return value;
                    }
                    ThrowJsonException("Expected a JSON value.");
                }
            }

            [[nodiscard]] JsonValue ParseArray()
            {
                JsonValue value;
                value.Kind = JsonKind::Array;
                ++_offset;
                SkipWhitespace();
                if (Consume(']'))
                {
                    return value;
                }
                for (;;)
                {
                    value.Array.push_back(ParseValue());
                    SkipWhitespace();
                    if (Consume(']'))
                    {
                        return value;
                    }
                    if (!Consume(','))
                    {
                        ThrowJsonException("Expected ',' or ']' in JSON array.");
                    }
                    SkipWhitespace();
                    if (_offset < _input.size() && _input[_offset] == ']')
                    {
                        ThrowJsonException("Trailing commas are not permitted.");
                    }
                }
            }

            [[nodiscard]] JsonValue ParseObject()
            {
                JsonValue value;
                value.Kind = JsonKind::Object;
                ++_offset;
                SkipWhitespace();
                if (Consume('}'))
                {
                    return value;
                }
                for (;;)
                {
                    if (_offset >= _input.size() || _input[_offset] != '"')
                    {
                        ThrowJsonException("Expected a JSON property name.");
                    }
                    std::string name = ParseString();
                    SkipWhitespace();
                    if (!Consume(':'))
                    {
                        ThrowJsonException("Expected ':' after JSON property name.");
                    }
                    SkipWhitespace();
                    value.Object.emplace_back(std::move(name), ParseValue());
                    SkipWhitespace();
                    if (Consume('}'))
                    {
                        return value;
                    }
                    if (!Consume(','))
                    {
                        ThrowJsonException("Expected ',' or '}' in JSON object.");
                    }
                    SkipWhitespace();
                    if (_offset < _input.size() && _input[_offset] == '}')
                    {
                        ThrowJsonException("Trailing commas are not permitted.");
                    }
                }
            }

            [[nodiscard]] std::string ParseString()
            {
                ++_offset;
                std::string result;
                while (_offset < _input.size())
                {
                    const auto byte = static_cast<unsigned char>(_input[_offset]);
                    if (byte == '"')
                    {
                        ++_offset;
                        return result;
                    }
                    if (byte == '\\')
                    {
                        ++_offset;
                        if (_offset >= _input.size())
                        {
                            ThrowJsonException("Incomplete JSON escape sequence.");
                        }
                        const char escape = _input[_offset++];
                        switch (escape)
                        {
                        case '"': result.push_back('"'); break;
                        case '\\': result.push_back('\\'); break;
                        case '/': result.push_back('/'); break;
                        case 'b': result.push_back('\b'); break;
                        case 'f': result.push_back('\f'); break;
                        case 'n': result.push_back('\n'); break;
                        case 'r': result.push_back('\r'); break;
                        case 't': result.push_back('\t'); break;
                        case 'u':
                        {
                            char32_t code = ReadHex4();
                            if (code >= 0xD800U && code <= 0xDBFFU)
                            {
                                if (_offset + 2 > _input.size()
                                    || _input[_offset] != '\\'
                                    || _input[_offset + 1] != 'u')
                                {
                                    ThrowJsonException("Incomplete UTF-16 surrogate pair in JSON string.");
                                }
                                _offset += 2;
                                const char32_t low = ReadHex4();
                                if (low < 0xDC00U || low > 0xDFFFU)
                                {
                                    ThrowJsonException("Invalid UTF-16 surrogate pair in JSON string.");
                                }
                                code = 0x10000U + ((code - 0xD800U) << 10) + (low - 0xDC00U);
                            }
                            else if (code >= 0xDC00U && code <= 0xDFFFU)
                            {
                                ThrowJsonException("Invalid UTF-16 surrogate in JSON string.");
                            }
                            AppendUtf8(result, code);
                            break;
                        }
                        default:
                            ThrowJsonException("Invalid JSON escape sequence.");
                        }
                        continue;
                    }
                    if (byte < 0x20U)
                    {
                        ThrowJsonException("Unescaped control character in JSON string.");
                    }
                    const std::size_t length = Utf8SequenceLength(_input, _offset);
                    result.append(_input.substr(_offset, length));
                    _offset += length;
                }
                ThrowJsonException("Unterminated JSON string.");
            }

            [[nodiscard]] char32_t ReadHex4()
            {
                if (_offset + 4 > _input.size())
                {
                    ThrowJsonException("Incomplete Unicode escape in JSON string.");
                }
                char32_t value = 0;
                for (int i = 0; i < 4; ++i)
                {
                    const char ch = _input[_offset++];
                    value <<= 4;
                    if (ch >= '0' && ch <= '9')
                    {
                        value |= static_cast<char32_t>(ch - '0');
                    }
                    else if (ch >= 'a' && ch <= 'f')
                    {
                        value |= static_cast<char32_t>(ch - 'a' + 10);
                    }
                    else if (ch >= 'A' && ch <= 'F')
                    {
                        value |= static_cast<char32_t>(ch - 'A' + 10);
                    }
                    else
                    {
                        ThrowJsonException("Invalid Unicode escape in JSON string.");
                    }
                }
                return value;
            }

            [[nodiscard]] std::string ParseNumber()
            {
                const std::size_t start = _offset;
                if (Consume('-') && _offset >= _input.size())
                {
                    ThrowJsonException("Invalid JSON number.");
                }
                if (_input[_offset] == '0')
                {
                    ++_offset;
                    if (_offset < _input.size() && _input[_offset] >= '0' && _input[_offset] <= '9')
                    {
                        ThrowJsonException("Leading zeroes are not permitted in JSON numbers.");
                    }
                }
                else
                {
                    if (_input[_offset] < '1' || _input[_offset] > '9')
                    {
                        ThrowJsonException("Invalid JSON number.");
                    }
                    while (_offset < _input.size()
                        && _input[_offset] >= '0' && _input[_offset] <= '9')
                    {
                        ++_offset;
                    }
                }
                if (_offset < _input.size() && _input[_offset] == '.')
                {
                    ++_offset;
                    const std::size_t fraction = _offset;
                    while (_offset < _input.size()
                        && _input[_offset] >= '0' && _input[_offset] <= '9')
                    {
                        ++_offset;
                    }
                    if (_offset == fraction)
                    {
                        ThrowJsonException("Invalid JSON number.");
                    }
                }
                if (_offset < _input.size()
                    && (_input[_offset] == 'e' || _input[_offset] == 'E'))
                {
                    ++_offset;
                    if (_offset < _input.size()
                        && (_input[_offset] == '+' || _input[_offset] == '-'))
                    {
                        ++_offset;
                    }
                    const std::size_t exponent = _offset;
                    while (_offset < _input.size()
                        && _input[_offset] >= '0' && _input[_offset] <= '9')
                    {
                        ++_offset;
                    }
                    if (_offset == exponent)
                    {
                        ThrowJsonException("Invalid JSON number.");
                    }
                }
                return std::string(_input.substr(start, _offset - start));
            }

            void ReadLiteral(std::string_view literal)
            {
                if (_input.substr(_offset, literal.size()) != literal)
                {
                    ThrowJsonException("Invalid JSON literal.");
                }
                _offset += literal.size();
            }

            void SkipWhitespace() noexcept
            {
                while (_offset < _input.size() && IsJsonWhitespace(_input[_offset]))
                {
                    ++_offset;
                }
            }

            [[nodiscard]] bool Consume(char value) noexcept
            {
                if (_offset < _input.size() && _input[_offset] == value)
                {
                    ++_offset;
                    return true;
                }
                return false;
            }

            std::string_view _input;
            std::size_t _offset = 0;
        };

        [[nodiscard]] const JsonValue* TryGetProperty(const JsonValue& value,
            std::string_view name)
        {
            if (value.Kind != JsonKind::Object)
            {
                ThrowInvalidOperation("The requested operation requires an element of type 'Object'.");
            }
            for (auto it = value.Object.rbegin(); it != value.Object.rend(); ++it)
            {
                if (it->first == name)
                {
                    return &it->second;
                }
            }
            return nullptr;
        }

        [[nodiscard]] std::optional<std::string> GetString(const JsonValue& value)
        {
            if (value.Kind == JsonKind::Null)
            {
                return std::nullopt;
            }
            if (value.Kind != JsonKind::String)
            {
                ThrowInvalidOperation("The requested operation requires an element of type 'String'.");
            }
            return value.Text;
        }

        [[nodiscard]] bool TryGetInt64(const JsonValue& value, std::int64_t& result)
        {
            if (value.Kind != JsonKind::Number)
            {
                ThrowInvalidOperation("The requested operation requires an element of type 'Number'.");
            }
            const char* first = value.Text.data();
            const char* last = first + value.Text.size();
            const auto parsed = std::from_chars(first, last, result, 10);
            return parsed.ec == std::errc{} && parsed.ptr == last;
        }

        struct Asset
        {
            std::string Name;
            std::string Url;
            std::int64_t Size = 0;
        };

        [[nodiscard]] std::optional<Asset> PickAsset(const std::vector<Asset>& assets);

        [[nodiscard]] std::string LowerForInvariantAsciiContains(std::string_view input)
        {
            std::string output;
            output.reserve(input.size());
            std::size_t offset = 0;
            while (offset < input.size())
            {
                const auto first = static_cast<unsigned char>(input[offset]);
                if (first >= 'A' && first <= 'Z')
                {
                    output.push_back(static_cast<char>(first + ('a' - 'A')));
                    ++offset;
                    continue;
                }
                if (offset + 3 <= input.size()
                    && static_cast<unsigned char>(input[offset]) == 0xE2U
                    && static_cast<unsigned char>(input[offset + 1]) == 0x84U
                    && static_cast<unsigned char>(input[offset + 2]) == 0xAAU)
                {
                    output.push_back('k');
                    offset += 3;
                    continue;
                }
                const std::size_t length = Utf8SequenceLength(input, offset);
                output.append(input.substr(offset, length));
                offset += length;
            }
            return output;
        }

        class CurlRequestMessage final : public HttpRequestMessage
        {
        public:
            explicit CurlRequestMessage(std::string url) : Url(std::move(url))
            {
            }

            std::string Url;
        };

        class CurlResponseMessage final : public HttpResponseMessage
        {
        public:
            long StatusCode = 0;
            std::string Body;
        };

        struct CurlWriteContext
        {
            std::string* Body = nullptr;
            std::exception_ptr Failure;
        };

        std::size_t CurlWrite(char* data, std::size_t size, std::size_t count,
            void* opaque) noexcept
        {
            const std::size_t bytes = size * count;
            auto* context = static_cast<CurlWriteContext*>(opaque);
            try
            {
                context->Body->append(data, bytes);
                return bytes;
            }
            catch (...)
            {
                context->Failure = std::current_exception();
                return 0;
            }
        }

        [[nodiscard]] std::unique_ptr<HttpResponseMessage> PerformRequest(
            const CurlRequestMessage& request,
            std::chrono::milliseconds timeout,
            std::string_view userAgent,
            std::string_view accept)
        {
            static std::once_flag curlOnce;
            static CURLcode curlInit = CURLE_OK;
            std::call_once(curlOnce, []
            {
                curlInit = curl_global_init(CURL_GLOBAL_DEFAULT);
            });
            if (curlInit != CURLE_OK)
            {
                throw NamedException("HttpRequestException", curl_easy_strerror(curlInit));
            }

            using EasyPtr = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
            EasyPtr easy(curl_easy_init(), &curl_easy_cleanup);
            if (!easy)
            {
                throw NamedException("HttpRequestException", "curl_easy_init failed");
            }

            struct SlistDeleter
            {
                void operator()(curl_slist* list) const noexcept
                {
                    if (list != nullptr)
                    {
                        curl_slist_free_all(list);
                    }
                }
            };
            std::unique_ptr<curl_slist, SlistDeleter> headers;
            curl_slist* rawHeaders = nullptr;
            const std::string acceptHeader = "Accept: " + std::string(accept);
            rawHeaders = curl_slist_append(rawHeaders, acceptHeader.c_str());
            if (rawHeaders == nullptr)
            {
                throw std::bad_alloc();
            }
            headers.reset(rawHeaders);

            auto response = std::make_unique<CurlResponseMessage>();
            CurlWriteContext context{&response->Body, nullptr};
            const std::string url = request.Url;
            const std::string agent(userAgent);

            curl_easy_setopt(easy.get(), CURLOPT_URL, url.c_str());
            curl_easy_setopt(easy.get(), CURLOPT_HTTPGET, 1L);
            curl_easy_setopt(easy.get(), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
            curl_easy_setopt(easy.get(), CURLOPT_HTTPHEADER, headers.get());
            curl_easy_setopt(easy.get(), CURLOPT_USERAGENT, agent.c_str());
            curl_easy_setopt(easy.get(), CURLOPT_TIMEOUT_MS,
                static_cast<long>(timeout.count()));
            curl_easy_setopt(easy.get(), CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(easy.get(), CURLOPT_MAXREDIRS, 50L);
            curl_easy_setopt(easy.get(), CURLOPT_PROTOCOLS_STR, "https");
            curl_easy_setopt(easy.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https");
            curl_easy_setopt(easy.get(), CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(easy.get(), CURLOPT_WRITEFUNCTION, &CurlWrite);
            curl_easy_setopt(easy.get(), CURLOPT_WRITEDATA, &context);

            const CURLcode code = curl_easy_perform(easy.get());
            if (context.Failure)
            {
                std::rethrow_exception(context.Failure);
            }
            if (code != CURLE_OK)
            {
                if (code == CURLE_OPERATION_TIMEDOUT)
                {
                    throw NamedException("TaskCanceledException", curl_easy_strerror(code));
                }
                throw NamedException("HttpRequestException", curl_easy_strerror(code));
            }
            curl_easy_getinfo(easy.get(), CURLINFO_RESPONSE_CODE, &response->StatusCode);
            return response;
        }

        class CurlAwaiter final : public HttpResponseAwaiter
        {
        public:
            explicit CurlAwaiter(std::unique_ptr<HttpResponseMessage> response)
                : _response(std::move(response))
            {
            }

            explicit CurlAwaiter(std::exception_ptr failure)
                : _failure(std::move(failure))
            {
            }

            [[nodiscard]] std::unique_ptr<HttpResponseMessage> GetResult() override
            {
                if (_failure)
                {
                    std::rethrow_exception(_failure);
                }
                return std::move(_response);
            }

        private:
            std::unique_ptr<HttpResponseMessage> _response;
            std::exception_ptr _failure;
        };

        class CurlTask final : public HttpResponseTask
        {
        public:
            explicit CurlTask(std::unique_ptr<HttpResponseMessage> response)
                : _awaiter(std::move(response))
            {
            }

            explicit CurlTask(std::exception_ptr failure)
                : _awaiter(std::move(failure))
            {
            }

            [[nodiscard]] HttpResponseAwaiter& GetAwaiter() override
            {
                return _awaiter;
            }

        private:
            CurlAwaiter _awaiter;
        };

        class CurlClient final : public HttpClient
        {
        public:
            explicit CurlClient(std::chrono::milliseconds timeout) : _timeout(timeout)
            {
            }

            void AddUserAgent(std::string value)
            {
                _userAgent = std::move(value);
            }

            void AddAccept(std::string value)
            {
                _accept = std::move(value);
            }

            [[nodiscard]] std::unique_ptr<HttpResponseTask> SendAsync(
                HttpRequestMessage* request,
                HttpCompletionOption completion,
                CancellationToken cancel) override
            {
                (void)cancel;
                try
                {
                    if (completion != HttpCompletionOption::ResponseContentRead)
                    {
                        throw NamedException("ArgumentOutOfRangeException", "completionOption");
                    }
                    auto* concrete = dynamic_cast<CurlRequestMessage*>(request);
                    if (concrete == nullptr)
                    {
                        throw NullReferenceException();
                    }
                    return std::make_unique<CurlTask>(PerformRequest(*concrete,
                        _timeout, _userAgent, _accept));
                }
                catch (...)
                {
                    return std::make_unique<CurlTask>(std::current_exception());
                }
            }

        private:
            std::chrono::milliseconds _timeout;
            std::string _userAgent;
            std::string _accept;
        };

        [[nodiscard]] std::string ExceptionTypeName(const std::exception& ex)
        {
            if (const auto* named = dynamic_cast<const NamedException*>(&ex))
            {
                return named->Name();
            }
            if (dynamic_cast<const NullReferenceException*>(&ex) != nullptr)
            {
                return "NullReferenceException";
            }
            if (dynamic_cast<const std::bad_alloc*>(&ex) != nullptr)
            {
                return "OutOfMemoryException";
            }
            if (dynamic_cast<const std::out_of_range*>(&ex) != nullptr)
            {
                return "ArgumentOutOfRangeException";
            }
            if (dynamic_cast<const std::invalid_argument*>(&ex) != nullptr)
            {
                return "ArgumentException";
            }
            return "Exception";
        }

        [[nodiscard]] std::optional<Asset> PickAsset(const std::vector<Asset>& assets)
        {
            const std::string rid = UpdateCheck::Rid();
            for (const Asset& asset : assets)
            {
                const std::string name = LowerForInvariantAsciiContains(asset.Name);
                if (name.find(rid) == std::string::npos)
                {
                    continue;
                }
                const bool containsServer = name.find("-server-") != std::string::npos;
                if (containsServer != UpdateCheck::IsServerBuild())
                {
                    continue;
                }
                return asset;
            }
            return std::nullopt;
        }
    }

    std::optional<std::string> UpdateCheck::LastReason()
    {
        std::lock_guard lock(LastReasonMutex);
        return LastReasonValue;
    }

    void UpdateCheck::SetLastReason(std::optional<std::string> reason)
    {
        std::lock_guard lock(LastReasonMutex);
        LastReasonValue = std::move(reason);
    }

    std::optional<UpdateInfo> UpdateCheck::Latest(CancellationToken cancel)
    {
        SetLastReason(std::nullopt);
        if (!BuildVersion::IsRelease())
        {
            SetLastReason("this is a local build, so it is left alone");
            return std::nullopt;
        }

        std::string json;
        try
        {
            CurlClient client(std::chrono::duration_cast<std::chrono::milliseconds>(Timeout));
            client.AddUserAgent(std::string(Mods::Branding::FileName) + "/"
                + BuildVersion::Display());
            client.AddAccept("application/vnd.github+json");
            CurlRequestMessage request(ApiUrl());
            std::unique_ptr<HttpResponseMessage> response = SyncHttp::Send(&client,
                &request, HttpCompletionOption::ResponseContentRead, cancel);
            auto* concrete = dynamic_cast<CurlResponseMessage*>(response.get());
            if (concrete == nullptr)
            {
                throw NullReferenceException();
            }
            if (concrete->StatusCode == 404)
            {
                SetLastReason("no releases have been published yet");
                return std::nullopt;
            }
            if (concrete->StatusCode == 403 || concrete->StatusCode == 429)
            {
                SetLastReason("GitHub is rate-limiting this address; try later");
                return std::nullopt;
            }
            if (concrete->StatusCode < 200 || concrete->StatusCode > 299)
            {
                SetLastReason("GitHub answered " + std::to_string(concrete->StatusCode));
                return std::nullopt;
            }
            json = concrete->Body;
        }
        catch (const std::exception& ex)
        {
            SetLastReason("could not reach GitHub (" + ExceptionTypeName(ex) + ")");
            return std::nullopt;
        }
        catch (...)
        {
            SetLastReason("could not reach GitHub (Exception)");
            return std::nullopt;
        }
        return Parse(json);
    }

    std::optional<UpdateInfo> UpdateCheck::Parse(std::string_view json,
        std::optional<Version> installed)
    {
        SetLastReason(std::nullopt);
        if (!installed.has_value())
        {
            installed = BuildVersion::Current();
        }
        if (!installed.has_value())
        {
            SetLastReason("this is a local build, so it is left alone");
            return std::nullopt;
        }

        std::string tag;
        std::string notes;
        std::string page;
        std::vector<Asset> assets;
        try
        {
            const JsonValue root = JsonParser(json).ParseDocument();
            if (const JsonValue* value = TryGetProperty(root, "tag_name"))
            {
                tag = GetString(*value).value_or("");
            }
            if (const JsonValue* value = TryGetProperty(root, "body"))
            {
                notes = GetString(*value).value_or("");
            }
            if (const JsonValue* value = TryGetProperty(root, "html_url"))
            {
                page = GetString(*value).value_or("");
            }
            if (const JsonValue* list = TryGetProperty(root, "assets"))
            {
                if (list->Kind != JsonKind::Array)
                {
                    ThrowInvalidOperation("The requested operation requires an element of type 'Array'.");
                }
                for (const JsonValue& item : list->Array)
                {
                    std::string name;
                    std::string url;
                    std::int64_t size = 0;
                    if (const JsonValue* value = TryGetProperty(item, "name"))
                    {
                        name = GetString(*value).value_or("");
                    }
                    if (const JsonValue* value = TryGetProperty(item, "browser_download_url"))
                    {
                        url = GetString(*value).value_or("");
                    }
                    if (const JsonValue* value = TryGetProperty(item, "size"))
                    {
                        std::int64_t parsedSize = 0;
                        if (TryGetInt64(*value, parsedSize))
                        {
                            size = parsedSize;
                        }
                    }
                    if (!name.empty())
                    {
                        assets.push_back(Asset{std::move(name), std::move(url), size});
                    }
                }
            }
        }
        catch (const NamedException& ex)
        {
            if (ex.Name() == "JsonException")
            {
                SetLastReason("GitHub's answer could not be read");
                return std::nullopt;
            }
            throw;
        }

        const std::optional<Version> published = BuildVersion::Parse(tag);
        if (!published.has_value())
        {
            SetLastReason("the latest release (" + tag + ") is not a plain version tag");
            return std::nullopt;
        }
        const Version current = BuildVersion::Normalise(*installed);
        if (*published <= current)
        {
            SetLastReason("v" + current.ToString(3) + " is already the latest");
            return std::nullopt;
        }

        const std::optional<Asset> package = PickAsset(assets);
        UpdateInfo info;
        info.Tag = std::move(tag);
        info.Version = *published;
        info.AssetName = package.has_value() ? package->Name : std::string();
        info.AssetUrl = package.has_value() ? package->Url : std::string();
        info.AssetSize = package.has_value() ? package->Size : 0;
        info.PageUrl = !page.empty() ? std::move(page) : std::string(ReleasesPage);
        info.Notes = std::move(notes);
        return info;
    }

    bool UpdateCheck::IsServerBuild() noexcept
    {
#if defined(MPHREAD_SERVER)
        return true;
#else
        return false;
#endif
    }

    std::string UpdateCheck::Rid()
    {
#if defined(__ANDROID__)
        return "android";
#else
    #if defined(_WIN32)
        constexpr std::string_view os = "win";
    #elif defined(__APPLE__)
        constexpr std::string_view os = "osx";
    #else
        constexpr std::string_view os = "linux";
    #endif

    #if defined(__x86_64__) || defined(_M_X64)
        constexpr std::string_view arch = "x64";
    #elif defined(__aarch64__) || defined(_M_ARM64)
        constexpr std::string_view arch = "arm64";
    #elif defined(__i386__) || defined(_M_IX86)
        constexpr std::string_view arch = "x86";
    #elif defined(__arm__) || defined(_M_ARM)
        #if defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) \
            || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) \
            || defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6T2__)
        constexpr std::string_view arch = "armv6";
        #else
        constexpr std::string_view arch = "arm";
        #endif
    #elif defined(__wasm__)
        constexpr std::string_view arch = "wasm";
    #elif defined(__s390x__)
        constexpr std::string_view arch = "s390x";
    #elif defined(__loongarch64)
        constexpr std::string_view arch = "loongarch64";
    #elif defined(__powerpc64__) && defined(__LITTLE_ENDIAN__)
        constexpr std::string_view arch = "ppc64le";
    #elif defined(__riscv) && __riscv_xlen == 64
        constexpr std::string_view arch = "riscv64";
    #else
        constexpr std::string_view arch = "unknown";
    #endif
        return std::string(os) + "-" + std::string(arch);
#endif
    }

    std::string UpdateCheck::PackageSuffix()
    {
        return IsServerBuild() ? "server-" + Rid() : Rid();
    }

    std::string UpdateCheck::BinaryName()
    {
#if !defined(_WIN32)
        return std::string(Mods::Branding::FileName);
#else
        std::string name(Mods::Branding::FileName);
        if (IsServerBuild())
        {
            name += "Server";
        }
        return name + ".exe";
#endif
    }
}
