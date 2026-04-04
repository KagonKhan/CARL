#ifndef CARL_CONFIG_GROUP_HPP
#define CARL_CONFIG_GROUP_HPP

#include "config_value.hpp"

#include <vector>

namespace CARL
{

class ConfigGroup : public IConfigValue
{
public:
    void parse(YAML::Node const& node) override;

    [[nodiscard]] ValidationResult validate() const override;

    void printTo(std::ostream& os, std::string_view indent = "") const override;

protected:
    explicit ConfigGroup(std::string name, Required required = Required::YES)
        : name_(name),
          required_(required) {}

    void register_(IConfigValue* value) { entries_.push_back(value); }

private:
    std::string name_;
    Required    required_;

    std::vector<IConfigValue*> entries_;
};

} // namespace CARL

#endif // CARL_CONFIG_GROUP_HPP
