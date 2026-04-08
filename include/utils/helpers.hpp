#ifndef CARL_VALIDATION_RESULT_HPP
#define CARL_VALIDATION_RESULT_HPP

#include <fmt/core.h>

#include <string>
#include <vector>

namespace CARL
{

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


template <typename T>
struct Default
{
    T value;

    explicit Default(T v)
        : value(std::move(v)) {}
};


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

} // namespace CARL

#endif // CARL_VALIDATION_RESULT_HPP
