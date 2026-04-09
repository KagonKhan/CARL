#ifndef CARL_CONFIG_GROUP_HPP
#define CARL_CONFIG_GROUP_HPP

#include "config_value.hpp"
#include "utils/utils.hpp"

#include <vector>

namespace CARL
{

/// @brief if a group contains ID member and will be used inside other structures (lists, maps), ID member HAS to be first
class ConfigGroup : public IConfigValue
{
public:
    void                           parse(YAML::Node const& node) override;
    [[nodiscard]] ValidationResult validate() const override;
    void                           printTo(std::ostream& os, std::string const& indent = "") const override;
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }

protected:
    explicit ConfigGroup(std::string name = "", Required required = Required::YES)
        : name_(std::move(name)),
          required_(required) {}
    ~ConfigGroup() noexcept override = default;

    ConfigGroup(const ConfigGroup&)             = delete;
    ConfigGroup(ConfigGroup&&)                  = delete;
    ConfigGroup& operator =(const ConfigGroup&) = delete;
    ConfigGroup& operator =(ConfigGroup&&)      = delete;

    template <typename ... Entries>
    void registerEntries(Entries&... entries)
    {
        static_assert(
            (std::is_base_of_v<IConfigValue, Entries> && ...),
            "All entries passed to registerEntries() must derive from IConfigValue"
        );
        (entries_.push_back(&entries), ...);
    }

private:
    std::string name_;
    Required    required_;

    std::vector<IConfigValue*> entries_;
};

} // namespace CARL

#endif // CARL_CONFIG_GROUP_HPP
