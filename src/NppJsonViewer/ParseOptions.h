#pragma once

// Parser policy shared by both hosts. Keep this header free of UI/host APIs.
struct ParseOptions
{
    bool bIgnoreComment       = true;
    bool bIgnoreTrailingComma = true;
    bool bReplaceUndefined    = false;
};
