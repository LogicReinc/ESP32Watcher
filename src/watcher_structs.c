#include "watcher_structs.h"
#include "freertos/FreeRTOS.h"
#include "string.h"

//WatcherServerSettings
WatcherServerSettings* createWatcherServerSettings(){
    WatcherServerSettings* s = malloc(sizeof(WatcherServerSettings));
    strcpy(s->ServerUrl, "");
    strcpy(s->ServerKey, "");
    return s;
}
WatcherServerSettings* updateWatcherServerSettingsJson(WatcherServerSettings* s, cJSON* json){
    cJSON* server = cJSON_GetObjectItem(json, "server");
    cJSON* key = cJSON_GetObjectItem(json, "key");
    if(server)
    {
        printf("Updating Server to %s\n", server->valuestring);
        strncpy((char*)s->ServerUrl, server->valuestring, strlen(server->valuestring));
    }
    if(key)
    {
        printf("Updating Key to %s\n", key->valuestring);
        strncpy((char*)s->ServerKey, key->valuestring, strlen(key->valuestring));
    }
    return s;
}
WatcherServerSettings* parseWatcherServerSettingsJson(cJSON* json){
    WatcherServerSettings* s = createWatcherServerSettings();
    return updateWatcherServerSettingsJson(s,json);
}
cJSON* serializeWatcherServerSettings(WatcherServerSettings* settings){
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "server", settings->ServerUrl);
    cJSON_AddStringToObject(json, "key", settings->ServerKey);
    return json;
}

//WatcherSettings
WatcherSettings* createWatcherSettings(){
    WatcherSettings* s = malloc(sizeof(WatcherSettings));
    s->Interval = 5000;
    return s;
}
WatcherSettings* updateWatcherSettingsJson(WatcherSettings* s, cJSON* json){
    cJSON* interval = cJSON_GetObjectItem(json, "interval");
    if(interval)
        s->Interval = interval->valueint;
    return s;
}
WatcherSettings* parseWatcherSettingsJson(cJSON* json){
    WatcherSettings* s = createWatcherSettings();
    return updateWatcherSettingsJson(s, json);
}
cJSON* serializeWatcherSettings(WatcherSettings* settings){
    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "interval", settings->Interval);
    return json;
}
