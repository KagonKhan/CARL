#ifndef CARL_CONFIG_MAP_HPP
#define CARL_CONFIG_MAP_HPP

#include "iconfig_value.hpp"
#include "utils/config_exception.hpp"

#include <iostream>
#include <map>
#include <vector>

namespace CARL
{

template <typename Group, typename KeyType = int>
class ConfigMap : public IConfigValue
{
public:
    // static asserts


public:
    explicit constexpr ConfigMap(std::string name, Required required = Required::YES)
        : name_(std::move(name)),
          required_(required)
    {}

    void parse(YAML::Node const& node) override
    {
        if (!node.IsMap() || !node[name_]) {
            return;
        }

        if (!node[name_].IsSequence()) {
            throw ParsingError("{} must be a sequence type", name_);
        }

        for (auto const& entry : node[name_]) {
            if (!entry.IsMap()) {
                throw ParsingError("each entry in {} must be a map type", name_);
            }

            if (!entry["id"]) {
                throw ParsingError("entry in {} is missing the 'id' field", name_);
            }

            KeyType id;
            try {
                id = entry["id"].as<KeyType>();
            }
            catch (YAML::Exception const& e) {
                throw ParsingError("could not parse 'id' in '{}: {}", name_, e.what());
            }

            if (entries_.count(id) > 0) {
                throw ParsingError("duplicate id '{}' in '{}'", id, name_);
            }

            auto group = std::make_unique<Group>();
            group->parse(entry);
            std::cout << "group before: \n";
            group->printTo(std::cout);
            entries_.emplace(std::move(id), std::move(group));

            std::cout << "\ngroup after: \n";
            entries_[id]->printTo(std::cout);

            std::cout << "\n\n-----------------\n\n";
        }
    }

    [[nodiscard]] ValidationResult validate() const override { return ValidationResult::success(); }

    void printTo(std::ostream& os, std::string_view indent = "") const override
    {
        os << indent << name_ << ":\n";
        std::string child_indent = std::string(indent) + "  ";

        for (auto const& [id, group] : entries_) {
            os << "- id: " << id << '\n';
            group->printTo(os, child_indent);
        }
    }

private:
    std::string name_;
    Required    required_;

    std::map<KeyType, std::unique_ptr<Group>> entries_;
    bool                                      wasParsed_ {false};
};

} // namespace CARL


#endif // CARL_CONFIG_MAP_HPP
