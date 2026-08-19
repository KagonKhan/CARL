#include <ostream>
#include <vector>
template <typename T>
std::ostream& operator <<(std::ostream& os, const std::vector<T>& vec)
{
    os << '[';

    for (std::size_t i = 0; i < vec.size(); ++i) {
        if (i != 0) {
            os << ", ";
        }

        os << vec[i];
    }

    os << ']';

    return os;
}

#include "config_value.hpp"
#include "utils/exceptions.hpp"

#include <gtest/gtest.h>
#include <limits>
#include <random>
#include <sstream>

// ============================================================
//  Construction
// ============================================================

TEST(ConfigValueConstruct, RequiredByDefault) {
    CARL::ConfigValue<int> cv {"x"};
    EXPECT_EQ(cv.name(), "x");
    // Not set, required -> validate fails
    auto result = cv.validate();
    EXPECT_FALSE(result.correct);
}

TEST(ConfigValueConstruct, ExplicitOptional) {
    CARL::ConfigValue<int> cv {"x", CARL::Required::NO};
    auto                   result = cv.validate();
    EXPECT_TRUE(result.correct);
}

TEST(ConfigValueConstruct, DefaultValueSetsValueAndMakesOptional) {
    CARL::ConfigValue<int> cv {"x", CARL::Default<int>{99}};
    auto                   result = cv.validate();
    EXPECT_TRUE(result.correct);
    EXPECT_EQ(*cv, 99);
}

TEST(ConfigValueConstruct, DefaultStringValue) {
    CARL::ConfigValue<std::string> cv {"key", CARL::Default<std::string>("hello")};
    EXPECT_TRUE(cv.validate().correct);
    EXPECT_EQ(*cv, "hello");
}

// ============================================================
//  parse() — happy paths
// ============================================================

TEST(ConfigValueParse, ParseInt) {
    CARL::ConfigValue<int> cv {"x"};
    auto                   node = YAML::Load("x: 42");
    cv.parse(node);
    EXPECT_TRUE(cv.validate().correct);
    EXPECT_EQ(*cv, 42);
}

TEST(ConfigValueParse, ParseNegativeInt) {
    CARL::ConfigValue<int> cv {"x"};
    auto                   node = YAML::Load("x: -7");
    cv.parse(node);
    EXPECT_EQ(*cv, -7);
}

TEST(ConfigValueParse, ParseIntMaxMin) {
    CARL::ConfigValue<int> cv {"x"};
    auto                   node = YAML::Load("x: " + std::to_string(std::numeric_limits<int>::max()));
    cv.parse(node);
    EXPECT_EQ(*cv, std::numeric_limits<int>::max());

    CARL::ConfigValue<int> cv2 {"x"};
    auto                   node2 = YAML::Load("x: " + std::to_string(std::numeric_limits<int>::min()));
    cv2.parse(node2);
    EXPECT_EQ(*cv2, std::numeric_limits<int>::min());
}

TEST(ConfigValueParse, ParseDouble) {
    CARL::ConfigValue<double> cv {"val"};
    auto                      node = YAML::Load("val: 3.14");
    cv.parse(node);
    EXPECT_DOUBLE_EQ(*cv, 3.14);
}

TEST(ConfigValueParse, ParseDouble_Scientific) {
    CARL::ConfigValue<double> cv {"val"};
    auto                      node = YAML::Load("val: 1.5e3");
    cv.parse(node);
    EXPECT_DOUBLE_EQ(*cv, 1500.0);
}

TEST(ConfigValueParse, ParseString) {
    CARL::ConfigValue<std::string> cv {"name"};
    auto                           node = YAML::Load("name: hello");
    cv.parse(node);
    EXPECT_EQ(*cv, "hello");
}

TEST(ConfigValueParse, ParseEmptyString) {
    CARL::ConfigValue<std::string> cv {"name"};
    auto                           node = YAML::Load("name: \"\"");
    cv.parse(node);
    EXPECT_EQ(*cv, "");
}

TEST(ConfigValueParse, ParseStringWithSpaces) {
    CARL::ConfigValue<std::string> cv {"name"};
    auto                           node = YAML::Load("name: \"hello world\"");
    cv.parse(node);
    EXPECT_EQ(*cv, "hello world");
}

TEST(ConfigValueParse, ParseBoolTrue) {
    CARL::ConfigValue<bool> cv {"flag"};
    auto                    node = YAML::Load("flag: true");
    cv.parse(node);
    EXPECT_TRUE(*cv);
}

