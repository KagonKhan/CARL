#include "config_group.hpp"

namespace CARL
{

void ConfigGroup::parse(YAML::Node const& node)
{
    if (!node.IsMap() || !node[name_]) {
        return;
    }


    if (auto group = node[name_]) {
        for (auto& entry : entries_) {
            entry->parse(node[name_]);
        }
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
            subentry = fmt::format("{}.{}", name_, subentry);
        }

        result.merge(entry_result);
    }

    return result;
}

void ConfigGroup::printTo(std::ostream& os, std::string_view indent) const
{
    os << indent << name_ << ":\n";
    std::string indentation = std::string(indent) + "  ";
    for (auto& entry : entries_) {
        entry->printTo(os, indentation);
    }
}

} // namespace CARL
