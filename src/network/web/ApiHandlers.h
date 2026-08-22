#ifndef API_HANDLERS_H
#define API_HANDLERS_H

#include <Arduino.h>
#include "../GitHubOTA.h"

/**
 * Build JSON response for GitHub update check
 * @param info Release info from GitHubOTA
 * @return JSON string
 */
String buildCheckUpdateJson(const GitHubOTA::ReleaseInfo& info, const char* currentDisplay);

#endif // API_HANDLERS_H