TEST(ConfigValueParse, ParseBoolFalse) {
    CARL::ConfigValue<bool> cv {"flag"};
    auto                    node = YAML::Load("flag: false");
    cv.parse(node);
    EXPECT_FALSE(*cv);
}

TEST(ConfigValueParse, ParseBoolYesNo) {
    CARL::ConfigValue<bool> cv {"flag"};
    auto                    node = YAML::Load("flag: yes");
    cv.parse(node);
    EXPECT_TRUE(*cv);
}

TEST(ConfigValueParse, ParseFloat) {
    CARL::ConfigValue<float> cv {"val"};
    auto                     node = YAML::Load("val: 1.5");
    cv.parse(node);
    EXPECT_FLOAT_EQ(*cv, 1.5f);
}

TEST(ConfigValueParse, ParseUnsignedInt) {
    CARL::ConfigValue<unsigned int> cv {"val"};
    auto                            node = YAML::Load("val: 4000000000");
    cv.parse(node);
    EXPECT_EQ(*cv, 4000000000u);
}

TEST(ConfigValueParse, ParseVector) {
    CARL::ConfigValue<std::vector<int>> cv {"vals"};
    auto                                node = YAML::Load("vals: [1, 2, 3, 4]");
    cv.parse(node);
    auto& v = *cv;
    ASSERT_EQ(v.size(), 4u);
    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[3], 4);
}

TEST(ConfigValueParse, ParseVectorEmpty) {
    CARL::ConfigValue<std::vector<int>> cv {"vals"};
    auto                                node = YAML::Load("vals: []");
    cv.parse(node);
    EXPECT_TRUE(cv->empty());
}

// ============================================================
//  parse() — key missing (silent at parse time)
// ============================================================

TEST(ConfigValueParse, MissingKeyLeavesValueUnset) {
    CARL::ConfigValue<int> cv {"x"};
    auto                   node = YAML::Load("y: 42"); // "x" absent
    EXPECT_NO_THROW(cv.parse(node));
    EXPECT_FALSE(cv.validate().correct);
}

TEST(ConfigValueParse, MissingOptionalKeyIsStillValid) {
    CARL::ConfigValue<int> cv {"x", CARL::Required::NO};
    auto                   node = YAML::Load("y: 42");
    cv.parse(node);
    EXPECT_TRUE(cv.validate().correct);
}

// ============================================================
//  parse() — error cases
// ============================================================

TEST(ConfigValueParse, NonMapNodeThrows) {
    CARL::ConfigValue<int> cv {"x"};
    auto                   scalar_node = YAML::Load("42"); // scalar, not map
    EXPECT_THROW(cv.parse(scalar_node), CARL::ParsingError);
}

TEST(ConfigValueParse, SequenceNodeThrows) {
    CARL::ConfigValue<int> cv {"x"};
    auto                   seq_node = YAML::Load("[1, 2, 3]");
    EXPECT_THROW(cv.parse(seq_node), CARL::ParsingError);
}

TEST(ConfigValueParse, NullNodeDoesNotThrow) {
    CARL::ConfigValue<int> cv {"x"};
    YAML::Node             null_node;
    EXPECT_NO_THROW(cv.parse(null_node));
}

TEST(ConfigValueParse, TypeMismatchThrows) {
    CARL::ConfigValue<int> cv {"x"};
    auto                   node = YAML::Load("x: notanumber");
    EXPECT_THROW(cv.parse(node), CARL::ParsingError);
}

TEST(ConfigValueParse, TypeMismatchErrorMessageContainsFieldName) {
    CARL::ConfigValue<int> cv {"myfield"};
    auto                   node = YAML::Load("myfield: notanumber");
    try {
        cv.parse(node);
        FAIL() << "Expected ParsingError";
    }
    catch (CARL::ParsingError const& e) {
        EXPECT_NE(std::string(e.what()).find("myfield"), std::string::npos);
    }
}

// ============================================================
//  parse() — overwrite (second parse wins)
// ============================================================

TEST(ConfigValueParse, SecondParseOverwritesFirstValue) {
    CARL::ConfigValue<int> cv {"x"};
    auto                   node1 = YAML::Load("x: 10");
    auto                   node2 = YAML::Load("x: 99");
    cv.parse(node1);
    EXPECT_EQ(*cv, 10);
    cv.parse(node2);
    EXPECT_EQ(*cv, 99);
}

