#pragma once

#include "ParseOptions.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace jsonviewer
{
enum class Type { Object, Array, String, Number, Boolean, Null };

struct Node
{
    Type type = Type::Null;
    std::string key;
    std::string value; // Decoded strings; original numeric spelling.
    std::size_t valueBegin = 0;
    std::size_t valueEnd = 0;
    std::size_t keyBegin = 0;
    std::size_t keyEnd = 0;
    std::size_t index = 0;
    Node* parent = nullptr;
    std::vector<std::unique_ptr<Node>> children; // Always in source order.

    bool isContainer() const { return type == Type::Object || type == Type::Array; }
    std::string path() const;
};

struct Document
{
    std::string source;
    std::unique_ptr<Node> root;
    std::size_t errorOffset = 0;
    std::string error;
    explicit operator bool() const { return root != nullptr && error.empty(); }
    std::string rawValue(const Node& node) const;
};

// Uses the upstream RapidJSON fork and parser flags; never round-trips through
// QJsonObject (which sorts keys), floating-point numbers, or a map.
Document parse(std::string source, const ParseOptions& options = {}, std::size_t maxDepth = 512);
std::string quote(const std::string& value);
// The upstream DOM sorter looks up members by C string; guard cases where
// lookup would lose information. Viewing/formatting still accepts these keys.
std::string keySortError(const Node& node);
}
