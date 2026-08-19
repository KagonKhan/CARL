#include "utils/utils.hpp"

#include <gtest/gtest.h>
#include <limits>

// ============================================================
//  ValidationResult
// ============================================================

TEST(ValidationResult, SuccessIsCorrectAndEmpty) {
    auto r = CARL::ValidationResult::success();
    EXPECT_TRUE(r.correct);
    EXPECT_TRUE(r.errors.empty());
}

TEST(ValidationResult, FailureIsIncorrectWithOneError) {
    auto r = CARL::ValidationResult::failure("field {} missing", "x");
    EXPECT_FALSE(r.correct);
    ASSERT_EQ(r.errors.size(), 1u);
    EXPECT_EQ(r.errors[0], "field x missing");
}

TEST(ValidationResult, FailureFormatsIntArgument) {
    auto r = CARL::ValidationResult::failure("value {} out of range", 42);
    ASSERT_EQ(r.errors.size(), 1u);
    EXPECT_EQ(r.errors[0], "value 42 out of range");
}

TEST(ValidationResult, MergeSuccessIntoSuccess) {
    auto r = CARL::ValidationResult::success();
    r.merge(CARL::ValidationResult::success());
    EXPECT_TRUE(r.correct);
    EXPECT_TRUE(r.errors.empty());
}

TEST(ValidationResult, MergeFailureIntoSuccess) {
    auto r = CARL::ValidationResult::success();
    r.merge(CARL::ValidationResult::failure("oops"));
    EXPECT_FALSE(r.correct);
    ASSERT_EQ(r.errors.size(), 1u);
    EXPECT_EQ(r.errors[0], "oops");
}

TEST(ValidationResult, MergeSuccessIntoFailureDoesNotClear) {
    auto r = CARL::ValidationResult::failure("first error");
    r.merge(CARL::ValidationResult::success());
    EXPECT_FALSE(r.correct);
    ASSERT_EQ(r.errors.size(), 1u);
    EXPECT_EQ(r.errors[0], "first error");
}

TEST(ValidationResult, MergeAccumulatesAllErrors) {
    auto r = CARL::ValidationResult::success();
    r.merge(CARL::ValidationResult::failure("err1"));
    r.merge(CARL::ValidationResult::failure("err2"));
    r.merge(CARL::ValidationResult::failure("err3"));
    EXPECT_FALSE(r.correct);
    ASSERT_EQ(r.errors.size(), 3u);
    EXPECT_EQ(r.errors[0], "err1");
    EXPECT_EQ(r.errors[1], "err2");
    EXPECT_EQ(r.errors[2], "err3");
}

TEST(ValidationResult, MergePreservesExistingErrorsOnMultiFailureMerge) {
    auto r     = CARL::ValidationResult::failure("original");
    auto other = CARL::ValidationResult::failure("new");
    r.merge(std::move(other));
    ASSERT_EQ(r.errors.size(), 2u);
    EXPECT_EQ(r.errors[0], "original");
    EXPECT_EQ(r.errors[1], "new");
}

// ============================================================
//  ValueSource
// ============================================================

TEST(ValueSource, UnsetIsNotSet) {
    auto s = CARL::ValueSource::unset();
    EXPECT_FALSE(s.isSet());
    EXPECT_FALSE(s.isDefault());
    EXPECT_FALSE(s.isParsed());
}

TEST(ValueSource, DefaultIsSetAndDefault) {
    auto s = CARL::ValueSource::fromDefault();
    EXPECT_TRUE(s.isSet());
    EXPECT_TRUE(s.isDefault());
    EXPECT_FALSE(s.isParsed());
}

TEST(ValueSource, ParsedIsSetAndParsed) {
    auto s = CARL::ValueSource::parsed();
    EXPECT_TRUE(s.isSet());
    EXPECT_FALSE(s.isDefault());
    EXPECT_TRUE(s.isParsed());
}

TEST(ValueSource, MarkParsedTransitionsUnsetToParsed) {
    auto s = CARL::ValueSource::unset();
    s.markParsed();
    EXPECT_TRUE(s.isSet());
    EXPECT_TRUE(s.isParsed());
    EXPECT_FALSE(s.isDefault());
}

TEST(ValueSource, MarkParsedTransitionsDefaultToParsed) {
    auto s = CARL::ValueSource::fromDefault();
    s.markParsed();
    EXPECT_TRUE(s.isParsed());
    EXPECT_FALSE(s.isDefault());
}

