#pragma once
#include <Arduino.h>
#include "comms/protocol/Commands.h"

enum class ParseResult : uint8_t
{
    OK,
    BAD_FORMAT,
    TOO_LONG,
};

class CommandParser
{
private:
    // Copy src into dst only if it fits (including '\0'). Never truncates.
    static bool copyField(char *dst, size_t dstSize, const String &src)
    {
        if (src.length() >= dstSize)
            return false;
        memcpy(dst, src.c_str(), src.length() + 1);
        return true;
    }

public:
    // Formats: "module:action", "module:action:key", "module:action:key:value"
    // Everything after the 3rd colon is the value, colons included.
    static ParseResult parse(String input, Command &out)
    {
        out = Command{}; // start clean, even if the caller reuses a struct

        while (input.endsWith("\r") || input.endsWith("\n"))
            input.remove(input.length() - 1);

        int c1 = input.indexOf(':');
        if (c1 <= 0)
            return ParseResult::BAD_FORMAT;

        int c2 = input.indexOf(':', c1 + 1);
        int c3 = (c2 == -1) ? -1 : input.indexOf(':', c2 + 1);

        String moduleStr = input.substring(0, c1);
        String actionStr = (c2 == -1) ? input.substring(c1 + 1) : input.substring(c1 + 1, c2);
        String keyStr = "";
        String valueStr = "";

        if (c2 != -1)
            keyStr = (c3 == -1) ? input.substring(c2 + 1) : input.substring(c2 + 1, c3);
        if (c3 != -1)
            valueStr = input.substring(c3 + 1);

        if (actionStr.length() == 0)
            return ParseResult::BAD_FORMAT;

        if (!copyField(out.module, sizeof(out.module), moduleStr) ||
            !copyField(out.action, sizeof(out.action), actionStr) ||
            !copyField(out.key, sizeof(out.key), keyStr) ||
            !copyField(out.value, sizeof(out.value), valueStr))
            return ParseResult::TOO_LONG;

        return ParseResult::OK;
    }

    static const char *errorText(ParseResult r)
    {
        switch (r)
        {
        case ParseResult::BAD_FORMAT:
            return "error:format";
        case ParseResult::TOO_LONG:
            return "error:too_long";
        default:
            return "ok";
        }
    }
};
