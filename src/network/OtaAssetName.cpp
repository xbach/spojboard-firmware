#include "OtaAssetName.h"

#include <string.h>
#include <stdlib.h>

namespace
{

const char PREFIX[] = "spojboard-";
const size_t PREFIX_LEN = sizeof(PREFIX) - 1;
const char SUFFIX[] = ".bin";
const size_t SUFFIX_LEN = sizeof(SUFFIX) - 1;

const int MAX_FIELDS = 8;

struct Field
{
    const char* begin;
    size_t len;
};

bool isDigit(char c) { return c >= '0' && c <= '9'; }

bool fieldEquals(const Field& f, const char* s)
{
    return strlen(s) == f.len && strncmp(f.begin, s, f.len) == 0;
}

/** Is this field exactly r<digits>? Writes the number out on success. */
bool isReleaseField(const Field& f, int* releaseOut)
{
    if (f.len < 2 || f.begin[0] != 'r')
        return false;
    for (size_t i = 1; i < f.len; i++)
        if (!isDigit(f.begin[i]))
            return false;
    char buf[12];
    size_t n = f.len - 1;
    if (n >= sizeof(buf))
        return false;
    memcpy(buf, f.begin + 1, n);
    buf[n] = '\0';
    *releaseOut = atoi(buf);
    return true;
}

/** A geometry token: <digits>x<digits>, e.g. 2x32. */
bool isDisplayField(const Field& f)
{
    const char* x = (const char*)memchr(f.begin, 'x', f.len);
    if (!x || x == f.begin || x == f.begin + f.len - 1)
        return false;
    for (const char* p = f.begin; p < x; p++)
        if (!isDigit(*p))
            return false;
    for (const char* p = x + 1; p < f.begin + f.len; p++)
        if (!isDigit(*p))
            return false;
    return true;
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

    // Split the part between "spojboard-" and ".bin" on dashes.
    const char* p = filename + PREFIX_LEN;
    const char* end = filename + len - SUFFIX_LEN;

    Field fields[MAX_FIELDS];
    int count = 0;
    while (p < end)
    {
        const char* dash = (const char*)memchr(p, '-', (size_t)(end - p));
        const char* stop = dash ? dash : end;
        if (count >= MAX_FIELDS)
            return info; // absurdly many fields; not a name we produce
        fields[count].begin = p;
        fields[count].len = (size_t)(stop - p);
        if (fields[count].len == 0)
            return info; // empty field, e.g. a doubled dash
        count++;
        p = dash ? dash + 1 : end;
    }

    // Minimum shape: <board> r<n> <buildid>
    if (count < 3)
        return info;

    // Field 0 is the board and must match this device exactly.
    if (!fieldEquals(fields[0], boardVariant))
        return info;

    // Field 1 is either the release marker (bare) or the display token.
    int release = -1;
    if (isReleaseField(fields[1], &release))
    {
        info.release = release;
        info.match = OtaAssetMatch::Bare;
        return info;
    }

    if (!isDisplayField(fields[1]))
        return info;
    if (count < 4 || !isReleaseField(fields[2], &release))
        return info;
    if (fields[1].len >= sizeof(info.display))
        return info;

    memcpy(info.display, fields[1].begin, fields[1].len);
    info.display[fields[1].len] = '\0';
    info.release = release;
    info.match = OtaAssetMatch::Display;
    return info;
}
