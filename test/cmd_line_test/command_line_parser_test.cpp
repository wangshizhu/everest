/**
 * @file command_line_parser_test.cpp
 * @brief CommandLineParser 的 GoogleTest 用例。
 */

#include "cmd_line/command_line_parser.h"

#include <cstdlib>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

/**
 * @brief 用命令行字符串解析一个 bool 选项。
 *
 * @param text 布尔文本。
 * @param out 解析结果。
 * @return 解析成功且取值成功返回 true。
 */
static bool ParseBoolArgument(const std::string& text, bool& out) {
    everest::CommandLineParser parser;
    parser.Add<bool>({.name = "flag_value", .short_name = 0, .desc = "", .need = false}, false);
    if (!parser.Parse("--flag_value=" + text)) {
        return false;
    }
    return parser.TryGet<bool>("flag_value", out);
}

TEST(CommandLineParserTest, IsNotCopyable) {
    static_assert(!std::is_copy_constructible_v<everest::CommandLineParser>);
    static_assert(!std::is_copy_assignable_v<everest::CommandLineParser>);
    static_assert(!std::is_move_constructible_v<everest::CommandLineParser>);
    static_assert(!std::is_move_assignable_v<everest::CommandLineParser>);
    SUCCEED();
}

TEST(CommandLineParserTest, AddAndGetDefault) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "item count", .need = true}, 5);

    EXPECT_EQ(parser.Get<int>("count"), 5);
    EXPECT_TRUE(parser.Exist("count"));
    EXPECT_FALSE(parser.IsSet("count"));
}

TEST(CommandLineParserTest, ParseLongOptionWithEquals) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "item count", .need = false}, 0);

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--count=7"}));

    EXPECT_EQ(parser.Get<int>("count"), 7);
    EXPECT_TRUE(parser.IsSet("count"));
}

TEST(CommandLineParserTest, ParseLongOptionSeparateValue) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "item count", .need = false}, 0);

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--count", "7"}));

    EXPECT_EQ(parser.Get<int>("count"), 7);
}

TEST(CommandLineParserTest, ParseShortOptionWithValue) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "item count", .need = false}, 0);

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "-c", "7"}));

    EXPECT_EQ(parser.Get<int>("count"), 7);
}

TEST(CommandLineParserTest, ParseBundledFlags) {
    everest::CommandLineParser parser;
    parser.AddWithoutValueCommand({.name = "aaa", .short_name = 'a', .desc = "a flag"});
    parser.AddWithoutValueCommand({.name = "bbb", .short_name = 'b', .desc = "b flag"});
    parser.AddWithoutValueCommand({.name = "ccc", .short_name = 'c', .desc = "c flag"});

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "-abc"}));

    EXPECT_TRUE(parser.IsSet("aaa"));
    EXPECT_TRUE(parser.IsSet("bbb"));
    EXPECT_TRUE(parser.IsSet("ccc"));
}

TEST(CommandLineParserTest, FlagDoesNotConsumeValue) {
    everest::CommandLineParser parser;
    parser.AddWithoutValueCommand({.name = "aaa", .short_name = 'a', .desc = "a flag"});

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "-a", "value"}));

    EXPECT_TRUE(parser.IsSet("aaa"));
    ASSERT_EQ(parser.Rest().size(), 1U);
    EXPECT_EQ(parser.Rest()[0], "value");
}

TEST(CommandLineParserTest, PositionalsCollected) {
    everest::CommandLineParser parser;
    parser.AddWithoutValueCommand({.name = "aaa", .short_name = 'a', .desc = "a flag"});

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "foo", "bar"}));

    const std::vector<std::string>& rest = parser.Rest();
    ASSERT_EQ(rest.size(), 2U);
    EXPECT_EQ(rest[0], "foo");
    EXPECT_EQ(rest[1], "bar");
}

