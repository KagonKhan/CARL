#ifndef CARL_VALIDATION_RESULT_HPP
#define CARL_VALIDATION_RESULT_HPP

#include <string>

namespace CARL
{

struct ValidationResult
{
    bool correct;
    std::string errorMessage;

    static ValidationResult success()                    { return {true, {}}; }
    static ValidationResult failure(std::string message) { return {false, std::move(message)}; }
};

}    // namespace CARL

#endif // CARL_VALIDATION_RESULT_HPP
