#ifndef CARL_CONFIG_VALUE_HPP
#define CARL_CONFIG_VALUE_HPP

#include "iconfig_value.hpp"
#include "utils/config_exception.hpp"
#include "utils/constraints.hpp"

#include <fmt/format.h>

#include <cassert>
#include <optional>

namespace CARL
{

template <typename T>
class ConfigValue : public IConfigValue
{
public:
    static_assert(is_printable<T>::value, "ConfigValue<T>: T must support operator<<(ostream)");
    static_assert(is_yaml_parsable<T>::value, "ConfigValue<T>: T must be parsable via YAML::Node::as<T>()");

public:
    explicit constexpr ConfigValue(std::string name, Required required = Required::YES);
    explicit constexpr ConfigValue(std::string name, Default<T> default_value);

    // ConfigValue(const ConfigValue&)             = default;
    // ConfigValue(ConfigValue&&)                  = default;
    // ConfigValue& operator =(const ConfigValue&) = default;
    // ConfigValue& operator =(ConfigValue&&)      = default;


public:
    void                             parse(YAML::Node const& node) override;
    [[nodiscard]] ValidationResult   validate() const override;
    void                             printTo(std::ostream& os, std::string const& indent) const override;
    [[nodiscard]] std::string const& name() const noexcept override { return name_; }

    /// @brief function provided for edge-cases. use sparingly
    void     patch(T value)               { value_ = std::move(value); }
    const T& operator *() const& noexcept { return assertInitialized(), *value_; }

private:
    std::string      name_;
    Required         required_;
    std::optional<T> value_;
    bool             wasParsed_ {false};

    void assertInitialized() const
    {
        assert(value_.has_value() && "dereferencing unset ConfigValue, did you call validate()?");
    }
};

} // namespace CARL

#include "config_value.inl"

#endif // CARL_I_CONFIG_VALUE_HPP