TEST(ConfigValueParse, SecondParseWithMissingKeyDoesNotClearPreviousValue) {
    CARL::ConfigValue<int> cv {"x"};
    auto                   node1 = YAML::Load("x: 10");
    auto                   node2 = YAML::Load("y: 99"); // "x" absent in second file
    cv.parse(node1);
    cv.parse(node2);
    EXPECT_EQ(*cv, 10);
}

// ============================================================
//  validate()
// ============================================================

TEST(ConfigValueValidate, RequiredAndSetIsSuccess) {
    CARL::ConfigValue<int> cv {"x"};
    auto                   node = YAML::Load("x: 1");
    cv.parse(node);
    EXPECT_TRUE(cv.validate().correct);
}

TEST(ConfigValueValidate, RequiredAndNotSetIsFailure) {
    CARL::ConfigValue<int> cv {"x"};
    auto                   result = cv.validate();
    EXPECT_FALSE(result.correct);
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].find("x"), std::string::npos);
}

TEST(ConfigValueValidate, OptionalAndNotSetIsSuccess) {
    CARL::ConfigValue<int> cv {"x", CARL::Required::NO};
    EXPECT_TRUE(cv.validate().correct);
}

TEST(ConfigValueValidate, OptionalWithDefaultIsSuccess) {
    CARL::ConfigValue<int> cv {"x", CARL::Default<int>{0}};
    EXPECT_TRUE(cv.validate().correct);
}

TEST(ConfigValueValidate, ErrorMessageContainsFieldName) {
    CARL::ConfigValue<int> cv {"my_specific_field"};
    auto                   result = cv.validate();
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].find("my_specific_field"), std::string::npos);
}

// ============================================================
//  operator*
// ============================================================

TEST(ConfigValueDeref, ReturnsParsedValue) {
    CARL::ConfigValue<int> cv {"x"};
    auto                   node = YAML::Load("x: 77");
    cv.parse(node);
    EXPECT_EQ(*cv, 77);
}

TEST(ConfigValueDeref, ReturnsDefaultValue) {
    CARL::ConfigValue<int> cv {"x", CARL::Default<int>{55}};
    EXPECT_EQ(*cv, 55);
}

#ifndef NDEBUG
TEST(ConfigValueDeathTest, DerefUnsetValueAsserts) {
    CARL::ConfigValue<int> cv {"x"};
    EXPECT_DEATH(
        {[[maybe_unused]] const int& v = *cv;
        },
        "dereferencing unset ConfigValue");
}
#endif

// ============================================================
//  patch()
// ============================================================

TEST(ConfigValuePatch, OverwritesValue) {
    CARL::ConfigValue<int> cv {"x"};
    cv.patch(123);
    EXPECT_EQ(*cv, 123);
}

TEST(ConfigValuePatch, PatchedRequiredFieldSatisfiesValidation) {
    CARL::ConfigValue<int> cv {"x"};
    ASSERT_FALSE(cv.validate().correct);
    cv.patch(123);
    EXPECT_TRUE(cv.validate().correct);
}

TEST(ConfigValuePatch, PatchedValueIsTaggedInPrint) {
    CARL::ConfigValue<int> cv {"x"};
    cv.patch(123);
    std::ostringstream os;
    cv.printTo(os, "");
    EXPECT_EQ(os.str(), "x: 123 (patched)\n");
}

TEST(ConfigValuePatch, CanPatchAfterParse) {
    CARL::ConfigValue<int> cv {"x"};
    auto                   node = YAML::Load("x: 10");
    cv.parse(node);
    cv.patch(999);
    EXPECT_EQ(*cv, 999);
}

TEST(ConfigValuePatch, CanPatchDefaultValue) {
    CARL::ConfigValue<int> cv {"x", CARL::Default<int>{42}};
    cv.patch(0);
    EXPECT_EQ(*cv, 0);
}

// ============================================================
//  printTo()
// ============================================================

TEST(ConfigValuePrint, ParsedValueHasNoTag) {
    CARL::ConfigValue<int> cv {"x"};
    auto                   node = YAML::Load("x: 42");
    cv.parse(node);
    std::ostringstream os;
    cv.printTo(os, "");
    std::string out = os.str();
    EXPECT_NE(out.find("x"), std::string::npos);
    EXPECT_NE(out.find("42"), std::string::npos);
    EXPECT_EQ(out.find("<missing>"), std::string::npos);
    EXPECT_EQ(out.find("(default)"), std::string::npos);
}

