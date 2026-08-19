#include "helpers.hpp"
#include "utils/exceptions.hpp"
#include "utils/utils.hpp"

#include <gtest/gtest.h>
#include <cstddef>
#include <iterator>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

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

TEST(ConfigMapIdList, ScalarEntryThrowsParsingError) {
    // Sequence of scalars where mappings are expected. Reading "id" off a scalar makes yaml-cpp throw BadSubscript,
    // so the entry has to be type-checked before the key is read.
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load("items:\n  - 42\n  - 99");
    EXPECT_THROW(map.parse(node), CARL::ParsingError);
}

TEST(ConfigMapIdList, EntryWithoutIdThrowsParsingError) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load("items:\n  - label: no_id_here");
    EXPECT_THROW(map.parse(node), CARL::ParsingError);
}

TEST(ConfigMapIdList, EntryThatFailsToParseIsNotStored) {
    // The entry is emplaced only once it parsed, so a caller that catches the error sees no half-built entry.
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load("items:\n  - id: 7\n    label: [not, a, scalar]");
    EXPECT_THROW(map.parse(node), CARL::ParsingError);
    EXPECT_EQ(map.size(), 0u);
    EXPECT_FALSE(map.contains(7));
}

TEST(ConfigMapIdList, EntryWithoutIdLeavesTheNodeUntouched) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load("items:\n  - label: no_id_here");
    EXPECT_THROW(map.parse(node), CARL::ParsingError);

    YAML::Node const& read_only = node;
    EXPECT_EQ(read_only["items"][0].size(), 1u);
    EXPECT_FALSE(read_only["items"][0]["id"].IsDefined());
}

TEST(ConfigMapIdList, NullNodeIsAnEmptyCollection) {
    // "items:" with nothing under it is a declared but empty section, the same shape a group accepts
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    EXPECT_NO_THROW(map.parse(YAML::Load("items:")));
    auto result = map.validate();
    EXPECT_FALSE(result.correct);
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0], "items: is empty");
}

TEST(ConfigMapIdList, OptionalNullNodeIsSuccess) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST, CARL::Required::NO};
    EXPECT_NO_THROW(map.parse(YAML::Load("items:")));
    EXPECT_TRUE(map.validate().correct);
    EXPECT_TRUE(map.empty());
}

TEST(ConfigMapIdList, NamelessMapParsesFromCurrentNode) {
    CARL::ConfigMap<th::IdItem> map {"", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load("- id: 1\n  label: a\n- id: 2\n  label: b");
    EXPECT_NO_THROW(map.parse(node));
    EXPECT_EQ(map.size(), 2u);
}

TEST(ConfigMapIdList, RegistrationOrderOfIdDoesNotMatter) {
    CARL::ConfigMap<th::IdLastItem> map {"items", CARL::MapType::ID_LIST};
    auto                            node = YAML::Load("items:\n  - id: 7\n    label: seven");
    EXPECT_NO_THROW(map.parse(node));
    EXPECT_TRUE(map.validate().correct);
    ASSERT_TRUE(map.contains(7));
    EXPECT_EQ(*map.at(7).label, "seven");
}

TEST(ConfigMapIdList, ScalarParentNodeThrowsParsingError) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load("just_a_scalar");
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

TEST(ConfigMapStandard, NullNodeIsAnEmptyCollection) {
    CARL::ConfigMap<th::ModelEntry, std::string> map {"models", CARL::MapType::STANDARD, CARL::Required::NO};
    EXPECT_NO_THROW(map.parse(YAML::Load("models:")));
    EXPECT_TRUE(map.validate().correct);
    EXPECT_TRUE(map.empty());
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

TEST(ConfigMapValidate, MissingSectionUsesTheSameWordingAsAGroup) {
    CARL::ConfigMap<th::IdItem> map {"cameras", CARL::MapType::ID_LIST};
    map.parse(YAML::Load("x: 1"));
    auto result = map.validate();
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0], "cameras: is missing");
}

TEST(ConfigMapValidate, NullEntryThrows) {
    CARL::ConfigMap<th::ModelEntry, std::string> map {"models"};
    auto                                         node = YAML::Load(R"(
models:
  Canon100:
    zoom: 5
  BadEntry:
)");  // BadEntry is null, and null is not a map
    EXPECT_THROW(map.parse(node), CARL::ParsingError);
}

