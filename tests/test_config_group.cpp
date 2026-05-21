#include "helpers.hpp"
#include "utils/exceptions.hpp"
#include "utils/utils.hpp"

#include <gtest/gtest.h>
#include <sstream>

// ============================================================
//  parse() — named group
// ============================================================

TEST(ConfigGroupParse, NamedGroupParsesUnderItsKey) {
    th::NamedPairGroup g;
    auto               node = YAML::Load("pair:\n  x: 10\n  y: 20");
    g.parse(node);
    EXPECT_TRUE(g.validate().correct);
    EXPECT_EQ(*g.x, 10);
    EXPECT_EQ(*g.y, 20);
}

TEST(ConfigGroupParse, NamedGroupAbsentLeavesEntriesUnset) {
    th::NamedPairGroup g;
    auto               node = YAML::Load("other:\n  x: 1\n  y: 2");
    g.parse(node);  // "pair" not present
    // Validate should fail — group is required and was not parsed
    auto result = g.validate();
    EXPECT_FALSE(result.correct);
}

TEST(ConfigGroupParse, NamedGroupEmptyYamlDoesNotCrash) {
    th::NamedPairGroup g;
    YAML::Node         node; // null node
    EXPECT_NO_THROW(g.parse(node));
}

TEST(ConfigGroupParse, NamedGroupCustomName) {
    th::NamedPairGroup g {"coords"};
    auto               node = YAML::Load("coords:\n  x: 5\n  y: 6");
    g.parse(node);
    EXPECT_EQ(*g.x, 5);
    EXPECT_EQ(*g.y, 6);
}

// ============================================================
//  parse() — nameless group
// ============================================================

TEST(ConfigGroupParse, NamelessGroupParsesDirectlyFromNode) {
    th::PairGroup g;
    auto          node = YAML::Load("x: 3\ny: 7");
    g.parse(node);
    EXPECT_TRUE(g.validate().correct);
    EXPECT_EQ(*g.x, 3);
    EXPECT_EQ(*g.y, 7);
}

TEST(ConfigGroupParse, NamelessGroupNullNodeDoesNotCrash) {
    th::PairGroup g;
    YAML::Node    node;
    EXPECT_NO_THROW(g.parse(node));
}

// ============================================================
//  parse() — overwrite (multiple files)
// ============================================================

TEST(ConfigGroupParse, SecondParseOverwritesValues) {
    th::NamedPairGroup g;
    auto               node1 = YAML::Load("pair:\n  x: 1\n  y: 2");
    auto               node2 = YAML::Load("pair:\n  x: 99\n  y: 88");
    g.parse(node1);
    g.parse(node2);
    EXPECT_EQ(*g.x, 99);
    EXPECT_EQ(*g.y, 88);
}

TEST(ConfigGroupParse, SecondParseWithAbsentKeyDoesNotClearValues) {
    th::NamedPairGroup g;
    auto               node1 = YAML::Load("pair:\n  x: 1\n  y: 2");
    auto               node2 = YAML::Load("other:\n  x: 9");
    g.parse(node1);
    g.parse(node2);  // "pair" absent in second node
    EXPECT_EQ(*g.x, 1);
    EXPECT_EQ(*g.y, 2);
}

// ============================================================
//  validate()
// ============================================================

TEST(ConfigGroupValidate, AllRequiredPresentIsSuccess) {
    th::NamedPairGroup g;
    auto               node = YAML::Load("pair:\n  x: 1\n  y: 2");
    g.parse(node);
    EXPECT_TRUE(g.validate().correct);
}

TEST(ConfigGroupValidate, RequiredFieldMissingIsFailure) {
    th::NamedPairGroup g;
    auto               node = YAML::Load("pair:\n  x: 1"); // y missing
    g.parse(node);
    auto result = g.validate();
    EXPECT_FALSE(result.correct);
    ASSERT_EQ(result.errors.size(), 1u);
}

TEST(ConfigGroupValidate, BothRequiredFieldsMissingGivesTwoErrors) {
    th::NamedPairGroup g;
    auto               node = YAML::Load("pair: {}");
    g.parse(node);
    auto result = g.validate();
    EXPECT_FALSE(result.correct);
    EXPECT_EQ(result.errors.size(), 2u);
}

TEST(ConfigGroupValidate, ErrorIsPrefixedWithGroupName) {
    th::NamedPairGroup g {"coords"};
    auto               node = YAML::Load("coords:\n  x: 1"); // y missing
    g.parse(node);
    auto result = g.validate();
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].find("coords"), std::string::npos);
    EXPECT_NE(result.errors[0].find("y"), std::string::npos);
}

TEST(ConfigGroupValidate, NamelessGroupErrorHasNoPrefix) {
    th::PairGroup g;
    auto          node = YAML::Load("x: 1"); // y missing
    g.parse(node);
    auto result = g.validate();
    ASSERT_EQ(result.errors.size(), 1u);
    // Error should mention "y" but not have a "." prefix
    EXPECT_NE(result.errors[0].find("y"), std::string::npos);
    EXPECT_EQ(result.errors[0].find('.'), std::string::npos);
}

TEST(ConfigGroupValidate, OptionalGroupAbsentIsSuccess) {
    th::OptionalSection g;
    auto                node = YAML::Load("something_else: 1");
    g.parse(node);
    EXPECT_TRUE(g.validate().correct);
}