TEST(ValueSource, DisplayTagUnset) {
    EXPECT_EQ(CARL::ValueSource::unset().displayTag(), "<missing>");
}

TEST(ValueSource, DisplayTagDefault) {
    EXPECT_EQ(CARL::ValueSource::fromDefault().displayTag(), "(default)");
}

TEST(ValueSource, DisplayTagParsed) {
    EXPECT_EQ(CARL::ValueSource::parsed().displayTag(), "");
}

TEST(ValueSource, PatchedIsSetAndPatched) {
    auto s = CARL::ValueSource::patched();
    EXPECT_TRUE(s.isSet());
    EXPECT_TRUE(s.isPatched());
    EXPECT_FALSE(s.isDefault());
    EXPECT_FALSE(s.isParsed());
}

TEST(ValueSource, MarkPatchedTransitionsUnsetToPatched) {
    auto s = CARL::ValueSource::unset();
    s.markPatched();
    EXPECT_TRUE(s.isSet());
    EXPECT_TRUE(s.isPatched());
}

TEST(ValueSource, MarkPatchedOverridesParsed) {
    auto s = CARL::ValueSource::parsed();
    s.markPatched();
    EXPECT_TRUE(s.isPatched());
    EXPECT_FALSE(s.isParsed());
}

TEST(ValueSource, DisplayTagPatched) {
    EXPECT_EQ(CARL::ValueSource::patched().displayTag(), "(patched)");
}

TEST(ValueSource, DefaultConstructorIsUnset) {
    CARL::ValueSource s;
    EXPECT_FALSE(s.isSet());
    EXPECT_EQ(s.displayTag(), "<missing>");
}

// ============================================================
//  Required
// ============================================================

TEST(Required, YesIsTrue) {
    EXPECT_TRUE(static_cast<bool>(CARL::Required::YES));
}

TEST(Required, NoIsFalse) {
    EXPECT_FALSE(static_cast<bool>(CARL::Required::NO));
}

TEST(Required, NotYesIsFalse) {
    EXPECT_FALSE(!CARL::Required::YES);
}

TEST(Required, NotNoIsTrue) {
    EXPECT_TRUE(!CARL::Required::NO);
}

TEST(Required, EqualityYesYes) {
    EXPECT_TRUE(CARL::Required::YES == CARL::Required::YES);
    EXPECT_FALSE(CARL::Required::YES != CARL::Required::YES);
}

TEST(Required, EqualityNoNo) {
    EXPECT_TRUE(CARL::Required::NO == CARL::Required::NO);
}

TEST(Required, InequalityYesNo) {
    EXPECT_TRUE(CARL::Required::YES != CARL::Required::NO);
    EXPECT_FALSE(CARL::Required::YES == CARL::Required::NO);
}

// ============================================================
//  reindent
// ============================================================

TEST(Reindent, SingleLinePassthrough) {
    EXPECT_EQ(CARL::reindent("hello world", "  "), "hello world");
}

TEST(Reindent, EmptyStringPassthrough) {
    EXPECT_EQ(CARL::reindent("", "  "), "");
}

TEST(Reindent, TwoLinesNoIndent) {
    // Each line gets prefixed with "\n" + indent + "  "
    auto result = CARL::reindent("line1\nline2", "");
    EXPECT_EQ(result, "\n  line1\n  line2");
}

TEST(Reindent, TwoLinesWithIndent) {
    auto result = CARL::reindent("line1\nline2", "  ");
    EXPECT_EQ(result, "\n    line1\n    line2");
}

TEST(Reindent, ThreeLines) {
    auto result = CARL::reindent("a\nb\nc", "");
    EXPECT_EQ(result, "\n  a\n  b\n  c");
}

TEST(Reindent, TrailingNewlineDoesNotAddExtraLine) {
    // "line1\n" has a trailing newline; after stripping, should produce one line
    auto result = CARL::reindent("line1\n", "");
    EXPECT_EQ(result, "\n  line1");
}

TEST(Reindent, CRLFLineEnding) {
    auto result = CARL::reindent("line1\r\nline2", "");
    EXPECT_EQ(result, "\n  line1\n  line2");
}

TEST(Reindent, CRLFTrailingDoesNotAddExtraLine) {
    auto result = CARL::reindent("line1\r\n", "");
    EXPECT_EQ(result, "\n  line1");
}

TEST(Reindent, SingleLineWithNewlineInIndent) {
    // No newline in text — still a passthrough regardless of indent content
    EXPECT_EQ(CARL::reindent("value", "    "), "value");
}
