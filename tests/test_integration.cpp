#include "helpers.hpp"
#include "utils/exceptions.hpp"

#include <gtest/gtest.h>
#include <random>
#include <set>
#include <sstream>

// ============================================================
//  Full tree — flat group
// ============================================================

TEST(Integration, FlatGroupValidYaml)
{
    th::PersonGroup g;
    auto node = YAML::Load(R"(
person:
  name: Alice
  age: 30
  email: alice@example.com
)");
    g.parse(node);
    auto result = g.validate();
    EXPECT_TRUE(result.correct) << result.errors[0];
    EXPECT_EQ(*g.name,  "Alice");
    EXPECT_EQ(*g.age,   30);
    EXPECT_EQ(*g.email, "alice@example.com");
}

TEST(Integration, FlatGroupOptionalFieldAbsent)
{
    th::PersonGroup g;
    auto node = YAML::Load("person:\n  name: Bob\n  age: 25");
    g.parse(node);
    EXPECT_TRUE(g.validate().correct);
    EXPECT_EQ(*g.name, "Bob");
    EXPECT_EQ(*g.age,  25);
}

TEST(Integration, FlatGroupRequiredFieldMissing)
{
    th::PersonGroup g;
    auto node = YAML::Load("person:\n  name: Charlie");  // age missing
    g.parse(node);
    auto result = g.validate();
    EXPECT_FALSE(result.correct);
    EXPECT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].find("age"), std::string::npos);
}

TEST(Integration, FlatGroupAllFieldsMissing)
{
    th::PersonGroup g;
    auto node = YAML::Load("person: {}");
    g.parse(node);
    auto result = g.validate();
    EXPECT_FALSE(result.correct);
    EXPECT_GE(result.errors.size(), 2u);  // name and age
}

// ============================================================
//  Full tree — nested groups
// ============================================================

TEST(Integration, NestedGroupAllPresent)
{
    th::OuterGroup g;
    auto node = YAML::Load(R"(
outer:
  x: 7
  inner:
    val: 99
)");
    g.parse(node);
    EXPECT_TRUE(g.validate().correct);
    EXPECT_EQ(*g.x,         7);
    EXPECT_EQ(*g.inner.val, 99);
}

TEST(Integration, NestedGroupInnerMissing)
{
    th::OuterGroup g;
    auto node = YAML::Load("outer:\n  x: 1");
    g.parse(node);
    auto result = g.validate();
    EXPECT_FALSE(result.correct);
    bool inner_val_error = false;
    for (auto const& e : result.errors) {
        if (e.find("inner") != std::string::npos && e.find("val") != std::string::npos)
            inner_val_error = true;
    }
    EXPECT_TRUE(inner_val_error);
}

TEST(Integration, OuterGroupAbsentGivesMultipleErrors)
{
    th::OuterGroup g;
    auto node = YAML::Load("unrelated: 1");
    g.parse(node);
    auto result = g.validate();
    EXPECT_FALSE(result.correct);
    EXPECT_GE(result.errors.size(), 2u);
}

// ============================================================
//  Full tree — ConfigMap with nested groups
// ============================================================

TEST(Integration, ConfigMapIdListFullValidation)
{
    CARL::ConfigMap<th::IdItem> map {"entries", CARL::MapType::ID_LIST};
    auto node = YAML::Load(R"(
entries:
  - id: 10
    label: ten
  - id: 20
    label: twenty
  - id: 30
    label: thirty
)");
    map.parse(node);
    EXPECT_TRUE(map.validate().correct);
}

TEST(Integration, ConfigMapIdListWithMissingSubFields)
{
    CARL::ConfigMap<th::IdItem> map {"entries", CARL::MapType::ID_LIST};
    auto node = YAML::Load(R"(
entries:
  - id: 1
    label: ok
  - id: 2
)");  // id 2 missing label
    map.parse(node);
    auto result = map.validate();
    EXPECT_FALSE(result.correct);
    EXPECT_NE(result.errors[0].find("entries[2]"), std::string::npos);
}

