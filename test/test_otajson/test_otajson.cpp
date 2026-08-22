// Regression test for the bug that made every release with four or more assets
// invisible to a device.
//
// r8 buffered the whole GitHub response and parsed it UNFILTERED into a fixed
// DynamicJsonDocument(8192). A GitHub release asset object is ~1,750 bytes of
// JSON (over 1,100 of it an embedded `uploader` object we never read), so the
// document grew ~1,248 bytes per asset. r8's own two-asset release already used
// 6,772 of its 8,192 bytes. At four assets deserializeJson returns NoMemory and
// the update check fails with "Failed to parse GitHub response" -- before ever
// looking at an asset name, so no naming scheme can rescue it.
//
// The fix is the Filter, not a bigger number: filtered, the document grows
// ~256 bytes per asset instead of ~1,248.
#include <unity.h>
#include <ArduinoJson.h>
#include <string>

// One asset object shaped like GitHub's real one, including the `uploader`
// blob that dominates its size and that we never read.
static std::string assetObject(const char* name)
{
    std::string uploader =
        "\"uploader\":{\"login\":\"xbach\",\"id\":12345678,"
        "\"node_id\":\"MDQ6VXNlcjEyMzQ1Njc4AAAAAAAAAAAAAAAAAAAAAAAAAAAA\","
        "\"avatar_url\":\"https://avatars.githubusercontent.com/u/12345678?v=4\","
        "\"gravatar_id\":\"\",\"url\":\"https://api.github.com/users/xbach\","
        "\"html_url\":\"https://github.com/xbach\","
        "\"followers_url\":\"https://api.github.com/users/xbach/followers\","
        "\"following_url\":\"https://api.github.com/users/xbach/following{/other_user}\","
        "\"gists_url\":\"https://api.github.com/users/xbach/gists{/gist_id}\","
        "\"starred_url\":\"https://api.github.com/users/xbach/starred{/owner}{/repo}\","
        "\"subscriptions_url\":\"https://api.github.com/users/xbach/subscriptions\","
        "\"organizations_url\":\"https://api.github.com/users/xbach/orgs\","
        "\"repos_url\":\"https://api.github.com/users/xbach/repos\","
        "\"events_url\":\"https://api.github.com/users/xbach/events{/privacy}\","
        "\"received_events_url\":\"https://api.github.com/users/xbach/received_events\","
        "\"type\":\"User\",\"user_view_type\":\"public\",\"site_admin\":false},";

    std::string s = "{";
    s += "\"url\":\"https://api.github.com/repos/xbach/spojboard-firmware/releases/assets/456645243\",";
    s += "\"id\":456645243,\"node_id\":\"RA_kwDOQ2q1ss4bN9p7\",";
    s += std::string("\"name\":\"") + name + "\",\"label\":\"\",";
    s += uploader;
    s += "\"content_type\":\"application/octet-stream\",\"state\":\"uploaded\",";
    s += "\"size\":1243360,";
    s += "\"digest\":\"sha256:0d7e8e559af5406b7dc13e06ad6a430b9e20b6b899d512440dd5c0d0f38153bd\",";
    s += "\"download_count\":2,\"created_at\":\"2026-06-24T13:07:21Z\",";
    s += "\"updated_at\":\"2026-06-24T13:07:21Z\",";
    s += std::string("\"browser_download_url\":\"https://github.com/xbach/spojboard-firmware/releases/download/r10/") + name + "\"";
    s += "}";
    return s;
}

static std::string releasePayload(int assetCount, int notesBytes)
{
    std::string s = "{\"tag_name\":\"r10\",\"name\":\"Release 10\",\"body\":\"";
    for (int i = 0; i < notesBytes; i++) s += 'x';
    s += "\",\"draft\":false,\"prerelease\":false,\"assets\":[";
    for (int i = 0; i < assetCount; i++)
    {
        if (i) s += ",";
        char name[80];
        snprintf(name, sizeof(name), "spojboard-matrixportal_s3_%dx32-r10-1a2b3c4d.bin", i + 2);
        s += assetObject(name);
    }
    s += "]}";
    return s;
}

