#ifndef CARL_CONFIG_GROUP_HPP
#define CARL_CONFIG_GROUP_HPP

#include "config_value.hpp"

#include <vector>

namespace CARL
{

/// @brief if a group contains ID member and will be used inside other structures (lists, maps), ID member HAS to be first
class ConfigGroup : public IConfigValue
{
public:
    void                             parse(YAML::Node const& node) override;
    [[nodiscard]] ValidationResult   validate() const override;
    void                             printTo(std::ostream& os, std::string const& indent = "") const override;
    [[nodiscard]] std::string const& name() const noexcept override { return name_; }

protected:
    explicit ConfigGroup(std::string name = "", Required required = Required::YES)
        : name_(name),
          required_(required) {}

    template <typename ... Entries>
    void register_(Entries&... entries) { (entries_.push_back(&entries), ...); }

private:
    std::string name_;
    Required    required_;

    std::vector<IConfigValue*> entries_;
};

} // namespace CARL

#endif // CARL_CONFIG_GROUP_HPP
