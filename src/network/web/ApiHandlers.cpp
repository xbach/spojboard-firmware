#include "ApiHandlers.h"
#include "WebUtils.h"

String buildCheckUpdateJson(const GitHubOTA::ReleaseInfo& info, const char* currentDisplay)
{
    String json;
    // Built by repeated += with a String temporary per escaped field. With a
    // full option list that is dozens of reallocations on a heap this firmware
    // keeps for weeks; reserve once instead.
    json.reserve(1024 + (size_t)info.optionCount * 384);
    json = "{";

    if (info.hasError)
    {
        json += "\"available\":false,";
        json += "\"error\":\"" + escapeJsonString(info.errorMsg) + "\"";
    }
    else if (info.available)
    {
        json += "\"available\":true,";
        json += "\"releaseNumber\":" + String(info.releaseNumber) + ",";
        json += "\"releaseName\":\"" + escapeJsonString(info.releaseName) + "\",";
        json += "\"releaseNotes\":\"" + escapeJsonString(info.releaseNotes) + "\",";
        json += "\"fileName\":\"" + escapeJsonString(info.assetName) + "\",";
        json += "\"fileSize\":" + String(info.assetSize) + ",";
        json += "\"assetUrl\":\"" + escapeJsonString(info.assetUrl) + "\",";

        // The geometry this device is currently configured for, so the client
        // can preselect the matching build. Empty when unknown.
        json += "\"currentDisplay\":\"" + escapeJsonString(currentDisplay ? currentDisplay : "") + "\",";

        // Every build in this release for this board. One entry means there is
        // nothing to choose and the UI installs it directly; more than one means
        // the release ships per-geometry builds and the user picks.
        json += "\"options\":[";
        for (int i = 0; i < info.optionCount; i++)
        {
            if (i)
            {
                json += ",";
            }
            json += "{\"name\":\"" + escapeJsonString(info.options[i].name) + "\",";
            json += "\"url\":\"" + escapeJsonString(info.options[i].url) + "\",";
            json += "\"display\":\"" + escapeJsonString(info.options[i].display) + "\",";
            json += "\"size\":" + String(info.options[i].size) + "}";
        }
        json += "]";
    }
    else
    {
        json += "\"available\":false";
    }

    json += "}";

    return json;
}
