#ifndef CARL_I_CONFIG_VALUE_HPP
#define CARL_I_CONFIG_VALUE_HPP

#include "utils/utils.hpp"

#include <yaml-cpp/yaml.h>

#include <ostream>
#include <string_view>


namespace CARL
{

/**
 * @brief Base requirement for a config value
 * @details Validate the correctess by calling "validate". Access to values is provided without any guards, so the
 *          responsibility lies on the user to make sure the structure is in a valid state.
 *          Parsing is designed with overwriting in mind, you may provide multiple config files, and the last one will
 *          prevail
 */
class IConfigValue
{
public:
    virtual ~IConfigValue() = default;

    virtual void                           parse(YAML::Node const& node)                              = 0;
    [[nodiscard]] virtual ValidationResult validate() const                                           = 0;
    virtual void                           printTo(std::ostream& os, std::string const& indent) const = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept                                      = 0;
};

/// @brief true for nodes that may be indexed by key. yaml-cpp throws BadSubscript when a scalar is subscripted, and a
///        null node stands in for an empty mapping, so both must be screened before reaching into a node.
/// @note  the node must be valid (defined or zombie-checked by the caller); querying the type of an invalid node throws
[[nodiscard]] inline bool isMapOrNull(YAML::Node const& node)
{
    return node.IsNull() || node.IsMap();
}

} // namespace CARL

#endif // CARL_I_CONFIG_VALUE_HPP