TEST(CommandLineParserTest, BareDashIgnored) {
    everest::CommandLineParser parser;
    parser.AddWithoutValueCommand({.name = "aaa", .short_name = 'a', .desc = "a flag"});

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "-"}));

    EXPECT_TRUE(parser.Rest().empty());
}

TEST(CommandLineParserTest, BareDoubleDashUndefined) {
    everest::CommandLineParser parser;

    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog", "--"}));
    EXPECT_NE(parser.Error().find("undefined option: --"), std::string::npos);
}

TEST(CommandLineParserTest, StringValue) {
    everest::CommandLineParser parser;
    parser.Add<std::string>({.name = "name", .short_name = 'n', .desc = "name", .need = false}, "");

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--name=abc"}));

    EXPECT_EQ(parser.Get<std::string>("name"), "abc");
}

TEST(CommandLineParserTest, BoolValueParsing) {
    bool value = false;

    ASSERT_TRUE(ParseBoolArgument("1", value));
    EXPECT_TRUE(value);
    ASSERT_TRUE(ParseBoolArgument("true", value));
    EXPECT_TRUE(value);
    ASSERT_TRUE(ParseBoolArgument("yes", value));
    EXPECT_TRUE(value);
    ASSERT_TRUE(ParseBoolArgument("0", value));
    EXPECT_FALSE(value);
    ASSERT_TRUE(ParseBoolArgument("false", value));
    EXPECT_FALSE(value);
    ASSERT_TRUE(ParseBoolArgument("no", value));
    EXPECT_FALSE(value);
    EXPECT_FALSE(ParseBoolArgument("maybe", value));
}

TEST(CommandLineParserTest, IntAndInt32Value) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "count", .need = false}, 0);
    parser.Add<int32_t>({.name = "count32", .short_name = 'd', .desc = "count32", .need = false}, 0);

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--count=-5", "--count32=12"}));

    EXPECT_EQ(parser.Get<int>("count"), -5);
    EXPECT_EQ(parser.Get<int32_t>("count32"), 12);
}

TEST(CommandLineParserTest, Int64Value) {
    everest::CommandLineParser parser;
    parser.Add<int64_t>({.name = "big", .short_name = 'b', .desc = "big", .need = false}, 0);

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--big=9000000000"}));

    EXPECT_EQ(parser.Get<int64_t>("big"), 9000000000LL);
}

TEST(CommandLineParserTest, Uint32Value) {
    everest::CommandLineParser parser;
    parser.Add<uint32_t>({.name = "limit", .short_name = 'l', .desc = "limit", .need = false}, 0);

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--limit=4294967295"}));
    EXPECT_EQ(parser.Get<uint32_t>("limit"), 4294967295U);
    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog", "--limit=-1"}));
    EXPECT_NE(parser.Error().find("option value is invalid"), std::string::npos);
}

TEST(CommandLineParserTest, Uint64AndSizeValue) {
    everest::CommandLineParser parser;
    parser.Add<uint64_t>({.name = "big", .short_name = 'b', .desc = "big", .need = false}, 0);
    parser.Add<size_t>({.name = "bytes", .short_name = 's', .desc = "bytes", .need = false}, 0);

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--big=123456789012345", "--bytes=42"}));

    EXPECT_EQ(parser.Get<uint64_t>("big"), 123456789012345ULL);
    EXPECT_EQ(parser.Get<size_t>("bytes"), static_cast<size_t>(42));
}

TEST(CommandLineParserTest, NumericLeadingPlus) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "count", .need = false}, 0);
    parser.Add<float>({.name = "ratio", .short_name = 'r', .desc = "ratio", .need = false}, 0.0F);
    parser.Add<int64_t>({.name = "big", .short_name = 'b', .desc = "big", .need = false}, 0);
    parser.Add<uint32_t>({.name = "limit", .short_name = 'l', .desc = "limit", .need = false}, 0);

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--count=+5", "--ratio=+0.5", "--big=+9000000000"}));

    EXPECT_EQ(parser.Get<int>("count"), 5);
    EXPECT_FLOAT_EQ(parser.Get<float>("ratio"), 0.5F);
    EXPECT_EQ(parser.Get<int64_t>("big"), 9000000000LL);

    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog", "--limit=+"}));
    EXPECT_NE(parser.Error().find("option value is invalid"), std::string::npos);
}