TEST(ConfigMapValidate, StringKeyErrorContainsKey) {
    CARL::ConfigMap<th::ModelEntry, std::string> map {"models"};
    auto                                         node =
        YAML::Load(R"(
models:
  Canon100:
    zoom: 5
  Sony200:
    display: "no zoom here"
)");  // Sony200 is missing the required zoom
    map.parse(node);
    auto result = map.validate();
    EXPECT_FALSE(result.correct);
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_EQ(result.errors[0], "models[Sony200].zoom: is missing");
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

TEST(ConfigMapPrint, NamelessIdListPrintsReloadableYaml) {
    // "- " occupies the first two columns, so every line of the entry has to sit two deeper than the bullet
    CARL::ConfigMap<th::IdItem> map {"", CARL::MapType::ID_LIST};
    map.parse(YAML::Load("- id: 1\n  label: a\n- id: 2\n  label: b"));
    std::ostringstream os;
    map.printTo(os, "");

    auto reloaded = YAML::Load(os.str());
    ASSERT_TRUE(reloaded.IsSequence());
    ASSERT_EQ(reloaded.size(), 2u);
    EXPECT_EQ(reloaded[1]["label"].as<std::string>(), "b");
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
//  read API
// ============================================================

TEST(ConfigMapAccess, AtReturnsParsedEntry) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load(R"(
items:
  - id: 1
    label: "alpha"
  - id: 2
    label: "beta"
)");
    map.parse(node);
    EXPECT_EQ(*map.at(1).label, "alpha");
    EXPECT_EQ(*map.at(2).label, "beta");
    EXPECT_EQ(*map.at(2).id, 2);
}

TEST(ConfigMapAccess, AtOnConstMapReturnsConstEntry) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load("items:\n  - id: 4\n    label: four");
    map.parse(node);

    auto const& const_map = map;
    EXPECT_EQ(*const_map.at(4).label, "four");
    static_assert(std::is_const_v<std::remove_reference_t<decltype(const_map.at(4))>>);
}

TEST(ConfigMapAccess, AtThrowsLookupErrorForMissingKey) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load("items:\n  - id: 1\n    label: a");
    map.parse(node);
    EXPECT_THROW((void)map.at(99), CARL::LookupError);

    try {
        (void)map.at(99);
        FAIL() << "Expected LookupError";
    }
    catch (CARL::LookupError const& e) {
        std::string const what {e.what()};
        EXPECT_NE(what.find("items"), std::string::npos);
        EXPECT_NE(what.find("99"), std::string::npos);
    }
}

TEST(ConfigMapAccess, FindReturnsNullptrForMissingKey) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node = YAML::Load("items:\n  - id: 1\n    label: a");
    map.parse(node);

    auto* present = map.find(1);
    ASSERT_NE(present, nullptr);
    EXPECT_EQ(*present->label, "a");
    EXPECT_EQ(map.find(2), nullptr);

    auto const& const_map = map;
    EXPECT_NE(const_map.find(1), nullptr);
    EXPECT_EQ(const_map.find(2), nullptr);
}

TEST(ConfigMapAccess, ContainsSizeAndEmpty) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST, CARL::Required::NO};
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0u);
    EXPECT_FALSE(map.contains(1));

    auto node = YAML::Load("items:\n  - id: 1\n    label: a\n  - id: 5\n    label: b");
    map.parse(node);
    EXPECT_FALSE(map.empty());
    EXPECT_EQ(map.size(), 2u);
    EXPECT_TRUE(map.contains(1));
    EXPECT_TRUE(map.contains(5));
    EXPECT_FALSE(map.contains(2));
}

TEST(ConfigMapAccess, IterationYieldsKeysAndGroupsInKeyOrder) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        node =
        YAML::Load(R"(
items:
  - id: 30
    label: c
  - id: 10
    label: a
  - id: 20
    label: b
)");
    map.parse(node);

    std::vector<int>         keys;
    std::vector<std::string> labels;
    for (auto const& [id, item] : map) {
        keys.push_back(id);
        labels.push_back(*item.label);
    }

    EXPECT_EQ(keys, (std::vector<int> {10, 20, 30}));
    EXPECT_EQ(labels, (std::vector<std::string> {"a", "b", "c"}));
}