TEST(Integration, ConfigMapStandardFullValidation)
{
    CARL::ConfigMap<th::ModelEntry, std::string> map {"cameras"};
    auto node = YAML::Load(R"(
cameras:
  CamA:
    zoom: 5
  CamB:
    zoom: 10
    display: widescreen
)");
    map.parse(node);
    EXPECT_TRUE(map.validate().correct);
}

// ============================================================
//  Defaults — integration
// ============================================================

TEST(Integration, DefaultsAppliedWhenFieldAbsent)
{
    th::ConfigWithDefaults g;
    auto node = YAML::Load("config:\n  required_field: 1");
    g.parse(node);
    EXPECT_TRUE(g.validate().correct);
    EXPECT_EQ(*g.required_field, 1);
    EXPECT_EQ(*g.optional_field, 42);
    EXPECT_EQ(*g.optional_str,   "hello");
}

TEST(Integration, DefaultsOverwrittenByParsedValues)
{
    th::ConfigWithDefaults g;
    auto node = YAML::Load(R"(
config:
  required_field: 7
  optional_field: 99
  optional_str: overwritten
)");
    g.parse(node);
    EXPECT_TRUE(g.validate().correct);
    EXPECT_EQ(*g.optional_field, 99);
    EXPECT_EQ(*g.optional_str,   "overwritten");
}

// ============================================================
//  Multi-file / overwrite semantics
// ============================================================

TEST(Integration, SecondParseOverwritesFirstForGroup)
{
    th::PersonGroup g;
    auto node1 = YAML::Load("person:\n  name: Alice\n  age: 30");
    auto node2 = YAML::Load("person:\n  name: Bob\n  age: 25");
    g.parse(node1);
    g.parse(node2);
    EXPECT_TRUE(g.validate().correct);
    EXPECT_EQ(*g.name, "Bob");
    EXPECT_EQ(*g.age,  25);
}

TEST(Integration, SecondParseWithPartialDataOverwritesOnlyPresentFields)
{
    th::PersonGroup g;
    auto node1 = YAML::Load("person:\n  name: Alice\n  age: 30");
    auto node2 = YAML::Load("person:\n  name: Alice-Updated");  // only name in second file
    g.parse(node1);
    g.parse(node2);
    EXPECT_EQ(*g.name, "Alice-Updated");
    EXPECT_EQ(*g.age,  30);  // from first parse, not overwritten
}

// ============================================================
//  Optional group — integration
// ============================================================

TEST(Integration, OptionalGroupAbsentDoesNotAffectValidation)
{
    // Top-level nameless group containing an optional section
    struct RootConfig : CARL::ConfigGroup
    {
        CARL::ConfigValue<int> required_val {"required_val"};
        th::OptionalSection    optional;
        RootConfig() { registerEntries(required_val, optional); }
    };

    RootConfig cfg;
    auto node = YAML::Load("required_val: 5");  // optional_section absent
    cfg.parse(node);
    EXPECT_TRUE(cfg.validate().correct);
    EXPECT_EQ(*cfg.required_val, 5);
}

TEST(Integration, OptionalGroupPresentAndValidated)
{
    struct RootConfig : CARL::ConfigGroup
    {
        CARL::ConfigValue<int> required_val {"required_val"};
        th::OptionalSection    optional;
        RootConfig() { registerEntries(required_val, optional); }
    };

    RootConfig cfg;
    auto node = YAML::Load("required_val: 5\noptional_section:\n  value: 42");
    cfg.parse(node);
    EXPECT_TRUE(cfg.validate().correct);
}

// ============================================================
//  printTo — full tree round-trip readability
// ============================================================

TEST(Integration, PrintFullGroupDoesNotCrash)
{
    th::PersonGroup g;
    auto node = YAML::Load("person:\n  name: Eve\n  age: 28");
    g.parse(node);
    std::ostringstream os;
    EXPECT_NO_THROW(g.printTo(os, ""));
    EXPECT_FALSE(os.str().empty());
}

TEST(Integration, PrintUnparsedGroupDoesNotCrash)
{
    th::PersonGroup g;
    std::ostringstream os;
    EXPECT_NO_THROW(g.printTo(os, ""));
    std::string out = os.str();
    EXPECT_NE(out.find("<missing>"), std::string::npos);
}

