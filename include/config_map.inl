namespace CARL
{

template <typename Group, typename KeyType>
void ConfigMap<Group, KeyType>::parse(YAML::Node const& node)
{
    if (!name_.empty() && !isMapOrNull(node)) {
        throw ParsingError("expected a map node while looking for '{}'", this->niceName());
    }

    // Allow for same-level maps (e.g., global entries without a wrapping name)
    YAML::Node const map_node = name_.empty()? node : node[name_];
    if (!map_node.IsDefined()) {
        return;
    }

    // a declared but empty section reads as a null node, the same way ConfigGroup treats it
    if (map_node.IsNull()) {
        wasParsed_ = true;
        return;
    }

    if ((mapType_ == MapType::ID_LIST) && !map_node.IsSequence()) {
        throw ParsingError("'{}' node must be a YAML sequence type", this->niceName());
    }
    else if ((mapType_ == MapType::STANDARD) && !map_node.IsMap()) {
        throw ParsingError("'{}' node must be a YAML map type", this->niceName());
    }

    // Duplicates are rejected within one document only. Across parse() calls the entries are overlaid instead, so a
    // later config file can override fields of an entry an earlier one introduced.
    std::set<KeyType> seen_in_this_pass;

    for (auto const& entry : map_node) {
        // resolve map/list traversal duality. Const, so a missing id is looked up instead of placeheld.
        YAML::Node const value = (mapType_ == MapType::ID_LIST)? YAML::Node(entry) : entry.second;

        if (!value.IsMap()) {
            throw ParsingError("each entry in '{}' must be a map type", this->niceName());
        }

        // read the id from the entry itself, only safe once the entry is known to be a map
        YAML::Node const key = (mapType_ == MapType::ID_LIST)? value["id"] : entry.first;
        KeyType          id  = parseKey(key);

        if (!seen_in_this_pass.insert(id).second) {
            throw ParsingError("duplicate id '{}' in '{}'", id, this->niceName());
        }

        auto existing = entries_.find(id);
        if (existing != entries_.end()) {
            existing->second->parse(value);
            continue;
        }

        // parse before storing, so an entry that throws leaves no half-built one behind
        auto fresh = std::make_unique<Group>();
        fresh->parse(value);
        entries_.emplace(std::move(id), std::move(fresh));
    }

    wasParsed_ = true;
}

template <typename Group, typename KeyType>
KeyType ConfigMap<Group, KeyType>::parseKey(YAML::Node const& key_node) const
{
    try {
        return key_node.as<KeyType>();
    }
    catch (YAML::Exception const& e) {
        throw ParsingError("'{}' failed to parse id: {}", this->niceName(), e.what());
    }
}

template <typename Group, typename KeyType>
Group& ConfigMap<Group, KeyType>::at(KeyType const& key)
{
    return const_cast<Group&>(static_cast<ConfigMap const&>(*this).at(key));
}

template <typename Group, typename KeyType>
Group const& ConfigMap<Group, KeyType>::at(KeyType const& key) const
{
    if (Group const* entry = find(key)) {
        return *entry;
    }

    throw LookupError("'{}' has no entry '{}'", this->niceName(), key);
}

template <typename Group, typename KeyType>
Group* ConfigMap<Group, KeyType>::find(KeyType const& key)
{
    return const_cast<Group*>(static_cast<ConfigMap const&>(*this).find(key));
}

template <typename Group, typename KeyType>
Group const* ConfigMap<Group, KeyType>::find(KeyType const& key) const
{
    auto entry = entries_.find(key);
    return (entry == entries_.end())? nullptr : entry->second.get();
}

template <typename Group, typename KeyType>
bool ConfigMap<Group, KeyType>::wasPatched() const noexcept
{
    return std::any_of(
        entries_.begin(),
        entries_.end(),
        [] (auto const& entry) { return entry.second->wasPatched(); }
    );
}

template <typename Group, typename KeyType>
ValidationResult ConfigMap<Group, KeyType>::validate() const
{
    if (isRequired_ && !wasParsed_) {
        return ValidationResult::failure("{}: is missing", this->niceName());
    }

    if (isRequired_ && entries_.empty()) {
        return ValidationResult::failure("{}: is empty", this->niceName());
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

    if (mapType_ == MapType::STANDARD) {
        std::string const sub_indent = indent + std::string(name_.empty()? 0 : 2, ' ');
        for (auto const& [id, group] : entries_) {
            os << sub_indent << id << ":\n";
            group->printTo(os, sub_indent + "  ");
        }

        return;
    }

    // workaround for prettier printing: hoist the first rendered line onto the "- " bullet. The bullet occupies the
    // first two columns of the entry, so the body of every entry sits two deeper than the bullet, named or not.
    std::string const sub_indent = indent + "  ";
    for (auto const& [_, group] : entries_) {
        std::ostringstream temp;
        group->printTo(temp, sub_indent);
        std::string group_str = temp.str();

        auto        newline    = group_str.find('\n');
        std::string first_line = group_str.substr(0, newline);
        std::string rest       = (newline != std::string::npos)? group_str.substr(newline + 1) : "";

        auto non_space = first_line.find_first_not_of(' ');
        first_line = (non_space == std::string::npos)? "" : first_line.substr(non_space);

        os << indent << "- " << first_line << '\n' << rest;
    }
}

} // namespace CARL