TEST(CommandLineParserTest, FloatAndDoubleValue) {
    everest::CommandLineParser parser;
    parser.Add<float>({.name = "ratio", .short_name = 'r', .desc = "ratio", .need = false}, 0.0F);
    parser.Add<double>({.name = "scale", .short_name = 's', .desc = "scale", .need = false}, 0.0);

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--ratio", "0.5", "--scale=2.25"}));

    EXPECT_FLOAT_EQ(parser.Get<float>("ratio"), 0.5F);
    EXPECT_DOUBLE_EQ(parser.Get<double>("scale"), 2.25);
}

TEST(CommandLineParserTest, IntRangeAcceptsInRange) {
    everest::CommandLineParser parser;
    parser.AddIntRange({.name = "port", .short_name = 'p', .desc = "listen port", .need = false}, 80, {1, 65535});

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--port", "8080"}));

    EXPECT_EQ(parser.Get<int>("port"), 8080);
}

TEST(CommandLineParserTest, IntRangeRejectsOutOfRange) {
    everest::CommandLineParser parser;
    parser.AddIntRange({.name = "port", .short_name = 'p', .desc = "listen port", .need = false}, 80, {1, 65535});

    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog", "--port=70000"}));
    EXPECT_NE(parser.Error().find("option value is invalid"), std::string::npos);

    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog", "--port=0"}));
    EXPECT_NE(parser.Error().find("option value is invalid"), std::string::npos);
}

TEST(CommandLineParserTest, Int64Range) {
    everest::CommandLineParser parser;
    parser.AddInt64Range({.name = "big", .short_name = 'b', .desc = "big", .need = false}, 0, {-10, 10});

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--big=-10"}));
    EXPECT_EQ(parser.Get<int64_t>("big"), -10);
    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--big=10"}));
    EXPECT_EQ(parser.Get<int64_t>("big"), 10);
    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog", "--big=11"}));
}

TEST(CommandLineParserTest, Uint32Range) {
    everest::CommandLineParser parser;
    parser.AddUint32Range({.name = "mask", .short_name = 'm', .desc = "mask", .need = false}, 0, {0, 10});

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--mask=10"}));
    EXPECT_EQ(parser.Get<uint32_t>("mask"), 10U);
    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog", "--mask=11"}));
}

TEST(CommandLineParserTest, Uint64Range) {
    everest::CommandLineParser parser;
    parser.AddUint64Range({.name = "bytes", .short_name = 'b', .desc = "bytes", .need = false}, 0, {0, 10});

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--bytes=10"}));
    EXPECT_EQ(parser.Get<uint64_t>("bytes"), 10U);
    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog", "--bytes=11"}));
}

TEST(CommandLineParserTest, StringOneOfAcceptsAllowed) {
    everest::CommandLineParser parser;
    parser.AddStringOneOf({.name = "format", .short_name = 'f', .desc = "format", .need = false}, "json", {"json", "xml"});

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--format", "xml"}));

    EXPECT_EQ(parser.Get<std::string>("format"), "xml");
}

TEST(CommandLineParserTest, StringOneOfRejectsOther) {
    everest::CommandLineParser parser;
    parser.AddStringOneOf({.name = "format", .short_name = 'f', .desc = "format", .need = false}, "json", {"json", "xml"});

    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog", "--format", "yaml"}));
    EXPECT_NE(parser.Error().find("option value is invalid"), std::string::npos);
}

TEST(CommandLineParserTest, InvalidArguments) {
    everest::CommandLineParser parser;

    EXPECT_FALSE(parser.Parse(0, nullptr));
    EXPECT_FALSE(parser.Error().empty());

    const char* const null_argument[] = {nullptr};
    EXPECT_FALSE(parser.Parse(1, null_argument));
    EXPECT_FALSE(parser.Error().empty());
}

