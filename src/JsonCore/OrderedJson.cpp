#include "OrderedJson.h"
#include "JsonHandler.h"

#include <rapidjson/memorystream.h>
#include <utility>
#include <unordered_set>

namespace jsonviewer
{
std::string quote(const std::string& value)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.String(value.data(), static_cast<rapidjson::SizeType>(value.size()));
    return {buffer.GetString(), buffer.GetSize()};
}

std::string Node::path() const
{
    if (!parent)
        return "$";
    return parent->path() + (parent->type == Type::Array
        ? "[" + std::to_string(index) + "]" : "[" + quote(key) + "]");
}

std::string Document::rawValue(const Node& node) const
{
    return source.substr(node.valueBegin, node.valueEnd - node.valueBegin);
}

std::string keySortError(const Node& node)
{
    std::unordered_set<std::string> keys;
    for (const auto& child : node.children)
    {
        if (node.type == Type::Object)
        {
            if (child->key.find('\0') != std::string::npos)
                return "Sorting keys containing NUL is not supported. The document was not changed.";
            if (!keys.insert(child->key).second)
                return "Sorting duplicate object keys is not supported. The document was not changed.";
        }
        auto error = keySortError(*child);
        if (!error.empty()) return error;
    }
    return {};
}

namespace
{
class Builder : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, Builder>
{
    Document& document;
    rapidjson::MemoryStream& stream;
    const std::size_t maxDepth;
    std::vector<Node*> stack;
    std::string key;
    std::size_t keyBegin = 0, keyEnd = 0, cursor = 0;

    // The SAX parser has already checked grammar. Locate source tokens without
    // re-encoding decoded strings, so escapes and UTF-8 byte positions stay exact.
    std::size_t nextToken() const
    {
        auto p = cursor;
        const auto& s = document.source;
        while (p < s.size())
        {
            const char c = s[p];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == ':')
                ++p;
            else if (c == '/' && p + 1 < s.size() && s[p + 1] == '/')
            {
                p += 2;
                while (p < s.size() && s[p] != '\n' && s[p] != '\r') ++p;
            }
            else if (c == '/' && p + 1 < s.size() && s[p + 1] == '*')
            {
                const auto end = s.find("*/", p + 2);
                p = end == std::string::npos ? s.size() : end + 2;
            }
            else break;
        }
        return p;
    }

    Node* append(Type type, std::size_t begin)
    {
        auto node = std::make_unique<Node>();
        node->type = type;
        node->valueBegin = begin;
        node->valueEnd = stream.Tell();
        auto* result = node.get();
        if (stack.empty()) document.root = std::move(node);
        else
        {
            auto* parent = stack.back();
            node->parent = parent;
            node->index = parent->children.size();
            if (parent->type == Type::Object)
            {
                node->key = std::move(key);
                node->keyBegin = keyBegin;
                node->keyEnd = keyEnd;
            }
            parent->children.push_back(std::move(node));
        }
        cursor = stream.Tell();
        return result;
    }

    bool scalar(Type type, std::string value)
    {
        append(type, nextToken())->value = std::move(value);
        return true;
    }

    bool begin(Type type)
    {
        if (stack.size() >= maxDepth)
        {
            document.error = "JSON nesting exceeds the viewer limit (" + std::to_string(maxDepth) + ").";
            return false;
        }
        // RapidJSON's iterative parser invokes container callbacks BEFORE
        // consuming the bracket, unlike its recursive parser.
        stack.push_back(append(type, stream.Tell()));
        cursor = stream.Tell() + 1;
        return true;
    }

    bool end()
    {
        stack.back()->valueEnd = stream.Tell() + 1;
        stack.pop_back();
        cursor = stream.Tell() + 1;
        return true;
    }

public:
    Builder(Document& doc, rapidjson::MemoryStream& input, std::size_t depth)
        : document(doc), stream(input), maxDepth(depth) {}
    bool Null() { return scalar(Type::Null, "null"); }
    bool Bool(bool value) { return scalar(Type::Boolean, value ? "true" : "false"); }
    bool RawNumber(const char* value, unsigned length, bool) { return scalar(Type::Number, {value, length}); }
    bool String(const char* value, unsigned length, bool) { return scalar(Type::String, {value, length}); }
    bool Key(const char* value, unsigned length, bool)
    {
        key.assign(value, length);
        keyBegin = nextToken();
        keyEnd = stream.Tell();
        cursor = stream.Tell();
        return true;
    }
    bool StartObject() { return begin(Type::Object); }
    bool StartArray() { return begin(Type::Array); }
    bool EndObject(unsigned) { return end(); }
    bool EndArray(unsigned) { return end(); }
};
}

Document parse(std::string source, const ParseOptions& options, std::size_t maxDepth)
{
    Document result;
    result.source = std::move(source);
    const auto nul = result.source.find('\0');
    if (nul != std::string::npos)
    {
        result.error = "Unexpected NUL byte in JSON source.";
        result.errorOffset = nul;
        return result;
    }
    rapidjson::MemoryStream stream(result.source.data(), result.source.size());
    rapidjson::Reader reader;
    Builder builder(result, stream, maxDepth);
    constexpr unsigned flags = flgBaseReader | rapidjson::kParseIterativeFlag | rapidjson::kParseValidateEncodingFlag;
    bool ok = false;
    if (options.bIgnoreComment && options.bIgnoreTrailingComma)
        ok = reader.Parse<flags | rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(stream, builder);
    else if (options.bIgnoreComment)
        ok = reader.Parse<flags | rapidjson::kParseCommentsFlag>(stream, builder);
    else if (options.bIgnoreTrailingComma)
        ok = reader.Parse<flags | rapidjson::kParseTrailingCommasFlag>(stream, builder);
    else
        ok = reader.Parse<flags>(stream, builder);
    if (!ok)
    {
        if (result.error.empty()) result.error = rapidjson::GetParseError_En(reader.GetParseErrorCode());
        result.errorOffset = reader.GetErrorOffset();
        result.root.reset();
    }
    return result;
}
}
