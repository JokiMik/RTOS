#include <gtest/gtest.h>
#include "../TimeParser.h"

// Test suite: TimeParserTest

TEST(TimeParserTest, TestCaseTimeBoundaryCheck) {
    char time_test[] = "256789";
    ASSERT_EQ(time_parse(time_test),TIME_VALUE_ERROR);
    char time_test2[] = "111111";
    ASSERT_NE(time_parse(time_test2),TIME_VALUE_ERROR);
}

TEST(TimeParserTest, TestCaseCorrectTime) {
    char time_test[] = "141205";
    ASSERT_EQ(time_parse(time_test),51125);
    char time_test2[] = "000005";
    ASSERT_NE(time_parse(time_test2),20);
}

TEST(TimeParser, TestCaseTimeLenTooShort) {
    char time_test[] = "094";
    ASSERT_EQ(time_parse(time_test), TIME_LEN_ERROR);
    char time_test2[] = "111111";
    ASSERT_NE(time_parse(time_test2), TIME_LEN_ERROR);
}

TEST(TimeParser, TestCaseTimeLenTooLong) {
    char time_test[] = "09401234678";
    ASSERT_EQ(time_parse(time_test), TIME_LEN_ERROR);
    char time_test2[] = "111111";
    ASSERT_NE(time_parse(time_test2), TIME_LEN_ERROR);
}

TEST(TimeParserTest, TestCaseTimeNull) {
    char *time_test = NULL;
    ASSERT_EQ(time_parse(time_test),TIME_ARRAY_ERROR);
    char time_test2[] = "111111";
    ASSERT_NE(time_parse(time_test2),TIME_ARRAY_ERROR);
}

TEST(TimeParserTest, TestCaseTimeZero) {
    char time_test[] = "000000";
    ASSERT_EQ(time_parse(time_test),TIME_ZERO_ERROR);
    char time_test2[] = "111111";
    ASSERT_NE(time_parse(time_test2),TIME_ZERO_ERROR);
}

TEST(TimeParserTest, TestCaseTimeDigitError) {
    char time_test[] = "abcdef";
    ASSERT_EQ(time_parse(time_test),TIME_DIGIT_ERROR);
    char time_test2[] = "111111";
    ASSERT_NE(time_parse(time_test2),TIME_DIGIT_ERROR);
}

// https://google.github.io/googletest/reference/testing.html
// https://google.github.io/googletest/reference/assertions.html
