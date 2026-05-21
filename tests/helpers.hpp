#pragma once

#include "config_group.hpp"
#include "config_map.hpp"
#include "config_value.hpp"
#include "utils/utils.hpp"

namespace th
{

// Minimal nameless group — parses directly from the current node
struct PairGroup : CARL::ConfigGroup
{
    CARL::ConfigValue<int> x {"x"};
    CARL::ConfigValue<int> y {"y"};
    PairGroup() { registerEntries(x, y); }
};

// Named group wrapping two ints
struct NamedPairGroup : CARL::ConfigGroup
{
    CARL::ConfigValue<int> x {"x"};
    CARL::ConfigValue<int> y {"y"};
    explicit NamedPairGroup(std::string name = "pair")
        : CARL::ConfigGroup(std::move(name)) { registerEntries(x, y); }
};

// Named group with optional field
struct PersonGroup : CARL::ConfigGroup
{
    CARL::ConfigValue<std::string> name {"name"};
    CARL::ConfigValue<int> age {"age"};
    CARL::ConfigValue<std::string> email {"email", CARL::Required::NO};
    PersonGroup()
        : CARL::ConfigGroup("person") { registerEntries(name, age, email); }
};

// Optional named group
struct OptionalSection : CARL::ConfigGroup
{
    CARL::ConfigValue<int> value {"value"};
    OptionalSection()
        : CARL::ConfigGroup("optional_section", CARL::Required::NO) { registerEntries(value); }
};

// Nameless group with id field first — for ConfigMap ID_LIST
struct IdItem : CARL::ConfigGroup
{
    CARL::ConfigValue<int> id {"id"};
    CARL::ConfigValue<std::string> label {"label"};
    IdItem() { registerEntries(id, label); }
};

// Nameless group for ConfigMap STANDARD (string-keyed)
struct ModelEntry : CARL::ConfigGroup
{
    CARL::ConfigValue<int> zoom {"zoom"};
    CARL::ConfigValue<std::string> display {"display", CARL::Required::NO};
    ModelEntry() { registerEntries(zoom, display); }
};

// Nested named groups
struct InnerGroup : CARL::ConfigGroup
{
    CARL::ConfigValue<int> val {"val"};
    InnerGroup()
        : CARL::ConfigGroup("inner") { registerEntries(val); }
};

struct OuterGroup : CARL::ConfigGroup
{
    InnerGroup inner;
    CARL::ConfigValue<int> x {"x"};
    OuterGroup()
        : CARL::ConfigGroup("outer") { registerEntries(inner, x); }
};

// Group that has both required and default-value fields
struct ConfigWithDefaults : CARL::ConfigGroup
{
    CARL::ConfigValue<int> required_field {"required_field"};
    CARL::ConfigValue<int> optional_field {"optional_field", CARL::Default<int>{42}};
    CARL::ConfigValue<std::string> optional_str {"optional_str", CARL::Default<std::string>("hello")};
    ConfigWithDefaults()
        : CARL::ConfigGroup("config") { registerEntries(required_field, optional_field, optional_str); }
};

#ifndef NDEBUG
// id NOT first — only used in death tests
struct BadIdOrderGroup : CARL::ConfigGroup
{
    CARL::ConfigValue<int> other {"other"};
    CARL::ConfigValue<int> id {"id"};
    BadIdOrderGroup() { registerEntries(other, id); }
};

#endif

} // namespace th
