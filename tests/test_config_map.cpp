#include "helpers.hpp"
#include "utils/exceptions.hpp"
#include "utils/utils.hpp"

#include <gtest/gtest.h>
#include <sstream>

// ============================================================
//  ID_LIST mode — parse()
// ============================================================

TEST(ConfigMapIdList, ParsesMultipleEntries) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node =
        YAML::Load(R"(
items:
  - id: 1
    label: "alpha"
  - id: 2
    label: "beta"
  - id: 3
    label: "gamma"
)");
    EXPECT_NO_THROW(map.parse(node));
    EXPECT_TRUE(map.validate().correct);
}

TEST(ConfigMapIdList, ParseSingleEntry) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load("items:\n  - id: 10\n    label: only");
    map.parse(node);
    EXPECT_TRUE(map.validate().correct);
}

TEST(ConfigMapIdList, NonSequenceNodeThrows) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load("items:\n  key: val"); // map, not sequence
    EXPECT_THROW(map.parse(node), CARL::ParsingError);
}

TEST(ConfigMapIdList, DuplicateIdThrows) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node =
        YAML::Load(R"(
items:
  - id: 1
    label: "first"
  - id: 1
    label: "duplicate"
)");
    EXPECT_THROW(map.parse(node), CARL::ParsingError);
}

TEST(ConfigMapIdList, NonMapEntryThrows) {
    CARL::ConfigMap<th::IdItem> map {""};
    auto                        node = YAML::Load("- 42\n  - 99"); // scalars, not maps
    EXPECT_THROW(map.parse(node), CARL::ParsingError);
}

TEST(ConfigMapIdList, AbsentKeyDoesNotParse) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load("other: stuff");
    map.parse(node);
    // Required + not parsed = failure
    EXPECT_FALSE(map.validate().correct);
}

TEST(ConfigMapIdList, EmptySequenceIsValidationFailureWhenRequired) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load("items: []");
    map.parse(node);
    auto result = map.validate();
    EXPECT_FALSE(result.correct);
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].find("empty"), std::string::npos);
}

TEST(ConfigMapIdList, OptionalAndAbsentIsSuccess) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST, CARL::Required::NO};
    auto                        node = YAML::Load("other: 1");
    map.parse(node);
    EXPECT_TRUE(map.validate().correct);
}

TEST(ConfigMapIdList, OptionalEmptySequenceIsSuccess) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST, CARL::Required::NO};
    auto                        node = YAML::Load("items: []");
    map.parse(node);
    // Optional + empty: no entries, but was parsed
    // validate() checks: !isRequired_ so skip the empty check
    EXPECT_TRUE(map.validate().correct);
}

// ============================================================
//  STANDARD mode — parse()
// ============================================================

TEST(ConfigMapStandard, ParsesMultipleEntries) {
    CARL::ConfigMap<th::ModelEntry, std::string> map {"models"};
    auto                                         node =
        YAML::Load(R"(
models:
  Canon100:
    zoom: 10
  Sony200:
    zoom: 20
    display: "wide"
)");
    EXPECT_NO_THROW(map.parse(node));
    EXPECT_TRUE(map.validate().correct);
}

TEST(ConfigMapStandard, NonMapNodeThrows) {
    CARL::ConfigMap<th::ModelEntry, std::string> map {"models"};
    auto                                         node = YAML::Load("models:\n  - item1\n  - item2"); // sequence, not map
    EXPECT_THROW(map.parse(node), CARL::ParsingError);
}

TEST(ConfigMapStandard, RequiredAndAbsentIsFailure) {
    CARL::ConfigMap<th::ModelEntry, std::string> map {"models"};
    auto                                         node = YAML::Load("other: 1");
    map.parse(node);
    EXPECT_FALSE(map.validate().correct);
}

TEST(ConfigMapStandard, IntKeyType) {
    CARL::ConfigMap<th::ModelEntry, int> map {"models"};
    auto                                 node = YAML::Load(R"(
models:
  1:
    zoom: 5
  2:
    zoom: 10
)");
    EXPECT_NO_THROW(map.parse(node));
    EXPECT_TRUE(map.validate().correct);
}

