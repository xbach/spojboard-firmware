#include "DisplayColors.h"
#include <string.h>

// ============================================================================
// Color Constants (initialized by initColors)
// ============================================================================

uint16_t COLOR_WHITE;
uint16_t COLOR_YELLOW;
uint16_t COLOR_RED;
uint16_t COLOR_GREEN;
uint16_t COLOR_BLUE;
uint16_t COLOR_ORANGE;
uint16_t COLOR_PURPLE;
uint16_t COLOR_BLACK;
uint16_t COLOR_CYAN;

// ============================================================================
// Color Initialization
// ============================================================================

void initColors(MatrixPanel_I2S_DMA* display)
{
    COLOR_WHITE = display->color565(255, 255, 255);
    COLOR_YELLOW = display->color565(255, 255, 0);
    COLOR_RED = display->color565(255, 0, 0);
    COLOR_GREEN = display->color565(0, 255, 0);
    COLOR_BLUE = display->color565(0, 0, 255);
    COLOR_ORANGE = display->color565(255, 165, 0);
    COLOR_PURPLE = display->color565(128, 0, 128);
    COLOR_BLACK = display->color565(0, 0, 0);
    COLOR_CYAN = display->color565(0, 255, 255);
}

// ============================================================================
// Color Name Parsing
// ============================================================================

uint16_t parseColorName(const char* colorName)
{
    if (!colorName)
        return 0;

    // Case-insensitive comparison
    if (strcasecmp(colorName, "RED") == 0)
        return COLOR_RED;
    if (strcasecmp(colorName, "GREEN") == 0)
        return COLOR_GREEN;
    if (strcasecmp(colorName, "BLUE") == 0)
        return COLOR_BLUE;
    if (strcasecmp(colorName, "YELLOW") == 0)
        return COLOR_YELLOW;
    if (strcasecmp(colorName, "ORANGE") == 0)
        return COLOR_ORANGE;
    if (strcasecmp(colorName, "PURPLE") == 0)
        return COLOR_PURPLE;
    if (strcasecmp(colorName, "CYAN") == 0)
        return COLOR_CYAN;
    if (strcasecmp(colorName, "WHITE") == 0)
        return COLOR_WHITE;

    return 0; // Invalid color name
}

// ============================================================================
// Configurable Line Colors with Pattern Matching
// ============================================================================

// Parse a pattern string into prefix length, required wildcards (*), and optional wildcards (?)
// Returns false if pattern is invalid
static bool parsePattern(const char* pattern, size_t patternLen, size_t& prefixLen, int& asteriskCount,
                         int& questionCount)
{
    asteriskCount = 0;
    questionCount = 0;
    prefixLen = 0;

    // Scan from end: first count '?' chars, then '*' chars, rest is prefix
    int i = patternLen - 1;

    // Count trailing '?' (optional positions)
    while (i >= 0 && pattern[i] == '?')
    {
        questionCount++;
        i--;
    }

    // Count '*' before the '?' block (required positions)
    while (i >= 0 && pattern[i] == '*')
    {
        asteriskCount++;
        i--;
    }

    prefixLen = (i >= 0) ? (size_t)(i + 1) : 0;

    // Must have at least one '*'
    if (asteriskCount == 0)
        return false;

    // No wildcards allowed in prefix
    for (size_t j = 0; j < prefixLen; j++)
    {
        if (pattern[j] == '*' || pattern[j] == '?')
            return false;
    }

    return true;
}

// Check if a line matches a parsed pattern
static bool matchesPattern(const char* line, size_t lineLen, const char* prefix, size_t prefixLen, int asteriskCount,
                           int questionCount)
{
    size_t minLen = prefixLen + asteriskCount;
    size_t maxLen = prefixLen + asteriskCount + questionCount;

    if (lineLen < minLen || lineLen > maxLen)
        return false;

    // Check prefix match
    if (prefixLen > 0 && strncasecmp(line, prefix, prefixLen) != 0)
        return false;

    return true;
}

uint16_t getLineColorWithConfig(const char* line, const char* configMap)
{
    if (!line)
        return COLOR_WHITE;

    if (!configMap || strlen(configMap) == 0)
        return COLOR_WHITE;

    size_t lineLen = strlen(line);
    char mapCopy[256];

    // Pass 1: Exact matches
    strlcpy(mapCopy, configMap, sizeof(mapCopy));
    char* token = strtok(mapCopy, ",");
    while (token != nullptr)
    {
        char* equals = strchr(token, '=');
        if (equals)
        {
            *equals = '\0';
            const char* configLine = token;
            const char* colorName = equals + 1;

            size_t configLineLen = strlen(configLine);
            bool hasWildcard = false;
            for (size_t i = 0; i < configLineLen; i++)
            {
                if (configLine[i] == '*' || configLine[i] == '?')
                {
                    hasWildcard = true;
                    break;
                }
            }

            if (!hasWildcard && strcasecmp(line, configLine) == 0)
            {
                uint16_t color = parseColorName(colorName);
                if (color != 0)
                    return color;
            }
        }
        token = strtok(nullptr, ",");
    }

    // Pass 2: Fixed-length patterns (* only, no ?)
    strlcpy(mapCopy, configMap, sizeof(mapCopy));
    token = strtok(mapCopy, ",");
    while (token != nullptr)
    {
        char* equals = strchr(token, '=');
        if (equals)
        {
            *equals = '\0';
            const char* configLine = token;
            const char* colorName = equals + 1;

            size_t configLineLen = strlen(configLine);
            size_t prefixLen;
            int asteriskCount, questionCount;

            if (configLineLen > 0 && parsePattern(configLine, configLineLen, prefixLen, asteriskCount, questionCount))
            {
                if (questionCount == 0) // Fixed-length only in this pass
                {
                    if (matchesPattern(line, lineLen, configLine, prefixLen, asteriskCount, questionCount))
                    {
                        uint16_t color = parseColorName(colorName);
                        if (color != 0)
                            return color;
                    }
                }
            }
        }
        token = strtok(nullptr, ",");
    }

    // Pass 3: Flexible patterns (with ?)
    strlcpy(mapCopy, configMap, sizeof(mapCopy));
    token = strtok(mapCopy, ",");
    while (token != nullptr)
    {
        char* equals = strchr(token, '=');
        if (equals)
        {
            *equals = '\0';
            const char* configLine = token;
            const char* colorName = equals + 1;

            size_t configLineLen = strlen(configLine);
            size_t prefixLen;
            int asteriskCount, questionCount;

            if (configLineLen > 0 && parsePattern(configLine, configLineLen, prefixLen, asteriskCount, questionCount))
            {
                if (questionCount > 0) // Flexible patterns only in this pass
                {
                    if (matchesPattern(line, lineLen, configLine, prefixLen, asteriskCount, questionCount))
                    {
                        uint16_t color = parseColorName(colorName);
                        if (color != 0)
                            return color;
                    }
                }
            }
        }
        token = strtok(nullptr, ",");
    }

    return COLOR_WHITE; // Absolute fallback
}
