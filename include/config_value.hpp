#ifndef CARL_CONFIG_VALUE_HPP
#define CARL_CONFIG_VALUE_HPP

#include "utils/parsing_extensions.hpp"

#include "iconfig_value.hpp"
#include "utils/constraints.hpp"
#include "utils/exceptions.hpp"
#include "utils/utils.hpp"

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
    explicit ConfigValue(std::string name, Required required = Required::YES);
    explicit ConfigValue(std::string name, Default<T> default_value);

    ConfigValue(const ConfigValue&)             = delete;
    ConfigValue(ConfigValue&&)                  = delete;
    ConfigValue& operator =(const ConfigValue&) = delete;
    ConfigValue& operator =(ConfigValue&&)      = delete;


public:
    void                           parse(YAML::Node const& node) override;
    [[nodiscard]] ValidationResult validate() const override;
    void                           printTo(std::ostream& os, std::string const& indent) const override;
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

    /// @brief function provided for edge-cases. use sparingly
    void     patch(T value)               { value_ = std::move(value); }
    const T& operator *() const& noexcept { return assertInitialized(), *value_; }

private:
    std::string      name_;
    Required         required_;
    std::optional<T> value_;
    ValueSource      source_;

    void assertInitialized() const
    {
        assert(value_.has_value() && "dereferencing unset ConfigValue, did you call validate()?");
    }
};

} // namespace CARL

#include "config_value.inl"

#endif // CARL_CONFIG_VALUE_HPP
