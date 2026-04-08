#ifndef CARL_CONFIG_MAP_HPP
#define CARL_CONFIG_MAP_HPP

#include "iconfig_value.hpp"
#include "utils/config_exception.hpp"

#include <iostream>
#include <map>
#include <vector>

namespace CARL
{

enum class MapType : uint8_t { ID_LIST, STANDARD, };

/// @brief Wrapper for Config Maps. Can parse usual Map, or a List containing ID members
template <typename Group, typename KeyType = int>
class ConfigMap : public IConfigValue
{
public:
    // static asserts


public:
    explicit constexpr ConfigMap(std::string name, MapType type = MapType::STANDARD, Required required = Required::YES)
        : name_(std::move(name)),
          mapType_(type),
          isRequired_(required) {}

    void                             parse(YAML::Node const& node) override;
    [[nodiscard]] ValidationResult   validate() const override;
    void                             printTo(std::ostream& os, std::string const& indent = "") const override;
    [[nodiscard]] std::string const& name() const noexcept override { return name_; }

private:
    std::string name_;
    Required    isRequired_;
    MapType     mapType_;

    std::map<KeyType, std::unique_ptr<Group>> entries_;
    bool                                      wasParsed_ {false};

    constexpr std::string niceName() const { return name_.empty()? "`nameless map`" : name_; }
};

} // namespace CARL


#include "config_map.inl"

#endif // CARL_CONFIG_MAP_HPP
