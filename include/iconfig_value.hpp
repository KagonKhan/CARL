#ifndef CARL_I_CONFIG_VALUE_HPP
#define CARL_I_CONFIG_VALUE_HPP

#include "helpers.hpp"

#include <yaml-cpp/yaml.h>

namespace CARL
{

class IConfigValue
{
public:
    virtual ~IConfigValue() = default;

    virtual void                           parse(YAML::Node const& node)                            = 0;
    [[nodiscard]] virtual ValidationResult validate() const                                         = 0;
    virtual void                           printTo(std::ostream& os, std::string_view indent) const = 0;
};

} // namespace CARL

#endif // CARL_I_CONFIG_VALUE_HPP