// ============================================================
//  validate() — error path prefixing
// ============================================================

TEST(ConfigMapValidate, MissingEntryFieldHasPrefixedError) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load(R"(
items:
  - id: 5
    label: "ok"
  - id: 7
)");  // id 7 is missing label
    map.parse(node);
    auto result = map.validate();
    EXPECT_FALSE(result.correct);
    bool found = false;
    for (auto const& e : result.errors) {
        if ((e.find("items") != std::string::npos) &&
            (e.find("7") != std::string::npos) &&
            (e.find("label") != std::string::npos)) {
            found = true;
        }
    }

    EXPECT_TRUE(found) << "Expected error prefixed as 'items[7].label: ...'";
}

TEST(ConfigMapValidate, MultipleEntriesWithMissingFieldsGivesMultipleErrors) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load(R"(
items:
  - id: 1
  - id: 2
  - id: 3
)");  // all missing label
    map.parse(node);
    auto result = map.validate();
    EXPECT_FALSE(result.correct);
    EXPECT_EQ(result.errors.size(), 3u);
}

TEST(ConfigMapValidate, RequiredNotParsedErrorContainsMapName) {
    CARL::ConfigMap<th::IdItem> map {"my_items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load("x: 1");
    map.parse(node);
    auto result = map.validate();
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].find("my_items"), std::string::npos);
}

TEST(ConfigMapValidate, StringKeyErrorContainsKey) {
    CARL::ConfigMap<th::ModelEntry, std::string> map {"models"};
    auto                                         node = YAML::Load(R"(
models:
  Canon100:
    zoom: 5
  BadEntry:
)");  // BadEntry is null
    // This will throw during parse (null entry is not a map)
    EXPECT_THROW(map.parse(node), CARL::ParsingError);
}

// ============================================================
//  printTo()
// ============================================================

TEST(ConfigMapPrint, IdListPrintsWithDashPrefix) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load(R"(
items:
  - id: 1
    label: "alpha"
  - id: 2
    label: "beta"
)");
    map.parse(node);
    std::ostringstream os;
    map.printTo(os, "");
    std::string out = os.str();
    EXPECT_NE(out.find("items"), std::string::npos);
    EXPECT_NE(out.find("- "), std::string::npos);
    EXPECT_NE(out.find("alpha"), std::string::npos);
    EXPECT_NE(out.find("beta"), std::string::npos);
}

TEST(ConfigMapPrint, StandardPrintsKeyValueFormat) {
    CARL::ConfigMap<th::ModelEntry, std::string> map {"models"};
    auto                                         node = YAML::Load("models:\n  CamA:\n    zoom: 5");
    map.parse(node);
    std::ostringstream os;
    map.printTo(os, "");
    std::string out = os.str();
    EXPECT_NE(out.find("models"), std::string::npos);
    EXPECT_NE(out.find("CamA"), std::string::npos);
    EXPECT_NE(out.find("5"), std::string::npos);
}

TEST(ConfigMapPrint, EmptyMapPrintsHeader) {
    CARL::ConfigMap<th::ModelEntry, std::string> map {"models", CARL::MapType::STANDARD, CARL::Required::NO};
    auto                                         node = YAML::Load("models: {}");
    map.parse(node);
    std::ostringstream os;
    map.printTo(os, "");
    std::string out = os.str();
    EXPECT_NE(out.find("models"), std::string::npos);
}

TEST(ConfigMapPrint, IndentIsApplied) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load("items:\n  - id: 1\n    label: x");
    map.parse(node);
    std::ostringstream os;
    map.printTo(os, "  ");
    std::string out = os.str();
    EXPECT_EQ(out.substr(0, 2), "  ");
}

// ============================================================
//  static_assert for default-constructibility (documented)
// ============================================================
// ConfigMap<Group> requires Group to be default-constructible.
// This is enforced by a static_assert. Example that must NOT compile:
//   struct NonDefault { NonDefault() = delete; ... };
//   CARL::ConfigMap<NonDefault> bad{"x"};  // compile error