TEST(CommandLineParserTest, UndefinedLongOption) {
    everest::CommandLineParser parser;

    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog", "--nope"}));
    EXPECT_NE(parser.Error().find("undefined option"), std::string::npos);
}

TEST(CommandLineParserTest, UndefinedShortOption) {
    everest::CommandLineParser parser;

    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog", "-z"}));
    EXPECT_FALSE(parser.Error().empty());
}

TEST(CommandLineParserTest, MissingValue) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "count", .need = false}, 0);

    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog", "--count"}));
    EXPECT_NE(parser.Error().find("option needs value"), std::string::npos);
}

TEST(CommandLineParserTest, InvalidValue) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "count", .need = false}, 0);

    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog", "--count", "abc"}));
    EXPECT_NE(parser.Error().find("option value is invalid"), std::string::npos);
}

TEST(CommandLineParserTest, FlagRejectsInlineValue) {
    everest::CommandLineParser parser;
    parser.AddWithoutValueCommand({.name = "verbose", .short_name = 'v', .desc = "verbose"});

    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog", "--verbose=1"}));
    EXPECT_EQ(parser.Error(), "option does not take value: --verbose=1");
}

TEST(CommandLineParserTest, GetOnFlagReportsNoValue) {
    everest::CommandLineParser parser;
    parser.AddWithoutValueCommand({.name = "verbose", .short_name = 'v', .desc = "verbose"});

    EXPECT_EQ(parser.Get<int>("verbose"), 0);
    EXPECT_NE(parser.Error().find("flag 'verbose' has no value"), std::string::npos);
}

TEST(CommandLineParserTest, RequiredOptionMissing) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "count", .need = true}, 0);

    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog"}));
    EXPECT_NE(parser.Error().find("need option"), std::string::npos);
}

TEST(CommandLineParserTest, DuplicateShortNameRejectedAtAdd) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "first", .short_name = 'x', .desc = "first", .need = false}, 0);
    parser.Add<int>({.name = "second", .short_name = 'x', .desc = "second", .need = false}, 1);

    EXPECT_FALSE(parser.Exist("second"));
    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog"}));
    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "-x", "3"}));
    EXPECT_TRUE(parser.IsSet("first"));
    EXPECT_EQ(parser.Get<int>("first"), 3);
}

TEST(CommandLineParserTest, ExistAndIsSet) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "count", .need = false}, 5);

    EXPECT_TRUE(parser.Exist("count"));
    EXPECT_FALSE(parser.IsSet("count"));

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--count=3"}));

    EXPECT_TRUE(parser.Exist("count"));
    EXPECT_TRUE(parser.IsSet("count"));
}

TEST(CommandLineParserTest, ReparseResetsOptionState) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "count", .need = false}, 5);

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--count=7"}));
    ASSERT_EQ(parser.Get<int>("count"), 7);

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog"}));

    EXPECT_FALSE(parser.IsSet("count"));
    EXPECT_EQ(parser.Get<int>("count"), 5);
}

TEST(CommandLineParserTest, ReparseRequiredOptionStillRequired) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "count", .need = true}, 0);

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--count=7"}));

    EXPECT_FALSE(parser.Parse(std::vector<std::string>{"prog"}));
    EXPECT_NE(parser.Error().find("need option"), std::string::npos);
}

TEST(CommandLineParserTest, TryGetNotDefinedOrMismatch) {
    everest::CommandLineParser parser;
    parser.Add<std::string>({.name = "name", .short_name = 'n', .desc = "name", .need = false}, "x");

    int number = 7;
    EXPECT_FALSE(parser.TryGet<int>("missing", number));
    EXPECT_EQ(number, 7);

    EXPECT_FALSE(parser.TryGet<int>("name", number));
    EXPECT_EQ(number, 7);

    std::string text = "keep";
    EXPECT_TRUE(parser.TryGet<std::string>("name", text));
    EXPECT_EQ(text, "x");
}