TEST(ConfigMapAccess, IterationOverConstMapWorks) {
    CARL::ConfigMap<th::ModelEntry, std::string> map {"models"};
    auto                                         node = YAML::Load("models:\n  CamA:\n    zoom: 5");
    map.parse(node);

    auto const& const_map = map;
    std::size_t seen      = 0;
    for (auto const& [key, entry] : const_map) {
        EXPECT_EQ(key, "CamA");
        EXPECT_EQ(*entry.zoom, 5);
        ++seen;
    }

    EXPECT_EQ(seen, 1u);
    EXPECT_EQ(std::distance(const_map.cbegin(), const_map.cend()), 1);
}

TEST(ConfigMapAccess, IteratorAndConstIteratorCompareAgainstEachOther) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    map.parse(YAML::Load("items:\n  - id: 1\n    label: a"));

    auto const& const_map = map;
    EXPECT_TRUE(map.begin() == const_map.begin());
    EXPECT_TRUE(map.end() == map.cend());
    EXPECT_TRUE(map.begin() != map.cend());
}

TEST(ConfigMapAccess, EmptyMapIterationIsANoop) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST, CARL::Required::NO};
    EXPECT_EQ(map.begin(), map.end());
    for (auto const& [id, item] : map) {
        (void)id;
        (void)item;
        FAIL() << "empty map must not yield entries";
    }
}

// ============================================================
//  parse() — overwrite across files (multi-file overlay)
// ============================================================

TEST(ConfigMapReparse, SecondParseOverlaysFieldsOfExistingEntry) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        base    = YAML::Load("items:\n  - id: 1\n    label: original");
    auto                        overlay = YAML::Load("items:\n  - id: 1\n    label: replaced");

    map.parse(base);
    EXPECT_NO_THROW(map.parse(overlay));
    EXPECT_EQ(map.size(), 1u);
    EXPECT_EQ(*map.at(1).label, "replaced");
}

TEST(ConfigMapReparse, SecondParseKeepsFieldsItDoesNotMention) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    auto                        base    = YAML::Load("items:\n  - id: 1\n    label: keep_me");
    auto                        overlay = YAML::Load("items:\n  - id: 1");

    map.parse(base);
    map.parse(overlay);
    EXPECT_TRUE(map.validate().correct);
    EXPECT_EQ(*map.at(1).label, "keep_me");
}

TEST(ConfigMapReparse, SecondParseAddsNewEntries) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    map.parse(YAML::Load("items:\n  - id: 1\n    label: a"));
    map.parse(YAML::Load("items:\n  - id: 2\n    label: b"));
    EXPECT_EQ(map.size(), 2u);
    EXPECT_EQ(*map.at(1).label, "a");
    EXPECT_EQ(*map.at(2).label, "b");
}

TEST(ConfigMapReparse, DuplicateWithinOneDocumentStillThrows) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    map.parse(YAML::Load("items:\n  - id: 1\n    label: a"));
    EXPECT_THROW(
        map.parse(YAML::Load("items:\n  - id: 2\n    label: b\n  - id: 2\n    label: c")),
        CARL::ParsingError
    );
}

TEST(ConfigMapReparse, StandardMapOverlaysByKey) {
    CARL::ConfigMap<th::ModelEntry, std::string> map {"models"};
    map.parse(YAML::Load("models:\n  CamA:\n    zoom: 5\n    display: wide"));
    EXPECT_NO_THROW(map.parse(YAML::Load("models:\n  CamA:\n    zoom: 50")));
    EXPECT_EQ(map.size(), 1u);
    EXPECT_EQ(*map.at("CamA").zoom, 50);
    EXPECT_EQ(*map.at("CamA").display, "wide");
}

TEST(ConfigMapReparse, SecondParseWithAbsentKeyKeepsEntries) {
    CARL::ConfigMap<th::IdItem> map {"items", CARL::MapType::ID_LIST};
    map.parse(YAML::Load("items:\n  - id: 1\n    label: a"));
    map.parse(YAML::Load("unrelated: 1"));
    EXPECT_EQ(map.size(), 1u);
    EXPECT_TRUE(map.validate().correct);
}

// ============================================================
//  static_assert for default-constructibility (documented)
// ============================================================
// ConfigMap<Group> requires Group to be default-constructible.
// This is enforced by a static_assert. Example that must NOT compile:
//   struct NonDefault { NonDefault() = delete; ... };
//   CARL::ConfigMap<NonDefault> bad{"x"};  // compile error
