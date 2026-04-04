#ifndef CARL_VALIDATION_RESULT_HPP
#define CARL_VALIDATION_RESULT_HPP

#include <string>
#include <vector>

namespace CARL
{

struct ValidationResult
{
    bool correct;
    std::vector<std::string> errors;

    static ValidationResult success()                    { return {true, {}}; }
    static ValidationResult failure(std::string message) { return {false, {std::move(message)}}; }

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


// struct Required
// {
//     enum Value
//     {
//         NO  = 0,
//         YES = 1,
//     };

//     Value value;

//     constexpr Required(Value v)
//         : value(v) {}
//     constexpr Required(bool b)
//         : value(b? YES : NO) {}

//     constexpr explicit operator bool() const             { return value == YES; }
//     constexpr bool     operator !() const                { return value == NO; }
//     constexpr bool     operator ==(Required other) const { return value == other.value; }
//     constexpr bool     operator !=(Required other) const { return value != other.value; }

//     static const Required YES;
//     static const Required NO;
// };

// constexpr Required Required::YES {Required::Value::YES};
// constexpr Required Required::NO {Required::Value::NO};

} // namespace CARL

#endif // CARL_VALIDATION_RESULT_HPP