TEST(CommandLineParserTest, TryGetCrossWidthMismatch) {
    everest::CommandLineParser parser;
    parser.Add<uint64_t>({.name = "big", .short_name = 'b', .desc = "big", .need = false}, 0);

    uint32_t out = 7;
    EXPECT_FALSE(parser.TryGet<uint32_t>("big", out));
    EXPECT_EQ(out, 7U);
}

TEST(CommandLineParserTest, GetFailsWithoutExit) {
    everest::CommandLineParser parser;
    parser.Add<std::string>({.name = "name", .short_name = 'n', .desc = "name", .need = false}, "x");

    EXPECT_EQ(parser.Get<int>("missing"), 0);
    EXPECT_FALSE(parser.Error().empty());

    EXPECT_EQ(parser.Get<int>("name"), 0);
    EXPECT_FALSE(parser.Error().empty());
}

TEST(CommandLineParserTest, ErrorOutlivesNextParse) {
    everest::CommandLineParser parser;

    ASSERT_FALSE(parser.Parse(std::vector<std::string>{"prog", "--nope"}));
    const std::string& error = parser.Error();
    ASSERT_FALSE(error.empty());

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog"}));

    EXPECT_FALSE(error.empty());
    EXPECT_NE(error.find("undefined option"), std::string::npos);
}

TEST(CommandLineParserTest, ProgramNameDerived) {
    {
        everest::CommandLineParser parser;
        const char* const argv[] = {"C:\\a\\prog.exe"};
        ASSERT_TRUE(parser.Parse(1, argv));
        EXPECT_EQ(parser.GetProgramName(), "prog");
    }

    {
        everest::CommandLineParser parser;
        const char* const argv[] = {"/a/prog"};
        ASSERT_TRUE(parser.Parse(1, argv));
        EXPECT_EQ(parser.GetProgramName(), "prog");
    }

    {
        everest::CommandLineParser parser;
        const char* const argv[] = {"prog.test"};
        ASSERT_TRUE(parser.Parse(1, argv));
        EXPECT_EQ(parser.GetProgramName(), "prog");
    }
}

TEST(CommandLineParserTest, SetProgramNameNotOverriddenByArgv) {
    everest::CommandLineParser parser;
    parser.SetProgramName("custom");

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"C:\\a\\prog.exe"}));

    EXPECT_EQ(parser.GetProgramName(), "custom");
}

TEST(CommandLineParserTest, UsageContainsOptionsAndDefault) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "item count", .need = false}, 5);

    const std::string usage = parser.Usage();

    EXPECT_NE(usage.find("--count"), std::string::npos);
    EXPECT_NE(usage.find("-c"), std::string::npos);
    EXPECT_NE(usage.find("item count"), std::string::npos);
    EXPECT_NE(usage.find("5"), std::string::npos);
}

TEST(CommandLineParserTest, ErrorFullJoinsErrors) {
    everest::CommandLineParser parser;

    ASSERT_FALSE(parser.Parse(std::vector<std::string>{"prog", "--aaa", "--bbb"}));

    const std::string full = parser.ErrorFull();
    EXPECT_NE(full.find("aaa"), std::string::npos);
    EXPECT_NE(full.find("bbb"), std::string::npos);
}

TEST(CommandLineParserTest, FooterIncludedInUsage) {
    everest::CommandLineParser parser;
    parser.Footer("extra");

    EXPECT_NE(parser.Usage().find("extra"), std::string::npos);
}

TEST(CommandLineParserTest, ParseStringQuotedValue) {
    everest::CommandLineParser parser;
    parser.Add<std::string>({.name = "name", .short_name = 'n', .desc = "name", .need = false}, "");

    ASSERT_TRUE(parser.Parse("--name \"a b\""));

    EXPECT_EQ(parser.Get<std::string>("name"), "a b");
}