TEST(ConfigValuePrint, DefaultValueHasDefaultTag) {
    CARL::ConfigValue<int> cv {"x", CARL::Default<int>(7)};
    std::ostringstream     os;
    cv.printTo(os, "");
    std::string out = os.str();
    EXPECT_NE(out.find("(default)"), std::string::npos);
    EXPECT_NE(out.find("7"), std::string::npos);
}

TEST(ConfigValuePrint, UnsetValueHasMissingTag) {
    CARL::ConfigValue<int> cv {"x"};
    std::ostringstream     os;
    cv.printTo(os, "");
    std::string out = os.str();
    EXPECT_NE(out.find("<missing>"), std::string::npos);
}

TEST(ConfigValuePrint, ParsedValueHasNoTrailingSpace) {
    CARL::ConfigValue<int> cv {"port"};
    auto                   node = YAML::Load("port: 8080");
    cv.parse(node);
    std::ostringstream os;
    cv.printTo(os, "");
    EXPECT_EQ(os.str(), "port: 8080\n");
}

TEST(ConfigValuePrint, DefaultValueHasSingleSpaceBeforeTag) {
    CARL::ConfigValue<int> cv {"timeout", CARL::Default<int>(30)};
    std::ostringstream     os;
    cv.printTo(os, "");
    EXPECT_EQ(os.str(), "timeout: 30 (default)\n");
}

TEST(ConfigValuePrint, UnsetValueHasSingleSpaceBeforeTag) {
    CARL::ConfigValue<int> cv {"api_key"};
    std::ostringstream     os;
    cv.printTo(os, "");
    EXPECT_EQ(os.str(), "api_key: <missing>\n");
}

TEST(ConfigValuePrint, IndentIsApplied) {
    CARL::ConfigValue<int> cv {"x"};
    auto                   node = YAML::Load("x: 1");
    cv.parse(node);
    std::ostringstream os;
    cv.printTo(os, "  ");
    std::string out = os.str();
    EXPECT_EQ(out.substr(0, 2), "  ");
}

// ============================================================
//  Fuzz / property-based: random valid int values
// ============================================================

TEST(ConfigValueFuzz, RandomIntValues) {
    std::mt19937                       rng {42};
    std::uniform_int_distribution<int> dist {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};

    for (int i = 0; i < 500; ++i) {
        int                    val  = dist(rng);
        auto                   node = YAML::Load("x: " + std::to_string(val));
        CARL::ConfigValue<int> cv {"x"};
        EXPECT_NO_THROW(cv.parse(node));
        EXPECT_TRUE(cv.validate().correct);
        EXPECT_EQ(*cv, val);
    }
}

TEST(ConfigValueFuzz, RandomDoubleValues) {
    std::mt19937                           rng {123};
    std::uniform_real_distribution<double> dist {-1e9, 1e9};

    for (int i = 0; i < 200; ++i) {
        double val = dist(rng);
        // Use fmt to avoid locale-dependent formatting
        auto                      yaml_str = fmt::format("x: {:.10f}", val);
        auto                      node     = YAML::Load(yaml_str);
        CARL::ConfigValue<double> cv {"x"};
        EXPECT_NO_THROW(cv.parse(node));
        EXPECT_TRUE(cv.validate().correct);
        EXPECT_NEAR(*cv, val, std::abs(val) * 1e-6 + 1e-9);
    }
}

TEST(ConfigValueFuzz, MalformedYamlDoesNotCrash) {
    static const std::vector<std::string> malformed {
        "",
        ":",
        "- - -",
        "null",
        "true",
        "42",
        "[1, 2, 3]",
        "{}",
        std::string(1000, 'a'),
    };

    for (auto const& input : malformed) {
        try {
            auto                   node = YAML::Load(input);
            CARL::ConfigValue<int> cv {"x"};
            cv.parse(node);
        }
        catch (CARL::ParsingError const&) { /* expected */
        }
        catch (YAML::Exception const&) {    /* expected */
        }
        // Must not crash or throw anything else
    }
}

TEST(ConfigValueFuzz, EdgeCaseStrings) {
    static const std::vector<std::string> strings {
        "",
        " ",
        "\t",
        "a",
        std::string(1000, 'x'),
        "hello\nworld",  // YAML will treat this as a literal block
        "null",
        "true",
        "1.0",
        "0",
    };

    for (auto const& s : strings) {
        auto                           node = YAML::Load("key: \"" + s + "\"");
        CARL::ConfigValue<std::string> cv {"key"};
        EXPECT_NO_THROW(cv.parse(node));
        EXPECT_TRUE(cv.validate().correct);
    }
}
