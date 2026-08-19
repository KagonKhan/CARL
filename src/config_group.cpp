#include "config_group.hpp"

#include "utils/exceptions.hpp"

#include <algorithm>
#include <string>

namespace CARL
{

void ConfigGroup::parse(YAML::Node const& node)
{
    if (!name_.empty() && !isMapOrNull(node)) {
        throw ParsingError("expected a map node while looking for group '{}'", niceName());
    }

    // Allow for parsing nameless groups -> main level or list entry
    YAML::Node entry_node = name_.empty()? node : node[name_];
    if (!entry_node) {
        return;
    }

    if (!isMapOrNull(entry_node)) {
        throw ParsingError("expected a map node for group '{}'", niceName());
    }

    wasParsed_ = true;

    for (auto* entry : entries_) {
        entry->parse(entry_node);
    }
}

[[nodiscard]] ValidationResult ConfigGroup::validate() const
{
    // a patched field counts as set, so code may fill in a group the config file never mentioned
    if (!wasParsed_ && !wasPatched()) {
        return required_? ValidationResult::failure("{}: is missing", niceName()) : ValidationResult::success();
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
}

bool ConfigGroup::wasPatched() const noexcept
{
    return std::any_of(entries_.begin(), entries_.end(), [] (auto const* entry) { return entry->wasPatched(); });
}

std::string ConfigGroup::niceName() const
{
    return displayName(name_, "<nameless group>");
}

} // namespace CARL
