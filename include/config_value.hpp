#ifndef CARL_CONFIG_VALUE_HPP
#define CARL_CONFIG_VALUE_HPP

#include "config_exception.hpp"
#include "constraints.hpp"
#include "iconfig_value.hpp"

#include <fmt/format.h>

#include <cassert>
#include <optional>

namespace CARL
{

enum class Required : bool { YES = true, NO = false, };


template <typename T>
class ConfigValue : public IConfigValue
{
public:
    static_assert(is_printable<T>::value, "ConfigValue<T>: T must support operator<<(ostream)");
    static_assert(is_yaml_parsable<T>::value, "ConfigValue<T>: T must be parsable via YAML::Node::as<T>()");

public:
    ConfigValue(std::string name, Required required = Required::YES)
        : name_(std::move(name)),
          required_(required) {}

    ConfigValue(std::string name, Default<T> default_value)
        : name_(std::move(name)),
          required_(Required::NO),
          value_(std::move(default_value.value)) {}


    ConfigValue(const ConfigValue&)             = default;
    ConfigValue(ConfigValue&&)                  = default;
    ConfigValue& operator =(const ConfigValue&) = default;
    ConfigValue& operator =(ConfigValue&&)      = default;


public:
    void parse(YAML::Node const& node) override
    {
        if (!node.IsMap()) {
            throw ParsingError("expected a map node while parsing field '{}'", name_);
        }

        if (auto field = node[name_]) {
            try {
                value_     = field.as<T>();
                wasParsed_ = true;
            }
            catch (YAML::Exception const& e) {
                throw ParsingError("Could not parse {}, with: {}", name_, e.what());
            }
        }
    }

    [[nodiscard]] ValidationResult validate() const override
    {
        if ((required_ == Required::YES) && !value_.has_value()) {
            return ValidationResult::failure(fmt::format("{} required field is missing", name_));
        }

        return ValidationResult::success();
    }

    void printTo(std::ostream& os, std::string_view indent) const override
    {
        if (value_) {
            os << indent << fmt::format("{}: {} {}\n", name_, *value_, wasParsed_? "" : "(default)");
        }
        else {
            os << indent << fmt::format("{}: {}\n", name_, "<not set>");
        }
    }

public:
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

    bool wasParsed_ {false};
};

} // namespace CARL

#endif // CARL_I_CONFIG_VALUE_HPP
