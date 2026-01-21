#include "WebUtils.h"

int countStops(const char* stopIds)
{
    if (!stopIds || stopIds[0] == '\0')
    {
        return 0;
    }

    int count = 1; // At least one stop if string is not empty
    for (const char* p = stopIds; *p; p++)
    {
        if (*p == ',')
        {
            count++;
        }
    }
    return count;
}

String escapeJsonString(const char* str)
{
    String result = "";
    if (!str)
        return result;

    for (size_t i = 0; str[i] != '\0'; i++)
    {
        char c = str[i];
        switch (c)
        {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        default:
            // Skip other control characters
            if (c >= 0 && c < 32)
            {
                // Skip control characters
            }
            else
            {
                result += c;
            }
            break;
        }
    }
    return result;
}
