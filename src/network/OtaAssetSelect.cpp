#include "OtaAssetSelect.h"
#include "OtaAssetName.h"
#include <string.h>

namespace
{
// strlcpy is a BSD extension and is not present on every host toolchain this
// builds on natively; the firmware's Arduino core does provide it.
void copyField(char* dst, size_t cap, const char* src)
{
    if (cap == 0)
    {
        return;
    }
    strncpy(dst, src ? src : "", cap - 1);
    dst[cap - 1] = '\0';
}
} // namespace

int otaCollectAssetOptions(JsonDocument& doc, const char* board, OtaAssetOption* out, int maxOptions)
{
    JsonArray assets = doc["assets"];
    if (assets.isNull())
    {
        return 0;
    }

    int count = 0;
    for (int pass = 0; pass < 2 && count < maxOptions; pass++)
    {
        const OtaAssetMatch want = (pass == 0) ? OtaAssetMatch::Display : OtaAssetMatch::Bare;

        for (JsonObject asset : assets)
        {
            if (count >= maxOptions)
            {
                break;
            }

            const char* name = asset["name"];
            const char* url = asset["browser_download_url"];
            int size = asset["size"] | 0;

            if (!name || !url || size <= 0)
            {
                continue;
            }

            OtaAssetInfo info = otaClassifyAsset(name, board);
            if (info.match != want)
            {
                continue;
            }

            copyField(out[count].name, sizeof(out[count].name), name);
            copyField(out[count].url, sizeof(out[count].url), url);
            copyField(out[count].display, sizeof(out[count].display), info.display);
            out[count].size = (size_t)size;
            count++;
        }
    }

    return count;
}
