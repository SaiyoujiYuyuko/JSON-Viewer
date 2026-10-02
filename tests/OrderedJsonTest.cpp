#include "OrderedJson.h"
#include "JsonHandler.h"
#include <gtest/gtest.h>

using namespace jsonviewer;

TEST(OrderedJson, PreservesObjectOrderAtEveryLevel)
{
    auto doc = parse(R"({"type":2,"startTime":"t","forcastType":3,"resData":[{"resCode":"x","resName":"水库","V0":6885,"realData":[],"pastData":[],"forcastData":[1,2,3]}],"wainData":[],"wagaData":[],"stData":[],"regData":[]})");
    ASSERT_TRUE(doc) << doc.error;
    const std::vector<std::string> keys{"type","startTime","forcastType","resData","wainData","wagaData","stData","regData"};
    ASSERT_EQ(doc.root->children.size(), keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) EXPECT_EQ(doc.root->children[i]->key, keys[i]);
    const auto& object = *doc.root->children[3]->children[0];
    const std::vector<std::string> nested{"resCode","resName","V0","realData","pastData","forcastData"};
    ASSERT_EQ(object.children.size(), nested.size());
    for (std::size_t i = 0; i < nested.size(); ++i) EXPECT_EQ(object.children[i]->key, nested[i]);
    EXPECT_EQ(object.children[5]->children.size(), 3);
}

TEST(OrderedJson, ArraysRetainNumericOrderBeyondTenAndKeepValueTypes)
{
    auto doc = parse(R"([0,1,2,3,4,5,6,7,8,9,10,11,{"z":false,"a":null},["nested"]])");
    ASSERT_TRUE(doc) << doc.error;
    ASSERT_EQ(doc.root->children.size(), 14);
    for (int i = 0; i < 12; ++i)
    {
        EXPECT_EQ(doc.root->children[i]->value, std::to_string(i));
        EXPECT_EQ(doc.root->children[i]->path(), "$[" + std::to_string(i) + "]");
    }
    EXPECT_EQ(doc.root->children[12]->children[0]->type, Type::Boolean);
    EXPECT_EQ(doc.root->children[12]->children[1]->type, Type::Null);
    EXPECT_EQ(doc.root->children[13]->children[0]->path(), "$[13][0]");
}

TEST(OrderedJson, EmptyAndDuplicateKeysAreNotLost)
{
    auto doc = parse(R"({"":{},"a":1,"a":2,"list":[],"zero":null})");
    ASSERT_TRUE(doc) << doc.error;
    ASSERT_EQ(doc.root->children.size(), 5);
    EXPECT_EQ(doc.root->children[0]->key, "");
    EXPECT_EQ(doc.root->children[0]->type, Type::Object);
    EXPECT_TRUE(doc.root->children[0]->children.empty());
    EXPECT_EQ(doc.root->children[1]->value, "1");
    EXPECT_EQ(doc.root->children[2]->value, "2");
    EXPECT_EQ(doc.root->children[3]->type, Type::Array);
    EXPECT_EQ(doc.root->children[0]->path(), "$[\"\"]");
}

TEST(OrderedJson, ExactNumericSpellingSurvivesTreeAndUpstreamFormatting)
{
    const std::string input = R"({"big":18446744073709551616001,"fraction":0.12345678901234567890123456789,"exp":1.23400e+123,"negative":-0})";
    auto doc = parse(input);
    ASSERT_TRUE(doc) << doc.error;
    EXPECT_EQ(doc.root->children[0]->value, "18446744073709551616001");
    EXPECT_EQ(doc.root->children[1]->value, "0.12345678901234567890123456789");
    EXPECT_EQ(doc.rawValue(*doc.root->children[2]), "1.23400e+123");
    JsonHandler handler({});
    const auto formatted = handler.FormatJson(input, LE::kLf, LF::kFormatDefault, ' ', 2);
    ASSERT_TRUE(formatted.success) << formatted.error_str;
    auto after = parse(formatted.response);
    ASSERT_TRUE(after);
    for (std::size_t i = 0; i < 4; ++i) EXPECT_EQ(after.root->children[i]->value, doc.root->children[i]->value);
}

