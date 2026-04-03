#ifndef CARL_CONFIG_VALUE_HPP
#define CARL_CONFIG_VALUE_HPP

#include "config_exception.hpp"
#include "iconfig_value.hpp"

#include <fmt/format.h>

#include <cassert>
#include <optional>

namespace CARL
{

enum class Required : bool { YES = false, NO = true, };

template <typename T>
struct Default
{
    T value;

    explicit Default(T v)
        : value(std::move(v)) {}
};

template <typename T>
class ConfigValue : public IConfigValue
{
public:
    ConfigValue(std::string name, Required required)
        : name_(std::move(name)),
          required_(required) {}

    ConfigValue(std::string name, Default<T> default_value)
        : name_(std::move(name)),
          required_(Required::YES),
          value_(std::move(default_value.value)) {}

    void parse(YAML::Node const& node) override
    {
        if (auto field = node[name_]) {
            try {
                value_ = field.as<T>();
            }
            catch (YAML::Exception const& e) {
                throw ParsingError("Could not parse {}, with: {}", name_, e.what());
            }
        }
    }

    [[nodiscard]] ValidationResult validate() const override
    {
        if ((required_ == Required::TRUE) && !value_.has_value()) {
            return ValidationResult::failure(fmt::format("required field is missing: {}", name_));
        }

        return ValidationResult::success();
    }

    void printTo(std::ostream& os, std::string_view indent) const override
    {
        if (value_) {
            os << indent << fmt::format("{}: {}\n", name_, *value_);
        }
        else {
            os << indent << fmt::format("{}: {}\n", name_, "<not set>");
        }
    }

    /// @brief function provided for edge-cases. use sparingly
    void patch(T value) { value_ = std::move(value); }

    const T& operator *() const& noexcept
    {
        assert(value_.has_value() && "dereferencing unset ConfigValue, did you call validate()?");
        return *value_;
    }

private:
    std::string name_;
    Required    required_;

    std::optional<T> value_;
};

} // namespace CARL

#endif // CARL_I_CONFIG_VALUE_HPP
