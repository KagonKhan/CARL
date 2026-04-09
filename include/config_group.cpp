#include "config_group.hpp"

namespace
{

bool containsMember(std::vector<CARL::IConfigValue*> const& vec, std::string member_name)
{
    return std::any_of(
        vec.begin(),
        vec.end(),
        [&member_name] (CARL::IConfigValue const* entry) { return entry->name() == member_name; }
    );
}

} // namespace


namespace CARL
{

void ConfigGroup::parse(YAML::Node const& node)
{
    if (containsMember(entries_, "id")) {
        assert((entries_.front()->name() == "id") && "id member is required to be first if present");
    }

    // Allow for parsing nameless groups -> main level or list entry
    YAML::Node entry_node = name_.empty()? node : node[name_];
    if (!entry_node) {
        return;
    }


    for (auto* entry : entries_) {
        entry->parse(entry_node);
    }
}

[[nodiscard]] ValidationResult ConfigGroup::validate() const
{
    if (!required_) {
        return {};
    }

    ValidationResult result = ValidationResult::success();

    for (auto const* entry : entries_) {
        ValidationResult prefixed = entry->validate();

        if (!name_.empty()) {
            for (auto& prefixed_error : prefixed.errors) {
                prefixed_error = fmt::format("{}.{}", name_, prefixed_error);
            }
        }

        result.merge(prefixed);
    }

    return result;
}

void ConfigGroup::printTo(std::ostream& os, std::string const& indent) const
{
    if (!name_.empty()) {
        os << indent << name_ << ":\n";
    }

    std::string sub_indent = indent + std::string(name_.empty()? 0 : 2, ' ');
    for (auto* entry : entries_) {
        entry->printTo(os, sub_indent);
    }

    // WARNING: this might be brittle, if you decide to print the config with an indent it would break.
    //          I'm not sure it's necessary to work on a "clean" solution right now, as it would require adding members
    //          to ConfigGroup that signify "isBase" or something like that
    if (bool is_base_level = indent.empty(); is_base_level) {
        os << "\n\n";
    }
}

} // namespace CARL