static void buildFilter(StaticJsonDocument<512>& filter)
{
    filter["tag_name"] = true;
    filter["name"] = true;
    filter["body"] = true;
    filter["assets"][0]["name"] = true;
    filter["assets"][0]["browser_download_url"] = true;
    filter["assets"][0]["size"] = true;
}

// Documents the bug exactly as it shipped. If this ever starts passing, the
// fixture has drifted from GitHub's real payload shape and the numbers below
// need re-measuring against a live response.
void test_r8_unfiltered_8k_breaks_after_a_handful_of_assets(void)
{
    // Find the first asset count an unfiltered 8KB document cannot handle.
    int firstFailure = 0;
    for (int n = 2; n <= 20; n++)
    {
        DynamicJsonDocument doc(8192);
        if (deserializeJson(doc, releasePayload(n, 880)) == DeserializationError::NoMemory)
        {
            firstFailure = n;
            break;
        }
    }

    // Against REAL GitHub payloads (asset object ~1,754 bytes) this is 4; the
    // fixture above is structurally faithful but ~15% smaller, so it breaks a
    // little later. The assertion is deliberately about the order of magnitude
    // rather than the exact number, so it stays true as GitHub's asset object
    // shape drifts -- what must never change is that it is a SMALL number.
    TEST_ASSERT_TRUE(firstFailure > 0);
    TEST_ASSERT_TRUE(firstFailure <= 6);
}

// A filter document that overflows drops its trailing keys silently, and those
// fields then read as null on every asset. Guard it explicitly.
void test_filter_document_does_not_overflow(void)
{
    StaticJsonDocument<512> filter;
    buildFilter(filter);
    TEST_ASSERT_FALSE(filter.overflowed());
}

// The fix: same payloads, filtered, into the shipping 12KB buffer.
void test_filtered_12k_survives_a_full_variant_matrix(void)
{
    StaticJsonDocument<512> filter;
    buildFilter(filter);

    for (int n : {2, 4, 8, 12, 16})
    {
        DynamicJsonDocument doc(12288);
        DeserializationError err =
            deserializeJson(doc, releasePayload(n, 880), DeserializationOption::Filter(filter));
        TEST_ASSERT_FALSE(err);
        TEST_ASSERT_EQUAL_INT(n, doc["assets"].as<JsonArray>().size());
        TEST_ASSERT_EQUAL_STRING("r10", doc["tag_name"]);
    }
}

// Release notes are the other input that can move the floor, so pin them too.
void test_filtered_12k_survives_a_long_changelog(void)
{
    StaticJsonDocument<512> filter;
    buildFilter(filter);

    DynamicJsonDocument doc(12288);
    DeserializationError err =
        deserializeJson(doc, releasePayload(16, 4000), DeserializationOption::Filter(filter));
    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_EQUAL_INT(16, doc["assets"].as<JsonArray>().size());
}

// The filter must keep every field findBinaryAsset and checkForUpdate read.
void test_filter_keeps_all_consumed_fields(void)
{
    StaticJsonDocument<512> filter;
    buildFilter(filter);

    DynamicJsonDocument doc(12288);
    TEST_ASSERT_FALSE(deserializeJson(doc, releasePayload(2, 100), DeserializationOption::Filter(filter)));

    TEST_ASSERT_NOT_NULL((const char*)doc["tag_name"]);
    TEST_ASSERT_NOT_NULL((const char*)doc["name"]);
    TEST_ASSERT_NOT_NULL((const char*)doc["body"]);
    JsonObject a = doc["assets"][0];
    TEST_ASSERT_NOT_NULL((const char*)a["name"]);
    TEST_ASSERT_NOT_NULL((const char*)a["browser_download_url"]);
    TEST_ASSERT_TRUE((a["size"] | 0) > 0);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_r8_unfiltered_8k_breaks_after_a_handful_of_assets);
    RUN_TEST(test_filter_document_does_not_overflow);
    RUN_TEST(test_filtered_12k_survives_a_full_variant_matrix);
    RUN_TEST(test_filtered_12k_survives_a_long_changelog);
    RUN_TEST(test_filter_keeps_all_consumed_fields);
    return UNITY_END();
}
