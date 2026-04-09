#ifndef CARL_UTILS_UTILS_HPP
#define CARL_UTILS_UTILS_HPP

#include <fmt/core.h>

#include <string>


/// @brief Wrapper for validating the entire config tree, allowing for merging of messages.
struct ValidationResult
{
    bool correct;
    std::vector<std::string> errors;

    static constexpr ValidationResult success() { return {true, {}}; }

    template <typename ... Args>
    static constexpr ValidationResult failure(fmt::format_string<Args...> fmt_str, Args&&... args)
    {
        return {false, {fmt::format(fmt_str, std::forward<Args>(args)...)}};
    }

    void merge(ValidationResult other)
    {
        if (!other.correct) {
            correct = false;
            errors.insert(errors.end(), other.errors.begin(), other.errors.end());
        }
    }
};


/// @brief Strong type wrapper for T
/// @tparam T wrapped type
template <typename T>
struct Default
{
    T value;

    explicit Default(T v)
        : value(std::move(v)) {}
};


/// @brief Strong type wrapper for debug information from where a config value comes from
class ValueSource
{
public:
    enum class Kind { UNSET, DEFAULT, PARSED, };

    static constexpr ValueSource unset() noexcept       { return ValueSource(Kind::UNSET);   }
    static constexpr ValueSource fromDefault() noexcept { return ValueSource(Kind::DEFAULT); }
    static constexpr ValueSource parsed() noexcept      { return ValueSource(Kind::PARSED);  }

public:
    constexpr ValueSource() noexcept = default;
    constexpr explicit ValueSource(Kind kind) noexcept
        : kind_(kind) {}

    constexpr void markParsed() noexcept { kind_ = Kind::PARSED; }

    [[nodiscard]] constexpr bool isSet() const noexcept     { return kind_ != Kind::UNSET;   }
    [[nodiscard]] constexpr bool isDefault() const noexcept { return kind_ == Kind::DEFAULT; }
    [[nodiscard]] constexpr bool isParsed() const noexcept  { return kind_ == Kind::PARSED;  }

    [[nodiscard]] constexpr std::string_view displayTag() const noexcept
    {
        switch (kind_) {
        case Kind::UNSET:
            return "<missing>";

        case Kind::DEFAULT:
            return "(default)";

        case Kind::PARSED:
            return "";
        }
        return "";  // unreachable, silences -Wreturn-type
    }

private:
    Kind kind_ {Kind::UNSET};
};


/// @brief Strong type for a boolean
struct Required
{
    constexpr explicit Required(bool b)
        : value(b) {}

    constexpr explicit operator bool() const             { return value == true; }
    constexpr bool     operator !() const                { return !value; }
    constexpr bool     operator ==(Required other) const { return value == other.value; }
    constexpr bool     operator !=(Required other) const { return !(value == other.value); }

    static const Required YES;
    static const Required NO;

private:
    bool value;
};

constexpr Required Required::YES {true};
constexpr Required Required::NO {false};


/// @brief split text and indent all lines if text contains a new line, otherwise noop
/// @param text text to reindent
/// @param indent level of indentation
/// @return formatted string
inline std::string reindent(std::string_view text, std::string_view indent)
{
    if (text.find('\n') == std::string_view::npos) {
        return std::string {text};
    }

    std::string result;

    while (!text.empty()) {
        auto pos  = text.find('\n');
        auto line = text.substr(
            0,
            pos == std::string_view::npos? pos : (pos > 0 && text[pos - 1] == '\r'? pos - 1 : pos)
        );

        result += fmt::format("\n{}  {}", indent, line);

        if (pos == std::string_view::npos) {
            break;
        }

        text.remove_prefix(pos + 1);
    }

    return result;
}

#endif // CARL_UTILS_UTILS_HPP
