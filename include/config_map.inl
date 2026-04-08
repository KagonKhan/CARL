namespace CARL
{

template <typename Group, typename KeyType>
void ConfigMap<Group, KeyType>::parse(YAML::Node const& node)
{
    // Allow for same-level maps (e.g., global entries without a wrapping name)
    YAML::Node map_node = name_.empty()? node : node[name_];

    if ((mapType_ == MapType::ID_LIST) && !map_node.IsSequence()) {
        throw ParsingError("'{}' node must be a YAML sequence type", this->niceName());
    }
    else if ((mapType_ == MapType::STANDARD) && !map_node.IsMap()) {
        throw ParsingError("'{}' node must be a YAML map type", this->niceName());
    }

    for (auto const& entry : map_node) {
        // resolve map/list duality
        YAML::Node key   = (mapType_ == MapType::ID_LIST)? entry["id"] : entry.first;
        YAML::Node value = (mapType_ == MapType::ID_LIST)? YAML::Node(entry) : entry.second;

        if (!value.IsMap()) {
            throw ParsingError("each entry in {} must be a map type", this->niceName());
        }

        KeyType id;
        try {
            id = key.as<KeyType>();

            if (entries_.count(id) > 0) {
                throw ParsingError("duplicate id '{}' in '{}'", id, this->niceName());
            }

            auto group = std::make_unique<Group>();
            group->parse(value);
            entries_.emplace(std::move(id), std::move(group));
        }
        catch (YAML::Exception const& e) {
            ParsingError("'{}' failed to parse id: {}", niceName(), e.what());
        }
    }


    wasParsed_ = true;
}

template <typename Group, typename KeyType>
ValidationResult ConfigMap<Group, KeyType>::validate() const
{
    if (isRequired_ && !wasParsed_) {
        return ValidationResult::failure("'{}' is required and missing", this->niceName());
    }

    if (isRequired_ && entries_.empty()) {
        return ValidationResult::failure("'{}' is required and empty", this->niceName());
    }

    ValidationResult result = ValidationResult::success();
    for (auto const& [id, group] : entries_) {
        auto entry_result = group->validate();

        for (auto& subentry : entry_result.errors) {
            subentry = fmt::format("{}[{}].{}", this->niceName(), id, subentry);
        }

        result.merge(entry_result);
    }

    return result;
}

template <typename Group, typename KeyType>
void ConfigMap<Group, KeyType>::printTo(std::ostream& os, std::string const& indent) const
{
    if (!name_.empty()) {
        os << indent << name_ << ":\n";
    }

    std::string sub_indent = indent + std::string(name_.empty()? 0 : 2, ' ');
    if (mapType_ == MapType::STANDARD) {
        for (auto const& [id, group] : entries_) {
            os << sub_indent << id << ": \n";
            group->printTo(os, sub_indent + "  ");
        }
    }
    else {
        // workaround for prettier printing
        for (auto const& [_, group] : entries_) {
            std::ostringstream temp;
            group->printTo(temp, sub_indent + "  ");
            std::string groupStr = temp.str();

            auto newline   = groupStr.find('\n');
            auto firstLine = groupStr.substr(0, newline);
            auto rest      = (newline != std::string::npos)? groupStr.substr(newline + 1) : "";

            auto nonSpace = firstLine.find_first_not_of(' ');
            os << sub_indent << "- " << firstLine.substr(nonSpace) << '\n';
            os << rest;
        }
    }
}

} // namespace CARL
