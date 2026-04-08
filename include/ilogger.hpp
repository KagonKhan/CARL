namespace CARL
{

template <typename Group, typename KeyType>
void ConfigMap<Group, KeyType>::parse(YAML::Node const& node)
{
    // Allow for same-level maps (e.g., global entries without a wrapping name)
    YAML::Node map_node = name_.empty()? node : node[name_];

    for (auto const& entry : map_node) {
        if (!entry.IsMap()) {
            throw ParsingError("each entry in {} must be a map type", name_);
        }

        KeyType id;

        try {
            if (entry["id"]) {
                id = entry["id"].as<KeyType>();
            }
            else {
                id = entry.first.as<KeyType>();
            }
        }
        catch (YAML::Exception const& e) {
            throw ParsingError("could not parse 'id' in '{}: {}", name_, e.what());
        }


        if (entries_.count(id) > 0) {
            throw ParsingError("duplicate id '{}' in '{}'", id, name_);
        }

        auto group = std::make_unique<Group>();
        group->parse(entry);
        entries_.emplace(std::move(id), std::move(group));
    }

    wasParsed_ = true;
}

template <typename Group, typename KeyType>
ValidationResult ConfigMap<Group, KeyType>::validate() const
{
    if (required_ && !wasParsed_) {
        return ValidationResult::failure("'{}' is required and missing", name_);
    }

    if (required_ && entries_.empty()) {
        return ValidationResult::failure("'{}' is empty", name_);
    }

    ValidationResult result = ValidationResult::success();
    for (auto const& [id, group] : entries_) {
        auto entry_result = group->validate();

        for (auto& subentry : entry_result.errors) {
            if (!name_.empty()) {
                subentry = fmt::format("{}[{}]: {}", name_, id, subentry);
            }

            result.merge(entry_result);
        }
    }

    return result;
}

template <typename Group, typename KeyType>
void ConfigMap<Group, KeyType>::printTo(std::ostream& os, std::string const& indent) const
{
    if (!name_.empty()) {
        os << indent << name_ << ":\n";
    }

    std::string child_indent = std::string(indent) + "  ";

    for (auto const& [id, group] : entries_) {
        // TODO: check what type of map this is, is it ID map, or normal map
        os << indent << "- id: " << id << '\n';
        group->printTo(os, child_indent);
    }
}

} // namespace CARL
