#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#define URL "http://swopenAPI.seoul.go.kr/api/subway/YOUR_SUBWAY_API_KEY/json/realtimeStationArrival/0/5/%EC%96%91%EC%9E%AC"
#define TARGET_SUBWAY_ID 1003

struct MemoryStruct {
    char *memory;
    size_t size;
};

typedef struct {
    int valid;
    int barvlDt;
    int arvlCd;
    char recptnDt[32];
    char updnLine[32];
    char arvlMsg2[128];
    char trainLineNm[128];
} TrainInfo;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        fprintf(stderr, "not enough memory\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = '\0';

    return realsize;
}

static int fetch_json(const char *url, struct MemoryStruct *chunk)
{
    CURL *curl_handle;
    CURLcode res;

    chunk->memory = malloc(1);
    chunk->size = 0;

    if (!chunk->memory) {
        fprintf(stderr, "malloc failed\n");
        return -1;
    }

    curl_handle = curl_easy_init();
    if (!curl_handle) {
        fprintf(stderr, "curl init failed\n");
        free(chunk->memory);
        return -1;
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 5L);

    res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl_handle);
        free(chunk->memory);
        return -1;
    }

    curl_easy_cleanup(curl_handle);
    return 0;
}

static int get_json_string(cJSON *obj, const char *key, char *out, size_t out_size)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return -1;
    }

    snprintf(out, out_size, "%s", item->valuestring);
    return 0;
}

static int get_json_int(cJSON *obj, const char *key, int *out)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    const char *p;
    char *endp = NULL;
    long v;

    if (item == NULL || out == NULL) {
        return -1;
    }

    if (cJSON_IsNumber(item)) {
        *out = item->valueint;
        return 0;
    }

    if (!cJSON_IsString(item) || item->valuestring == NULL || item->valuestring[0] == '\0') {
        return -1;
    }

    p = item->valuestring;
    while (*p != '\0') {
        if (!isdigit((unsigned char)*p)) {
            return -1;
        }
        p++;
    }

    v = strtol(item->valuestring, &endp, 10);
    if (*endp != '\0' || v < 0 || v > INT_MAX) {
        return -1;
    }

    *out = (int)v;
    return 0;
}

static int parse_best_train(const char *json_str, TrainInfo *best)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        fprintf(stderr, "JSON parse failed\n");
        return -1;
    }

    cJSON *list = cJSON_GetObjectItemCaseSensitive(root, "realtimeArrivalList");
    if (!cJSON_IsArray(list)) {
        fprintf(stderr, "realtimeArrivalList not found\n");
        cJSON_Delete(root);
        return -1;
    }

    int min_barvl = INT_MAX;
    cJSON *item = NULL;

    cJSON_ArrayForEach(item, list) {
        int sec = -1;
        int arvl_cd = -1;
        int subway_id = -1;
        char updn_line[32] = {0};

        if (get_json_string(item, "updnLine", updn_line, sizeof(updn_line)) != 0) {
            continue;
        }
        if (strstr(updn_line, "상행") == NULL) {
            continue;
        }
        if (get_json_int(item, "subwayId", &subway_id) != 0 || subway_id != TARGET_SUBWAY_ID) {
            continue;
        }

        if (get_json_int(item, "barvlDt", &sec) != 0) {
            continue;
        }
        (void)get_json_int(item, "arvlCd", &arvl_cd);

        if (sec < min_barvl) {
            min_barvl = sec;
            best->valid = 1;
            best->barvlDt = sec;
            best->arvlCd = arvl_cd;
            snprintf(best->updnLine, sizeof(best->updnLine), "%s", updn_line);

            if (get_json_string(item, "recptnDt", best->recptnDt, sizeof(best->recptnDt)) != 0) {
                best->recptnDt[0] = '\0';
            }
            if (get_json_string(item, "arvlMsg2", best->arvlMsg2, sizeof(best->arvlMsg2)) != 0) {
                best->arvlMsg2[0] = '\0';
            }
            if (get_json_string(item, "trainLineNm", best->trainLineNm, sizeof(best->trainLineNm)) != 0) {
                best->trainLineNm[0] = '\0';
            }
        }
    }

    cJSON_Delete(root);

    if (!best->valid) {
        fprintf(stderr, "no valid train found (updnLine=상행, subwayId=%d)\n", TARGET_SUBWAY_ID);
        return -1;
    }

    return 0;
}

int main(void)
{
    struct MemoryStruct chunk;
    TrainInfo best = {0};

    curl_global_init(CURL_GLOBAL_ALL);

    if (fetch_json(URL, &chunk) != 0) {
        curl_global_cleanup();
        return 1;
    }

    if (parse_best_train(chunk.memory, &best) != 0) {
        free(chunk.memory);
        curl_global_cleanup();
        return 1;
    }

    printf("updnLine    = %s\n", best.updnLine);
    printf("trainLineNm = %s\n", best.trainLineNm);
    printf("arvlMsg2    = %s\n", best.arvlMsg2);
    printf("arvlCd      = %d\n", best.arvlCd);
    printf("barvlDt     = %d\n", best.barvlDt);

    free(chunk.memory);
    curl_global_cleanup();
    return 0;
}