TEST(ConfigGroupValidate, OptionalGroupPresentButEntriesMissingIsFailure) {
    th::OptionalSection g;
    // Group key exists but the required field inside is absent
    auto node = YAML::Load("optional_section: {}");
    g.parse(node);
    auto result = g.validate();
    EXPECT_FALSE(result.correct);
}

TEST(ConfigGroupValidate, OptionalGroupPresentAndCompleteIsSuccess) {
    th::OptionalSection g;
    auto                node = YAML::Load("optional_section:\n  value: 42");
    g.parse(node);
    EXPECT_TRUE(g.validate().correct);
}

TEST(ConfigGroupValidate, OptionalFieldAllowedMissing) {
    th::PersonGroup g;
    auto            node = YAML::Load("person:\n  name: Alice\n  age: 30"); // email absent
    g.parse(node);
    EXPECT_TRUE(g.validate().correct);
}

// ============================================================
//  validate() — nested groups
// ============================================================

TEST(ConfigGroupValidate, NestedGroupErrorHasFullPath) {
    th::OuterGroup g;
    // inner.val is missing, outer.x is present
    auto node = YAML::Load("outer:\n  x: 5\n  inner: {}");
    g.parse(node);
    auto result = g.validate();
    EXPECT_FALSE(result.correct);
    ASSERT_GE(result.errors.size(), 1u);
    // Error should contain both "outer" and "inner" and "val"
    bool found = false;
    for (auto const& e : result.errors) {
        if ((e.find("outer") != std::string::npos) &&
            (e.find("inner") != std::string::npos) &&
            (e.find("val") != std::string::npos)) {
            found = true;
        }
    }

    EXPECT_TRUE(found) << "Expected a prefixed error like 'outer.inner.val: is missing'";
}

TEST(ConfigGroupValidate, NestedGroupAllPresentIsSuccess) {
    th::OuterGroup g;
    auto           node = YAML::Load("outer:\n  x: 5\n  inner:\n    val: 99");
    g.parse(node);
    EXPECT_TRUE(g.validate().correct);
}

TEST(ConfigGroupValidate, NestedGroupAllMissingHasMultipleErrors) {
    th::OuterGroup g;
    auto           node = YAML::Load("outer: {}");
    g.parse(node);
    auto result = g.validate();
    EXPECT_FALSE(result.correct);
    // Expect errors for inner.val and x
    EXPECT_GE(result.errors.size(), 2u);
}

// ============================================================
//  printTo()
// ============================================================

TEST(ConfigGroupPrint, NamedGroupPrintsKey) {
    th::NamedPairGroup g;
    auto               node = YAML::Load("pair:\n  x: 1\n  y: 2");
    g.parse(node);
    std::ostringstream os;
    g.printTo(os, "");
    std::string out = os.str();
    EXPECT_NE(out.find("pair"), std::string::npos);
    EXPECT_NE(out.find("x"), std::string::npos);
    EXPECT_NE(out.find("y"), std::string::npos);
}

TEST(ConfigGroupPrint, NamelessGroupDoesNotPrintExtraHeader) {
    th::PairGroup g;
    auto          node = YAML::Load("x: 10\ny: 20");
    g.parse(node);
    std::ostringstream os;
    g.printTo(os, "  ");  // non-empty indent -> not base level
    std::string out = os.str();
    EXPECT_NE(out.find("x"), std::string::npos);
    EXPECT_NE(out.find("y"), std::string::npos);
}

TEST(ConfigGroupPrint, BaseLevelAddsTrailingNewlines) {
    th::NamedPairGroup g;
    auto               node = YAML::Load("pair:\n  x: 1\n  y: 2");
    g.parse(node);
    std::ostringstream os;
    g.printTo(os, "");  // empty indent = base level
    std::string out = os.str();
    EXPECT_TRUE(out.size() >= 2u);
    EXPECT_EQ(out.substr(out.size() - 2), "\n\n");
}

TEST(ConfigGroupPrint, NestedGroupIndentsContents) {
    th::OuterGroup g;
    auto           node = YAML::Load("outer:\n  x: 5\n  inner:\n    val: 3");
    g.parse(node);
    std::ostringstream os;
    g.printTo(os, "");
    std::string out = os.str();
    // "outer" should appear at column 0, "inner" and "x" indented
    EXPECT_EQ(out.find("outer"), 0u);
    EXPECT_NE(out.find("  inner"), std::string::npos);
    EXPECT_NE(out.find("  x"), std::string::npos);
}

TEST(ConfigGroupPrint, DefaultAndParsedTagsAppear) {
    th::ConfigWithDefaults g;
    auto                   node = YAML::Load("config:\n  required_field: 1");
    g.parse(node);
    std::ostringstream os;
    g.printTo(os, "");
    std::string out = os.str();
    EXPECT_NE(out.find("(default)"), std::string::npos);
    EXPECT_EQ(out.find("<missing>"), std::string::npos);
}

// ============================================================
//  id-must-be-first assert (debug only)
// ============================================================

#ifndef NDEBUG
TEST(ConfigGroupDeathTest, IdNotFirstTriggersAssert) {
    th::BadIdOrderGroup g;
    auto                node = YAML::Load("other: 1\nid: 2");
    EXPECT_DEATH(g.parse(node), "id member is required to be first if present");
}
#endif

// ============================================================
//  registerEntries — compile-time constraint (documented here)
// ============================================================
// The static_assert in registerEntries() ensures only IConfigValue subclasses
// are registered. This cannot be tested at runtime; it produces a compile error.
// Example of what should NOT compile:
//   struct Bad : CARL::ConfigGroup { Bad() { int x = 1; registerEntries(x); } };