TEST(Integration, PrintMapDoesNotCrash)
{
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto node = YAML::Load("items:\n  - id: 1\n    label: x");
    map.parse(node);
    std::ostringstream os;
    EXPECT_NO_THROW(map.printTo(os, ""));
}

// ============================================================
//  Fuzz — random YAML trees
// ============================================================

TEST(IntegrationFuzz, RandomIntFieldsNeverCrash)
{
    std::mt19937 rng {42};
    std::uniform_int_distribution<int> int_dist {-100000, 100000};

    for (int i = 0; i < 300; ++i) {
        int x = int_dist(rng);
        int y = int_dist(rng);
        auto yaml_str = fmt::format("pair:\n  x: {}\n  y: {}", x, y);
        auto node     = YAML::Load(yaml_str);

        th::NamedPairGroup g;
        EXPECT_NO_THROW(g.parse(node));
        EXPECT_TRUE(g.validate().correct);
        EXPECT_EQ(*g.x, x);
        EXPECT_EQ(*g.y, y);
    }
}

TEST(IntegrationFuzz, RandomIdListNeverCrash)
{
    std::mt19937 rng {7};
    std::uniform_int_distribution<int> id_dist {1, 10000};

    for (int trial = 0; trial < 50; ++trial) {
        int count = std::uniform_int_distribution<int>{1, 20}(rng);
        std::string yaml = "items:\n";
        std::set<int> used;
        bool has_duplicate = false;

        for (int j = 0; j < count; ++j) {
            int id = id_dist(rng);
            if (used.count(id)) { has_duplicate = true; }
            used.insert(id);
            yaml += fmt::format("  - id: {}\n    label: item{}\n", id, j);
        }

        CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
        auto node = YAML::Load(yaml);

        if (has_duplicate) {
            EXPECT_THROW(map.parse(node), CARL::ParsingError);
        } else {
            EXPECT_NO_THROW(map.parse(node));
            EXPECT_TRUE(map.validate().correct);
        }
    }
}

TEST(IntegrationFuzz, GarbageYamlStringsNeverCrash)
{
    // Feed completely unexpected YAML strings through a group's parse.
    // The contract: either succeeds silently (key absent) or throws
    // ParsingError / YAML::Exception — never crashes or throws unexpected types.
    static const std::vector<std::string> garbage {
        "",
        "null",
        "42",
        "true",
        "[]",
        "{}",
        "key: ~",
        "key: !!binary YWJj",
        "pair:\n  x: [1,2]\n  y: hello",  // wrong types for int fields
        "pair:\n  x: null\n  y: 0",
        "pair: [1, 2]",                    // list instead of map
        "\xC0\x80",                        // invalid UTF-8
    };

    for (auto const& input : garbage) {
        th::NamedPairGroup g;
        try {
            auto node = YAML::Load(input);
            g.parse(node);
        }
        catch (CARL::ParsingError const&) { /* expected */ }
        catch (YAML::Exception const&)    { /* expected */ }
        // No other exception type should escape
    }
}

TEST(IntegrationFuzz, ExtraUnknownFieldsAreIgnored)
{
    // CARL only reads what it knows about — extra fields should be silently skipped
    th::PersonGroup g;
    auto node = YAML::Load(R"(
person:
  name: Alice
  age: 30
  unknown_field_1: some_value
  unknown_field_2: 12345
  deeply:
    nested: stuff
)");
    EXPECT_NO_THROW(g.parse(node));
    EXPECT_TRUE(g.validate().correct);
    EXPECT_EQ(*g.name, "Alice");
}

TEST(IntegrationFuzz, LargeNumberOfEntries)
{
    constexpr int N = 500;
    std::string yaml = "items:\n";
    for (int i = 1; i <= N; ++i) {
        yaml += fmt::format("  - id: {}\n    label: item{}\n", i, i);
    }

    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto node = YAML::Load(yaml);
    EXPECT_NO_THROW(map.parse(node));
    EXPECT_TRUE(map.validate().correct);
}