TEST(CommandLineParserTest, ParseStringEscapedSpace) {
    everest::CommandLineParser parser;
    parser.Add<std::string>({.name = "name", .short_name = 'n', .desc = "name", .need = false}, "");

    ASSERT_TRUE(parser.Parse("--name a\\ b"));

    EXPECT_EQ(parser.Get<std::string>("name"), "a b");
}

TEST(CommandLineParserTest, ParseStringUnclosedQuote) {
    everest::CommandLineParser parser;
    parser.Add<std::string>({.name = "name", .short_name = 'n', .desc = "name", .need = false}, "");

    EXPECT_FALSE(parser.Parse("--name \"abc"));
    EXPECT_NE(parser.Error().find("quote is not closed"), std::string::npos);
}

TEST(CommandLineParserTest, ParseStringIgnoresEmptyTokens) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "count", .need = false}, 0);

    ASSERT_TRUE(parser.Parse("  --count 7  "));

    EXPECT_EQ(parser.Get<int>("count"), 7);
    EXPECT_TRUE(parser.Rest().empty());
}

TEST(CommandLineParserTest, ParseStringWhitespaceOnlyFails) {
    everest::CommandLineParser parser;

    EXPECT_FALSE(parser.Parse("   "));
    EXPECT_NE(parser.Error().find("argument number must be longer than 0"), std::string::npos);
}

TEST(CommandLineParserTest, ParseStringQuotedEmptyValue) {
    everest::CommandLineParser parser;
    parser.Add<std::string>({.name = "name", .short_name = 'n', .desc = "name", .need = false}, "x");

    ASSERT_TRUE(parser.Parse("--name \"\""));

    EXPECT_TRUE(parser.IsSet("name"));
    EXPECT_EQ(parser.Get<std::string>("name"), "");
}

TEST(CommandLineParserTest, ParseVectorArguments) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "count", .need = false}, 0);

    ASSERT_TRUE(parser.Parse(std::vector<std::string>{"prog", "--count", "7"}));

    EXPECT_EQ(parser.Get<int>("count"), 7);
}

TEST(CommandLineParserDeathTest, PrintValueExitsZero) {
    everest::CommandLineParser parser;
    parser.Add<std::string>({.name = "version", .short_name = 'v', .desc = "show version", .need = false}, "1.0.0");

    EXPECT_EXIT(parser.Print("version"), ::testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(CommandLineParserDeathTest, PrintUnknownExitsFailure) {
    everest::CommandLineParser parser;

    EXPECT_EXIT(parser.Print("nope"), ::testing::ExitedWithCode(EXIT_FAILURE), "");
}

TEST(CommandLineParserDeathTest, ParseCheckFailsExitsFailure) {
    everest::CommandLineParser parser;
    parser.Add<int>({.name = "count", .short_name = 'c', .desc = "count", .need = true}, 0);

    const std::vector<std::string> args{"prog"};
    EXPECT_EXIT(parser.ParseCheck(args), ::testing::ExitedWithCode(EXIT_FAILURE), "");
}

TEST(CommandLineParserDeathTest, ParseCheckHelpExitsSuccess) {
    everest::CommandLineParser parser;

    const std::vector<std::string> args{"prog", "--help"};
    EXPECT_EXIT(parser.ParseCheck(args), ::testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(CommandLineParserDeathTest, ParseCheckHelpShortExitsSuccess) {
    everest::CommandLineParser parser;

    const std::vector<std::string> args{"prog", "-?"};
    EXPECT_EXIT(parser.ParseCheck(args), ::testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(CommandLineParserDeathTest, ParseGetOrSetBehaviorGetExitsZero) {
    everest::CommandLineParser parser;
    parser.AddWithoutValueCommand({.name = "version", .short_name = 'v', .desc = "show version"});

    char program[] = "prog";
    char option[] = "--version";
    char* arguments[] = {program, option, nullptr};
    EXPECT_EXIT(parser.ParseGetOrSetBehavior(2, arguments), ::testing::ExitedWithCode(EXIT_SUCCESS), "");
}
