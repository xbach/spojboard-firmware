#include "OtaAssetName.h"

#include <string.h>
#include <stdlib.h>

namespace
{

const char PREFIX[] = "spojboard-";
const size_t PREFIX_LEN = sizeof(PREFIX) - 1;
const char SUFFIX[] = ".bin";
const size_t SUFFIX_LEN = sizeof(SUFFIX) - 1;

bool isDigit(char c) { return c >= '0' && c <= '9'; }

/**
 * Is `s` (length `len`) a display token, i.e. <digits>x<digits>?
 * "2x32" yes, "2x" no, "x32" no, "n8r2" no, "12x128" yes.
 */
bool isDisplayToken(const char* s, size_t len)
{
    const char* x = (const char*)memchr(s, 'x', len);
    if (!x || x == s || x == s + len - 1)
        return false;
    for (const char* p = s; p < x; p++)
        if (!isDigit(*p))
            return false;
    for (const char* p = x + 1; p < s + len; p++)
        if (!isDigit(*p))
            return false;
    return true;
}

/**
 * Find the release marker "-r<digits>-" scanning RIGHT TO LEFT within
 * [begin, end). Returns a pointer to the '-' that starts it, or nullptr.
 * On success *releaseOut holds the parsed number.
 */
const char* findReleaseMarker(const char* begin, const char* end, int* releaseOut)
{
    for (const char* p = end - 2; p >= begin; p--)
    {
        if (p[0] != '-' || p[1] != 'r')
            continue;
        const char* d = p + 2;
        if (!isDigit(*d))
            continue;
        const char* q = d;
        while (q < end && isDigit(*q))
            q++;
        // Must be followed by the '-' that separates it from the build id.
        if (q >= end || *q != '-')
            continue;
        *releaseOut = atoi(d);
        return p;
    }
    return nullptr;
}

} // namespace

OtaAssetInfo otaClassifyAsset(const char* filename, const char* boardVariant)
{
    OtaAssetInfo info;
    info.match = OtaAssetMatch::None;
    info.display[0] = '\0';
    info.release = -1;

    if (!filename || !boardVariant || !*boardVariant)
        return info;

    const size_t len = strlen(filename);
    if (len <= PREFIX_LEN + SUFFIX_LEN)
        return info;
    if (strncmp(filename, PREFIX, PREFIX_LEN) != 0)
        return info;
    if (strcmp(filename + len - SUFFIX_LEN, SUFFIX) != 0)
        return info;

    const char* tokenBegin = filename + PREFIX_LEN;
    const char* tail = filename + len - SUFFIX_LEN;

    int release = -1;
    const char* marker = findReleaseMarker(tokenBegin, tail, &release);
    if (!marker)
        return info;

    // token = "<board>" or "<board>_<display>"
    const size_t tokenLen = (size_t)(marker - tokenBegin);
    const size_t boardLen = strlen(boardVariant);
    if (tokenLen < boardLen)
        return info;
    if (strncmp(tokenBegin, boardVariant, boardLen) != 0)
        return info;

    info.release = release;

    if (tokenLen == boardLen)
    {
        info.match = OtaAssetMatch::Bare;
        return info;
    }

    // Anything after the board must be exactly "_<display>".
    const char* rest = tokenBegin + boardLen;
    const size_t restLen = tokenLen - boardLen;
    if (rest[0] != '_' || restLen < 2)
    {
        info.release = -1;
        return info; // e.g. board "esp32_s3" vs token "esp32_s3_n8r2" -> not ours
    }

    const char* disp = rest + 1;
    const size_t dispLen = restLen - 1;
    if (dispLen >= sizeof(info.display) || !isDisplayToken(disp, dispLen))
    {
        info.release = -1;
        return info;
    }

    memcpy(info.display, disp, dispLen);
    info.display[dispLen] = '\0';
    info.match = OtaAssetMatch::Display;
    return info;
}