TEST(OrderedJson, SourceSpansHandleChineseEscapesCommentsAndContainers)
{
    const std::string source = R"(/*前言*/{"中文\"key": /* value */ "你好\n\u4e2d", "a.b": [ true, {"x":null} ], "": "\u0000"})";
    auto doc = parse(source);
    ASSERT_TRUE(doc) << doc.error;
    const auto& first = *doc.root->children[0];
    EXPECT_EQ(source.substr(first.keyBegin, first.keyEnd - first.keyBegin), R"("中文\"key")");
    EXPECT_EQ(first.value, "你好\n中");
    EXPECT_EQ(doc.rawValue(first), R"("你好\n\u4e2d")");
    EXPECT_EQ(first.path(), R"($["中文\"key"])");
    EXPECT_EQ(doc.rawValue(*doc.root->children[1]), R"([ true, {"x":null} ])");
    EXPECT_EQ(doc.rawValue(*doc.root->children[1]->children[1]), R"({"x":null})");
    EXPECT_EQ(doc.root->children[2]->value.size(), 1);
    EXPECT_EQ(doc.root->children[2]->value[0], '\0');
}

TEST(OrderedJson, AcceptsAllRootTypes)
{
    for (const auto* input : {"null", "true", "12345678901234567890", "\"中文\"", "{}", "[]"})
    {
        auto doc = parse(input);
        ASSERT_TRUE(doc) << input << ": " << doc.error;
        EXPECT_EQ(doc.rawValue(*doc.root), input);
        EXPECT_EQ(doc.root->path(), "$");
    }
}

TEST(OrderedJson, ParsePolicyAndErrorOffsets)
{
    EXPECT_TRUE(parse("{/* comment */\"z\":1,}"));
    ParseOptions strict;
    strict.bIgnoreComment = strict.bIgnoreTrailingComma = false;
    EXPECT_FALSE(parse("{/* comment */\"z\":1}", strict));
    EXPECT_FALSE(parse("{\"z\":1,}", strict));
    auto invalid = parse("{\"z\": }", strict);
    EXPECT_FALSE(invalid);
    EXPECT_FALSE(invalid.root);
    EXPECT_EQ(invalid.errorOffset, 6);
    EXPECT_FALSE(parse("{} trailing"));
    EXPECT_FALSE(parse(std::string("{}\0garbage", 10)));
    EXPECT_FALSE(parse("[[[[[]]]]]", {}, 4));
    EXPECT_TRUE(parse("[[[[[]]]]]", {}, 5));
}

TEST(OrderedJson, LeadingAndTrailingTriviaExcludedFromRawRoot)
{
    auto doc = parse(" \r\n /* hello */ [1,2] // end");
    ASSERT_TRUE(doc) << doc.error;
    EXPECT_EQ(doc.rawValue(*doc.root), "[1,2]");
    auto scalar = parse(" \t/* a */ -1.250e+02 // b");
    ASSERT_TRUE(scalar) << scalar.error;
    EXPECT_EQ(scalar.rawValue(*scalar.root), "-1.250e+02");
}

TEST(OrderedJson, UpstreamKeySortGuardRejectsAmbiguousKeysWithoutRejectingViewing)
{
    for (const auto* source : {R"({"a":1,"a":2})", R"([{"x":{"a\u0000b":1}}])"})
    {
        auto doc = parse(source);
        ASSERT_TRUE(doc) << doc.error;
        EXPECT_FALSE(keySortError(*doc.root).empty());
        EXPECT_EQ(doc.rawValue(*doc.root), source);
    }
    auto safe = parse(R"({"z":[{"a":1},{"a":2}],"":0,"a":3})");
    ASSERT_TRUE(safe);
    EXPECT_TRUE(keySortError(*safe.root).empty());
}
