#include "config_group.hpp"
#include <iostream>
namespace CARL
{

void ConfigGroup::parse(YAML::Node const& node)
{
    if (!node.IsMap()) {
        return;
    }

    // Allow for parsing nameless groups -> main level or list entry
    YAML::Node entry_node = name_.empty()? node : node[name_];
    if (!entry_node) {
        return;
    }


    for (auto& entry : entries_) {
        entry->parse(entry_node);
    }
}

[[nodiscard]] ValidationResult ConfigGroup::validate() const
{
    if (required_ == Required::NO) {
        return {};
    }

    ValidationResult result = ValidationResult::success();

    for (auto& entry : entries_) {
        auto entry_result = entry->validate();

        for (auto& subentry : entry_result.errors) {
            if (name_.empty()) {
                subentry = fmt::format("{}", subentry);
            }
            else {
                subentry = fmt::format("{}.{}", name_, subentry);
            }
        }

        result.merge(entry_result);
    }

    return result;
}

void ConfigGroup::printTo(std::ostream& os, std::string_view indent) const
{
    std::string sub_indent = std::string(indent) + std::string(name_.empty()? 0 : 2, ' ');

    if (!name_.empty()) {
        os << indent << name_ << ":\n";
    }

    for (auto& entry : entries_) {
        entry->printTo(os, sub_indent);
    }
}

} // namespace CARL
