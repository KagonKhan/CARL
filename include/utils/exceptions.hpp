#ifndef CARL_CONFIG_EXCEPTION_HPP
#define CARL_CONFIG_EXCEPTION_HPP

#include <fmt/core.h>

#include <stdexcept>

namespace CARL
{

struct FormattedException : std::runtime_error
{
    template <typename ... Args>
    explicit FormattedException(fmt::format_string<Args...> fmt_str, Args&&... args)
        : std::runtime_error(fmt::format(fmt_str, std::forward<Args>(args)...)) {}
};

struct ParsingError : FormattedException
{
    using FormattedException::FormattedException;
};

} // namespace CARL

#endif // CARL_CONFIG_EXCEPTION_HPP
