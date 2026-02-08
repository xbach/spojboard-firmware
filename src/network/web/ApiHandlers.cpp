#include "ApiHandlers.h"
#include "WebUtils.h"

String buildCheckUpdateJson(const GitHubOTA::ReleaseInfo& info)
{
    String json = "{";

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
        json += "\"assetUrl\":\"" + escapeJsonString(info.assetUrl) + "\"";
    }
    else
    {
        json += "\"available\":false";
    }

    json += "}";

    return json;
}
