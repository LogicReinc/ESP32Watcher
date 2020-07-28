
#include "watcher.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_camera.h"
#include "string.h"

bool watcher_initialized = false;
int watchTaskID = 0;
int watchTaskIDPass = 0;
WatcherSettings* settings;
WatcherServerSettings* serverSettings;

nvs_handle_t storageHandle;

#define STORAGE_WatcherSettings "watcher"
#define STORAGE_WatcherServerSettings "server"

const char* _ERRMSG_NVS_NOT_FOUND = "ESP_ERR_NVS_NOT_FOUND";
const char* _ERRMSG_NVS_INVALID_HANDLE = "ESP_ERR_NVS_INVALID_HANDLE";
const char* _ERRMSG_NVS_INVALID_NAME = "ESP_ERR_NVS_INVALID_NAME";
const char* _ERRMSG_NVS_INVALID_LENGTH = "ESP_ERR_NVS_INVALID_LENGTH";
const char* _ERRMSG_NVS_READ_ONLY = "ESP_ERR_NVS_READ_ONLY";
const char* _ERRMSG_NVS_NOT_ENOUGH_SPACE = "ESP_ERR_NVS_NOT_ENOUGH_SPACE ";
const char* _ERRMSG_NVS_REMOVE_FAILED = "ESP_ERR_NVS_REMOVE_FAILED";
const char* _ERRMSG_NVS_VALUE_TOO_LONG = "ESP_ERR_NVS_VALUE_TOO_LONG";
const char* _ERRMSG_UNKNOWN = "UNKNOWN";

char* getErrorMessage(esp_err_t* err){
    switch(*err){
        case ESP_ERR_NVS_NOT_FOUND:
            return _ERRMSG_NVS_NOT_FOUND;
        case ESP_ERR_NVS_INVALID_HANDLE:
            return _ERRMSG_NVS_INVALID_HANDLE;
        case ESP_ERR_NVS_INVALID_NAME:
            return _ERRMSG_NVS_INVALID_NAME;
        case ESP_ERR_NVS_INVALID_LENGTH:
            return _ERRMSG_NVS_INVALID_LENGTH;
        case ESP_ERR_NVS_READ_ONLY:
            return _ERRMSG_NVS_READ_ONLY;
        case ESP_ERR_NVS_NOT_ENOUGH_SPACE:
            return _ERRMSG_NVS_NOT_ENOUGH_SPACE;
        case ESP_ERR_NVS_REMOVE_FAILED:
            return _ERRMSG_NVS_REMOVE_FAILED;
        case ESP_ERR_NVS_VALUE_TOO_LONG:
            return _ERRMSG_NVS_VALUE_TOO_LONG;
    }
    return _ERRMSG_UNKNOWN;
}


//Initializes Watcher (Specifically load settings)
bool initWatcher(nvs_handle_t* t){
    char* watcherSettings[MAX_WatcherSettingsSize];
    size_t watcherSettingsSize = MAX_WatcherSettingsSize;
    char* watcherServerSettings[MAX_WatcherServerSettingsSize];
    size_t watcherServerSettingsSize = MAX_WatcherServerSettingsSize;
    printf("Attempting to read Watcher settings\n");
    esp_err_t errWS = nvs_get_str(*t, STORAGE_WatcherSettings, watcherSettings, &watcherSettingsSize);
    printf("Attempted WatcherSettings\n");
    esp_err_t errWSS = nvs_get_str(*t, STORAGE_WatcherServerSettings, watcherServerSettings, &watcherServerSettingsSize);
    printf("Attempted WatcherServerSettings\n");
    storageHandle = *t;
    if(errWSS == ESP_OK)
    {
        printf("Found WatcherServer Settings [%hu]\n", watcherServerSettingsSize);
        cJSON* wssJson = cJSON_Parse(watcherServerSettings);
        printf("Parsed Json..\n");
        serverSettings = parseWatcherServerSettingsJson(wssJson);
        printf("Deserialized..\n");
        //char* json = cJSON_Print(wssJson);
        //printf("Loaded WatcherServer Settings: %s\n", json);
        //free(json);
        
        cJSON_Delete(wssJson);
    }
    else
    {
        char* errMsg = getErrorMessage(&errWSS);
        printf("Creating new WatcherServerSettings due to %s\n", errMsg);
        serverSettings = createWatcherServerSettings();
    }
    if(errWS == ESP_OK)
    {
        printf("Found Watcher Settings [%hu]\n", watcherSettingsSize);
        cJSON* wsJson = cJSON_Parse(watcherSettings);
        settings = parseWatcherSettingsJson(wsJson);

        
        //char* json = cJSON_Print(wsJson);
        //printf("Loaded Watcher Settings: %s\n", json);
        //free(json);

        cJSON_Delete(wsJson);
    }
    else{
        char* errMsg = getErrorMessage(&errWSS);
        printf("Creating new WatcherSettings due to %s\n", errMsg);
        settings = createWatcherSettings();
    }
    watcher_initialized = true;
    return true;
}

//Save Settings
int saveWatcherSettings(){
    if(!watcher_initialized)
    {
        printf("Watcher not initialized.. call initWatcher\n");
        return RESULT_NOT_INITIALIZED;
    }
    cJSON* saveJson = serializeWatcherSettings(settings);
    char* str = cJSON_Print(saveJson);

    esp_err_t err = nvs_set_str(storageHandle, STORAGE_WatcherSettings, str);
    if(err == ESP_OK)
        printf("Saved new WatcherSettings %s\n", str);
    else
        printf("Error while saving\n%s\ndue to %s\n", str, getErrorMessage(&err));
    printf("Saved new WatcherSettings %s\n", str);
    free(str);
    cJSON_Delete(saveJson);
    return RESULT_OK;
}
int saveWatcherServerSettings(){
    if(!watcher_initialized)
    {
        printf("Watcher not initialized.. call initWatcher\n");
        return RESULT_NOT_INITIALIZED;
    }
    cJSON* saveJson = serializeWatcherServerSettings(serverSettings);
    char* str = cJSON_Print(saveJson);
    esp_err_t err = nvs_set_str(storageHandle, STORAGE_WatcherServerSettings, str);
    if(err == ESP_OK)
        printf("Saved new WatcherServerSettings %s\n", str);
    else
        printf("Error while saving\n%s\ndue to %s\n", str, getErrorMessage(&err));
    
    free(str);
    cJSON_Delete(saveJson);
    return RESULT_OK;
}

//Update Settings
int updateWatcherSettings(cJSON* json){
    if(!watcher_initialized)
    {
        printf("Watcher not initialized.. call initWatcher\n");
        return RESULT_NOT_INITIALIZED;
    }
    updateWatcherSettingsJson(settings, json);
    return saveWatcherSettings();
}
int updateWatcherServerSettings(cJSON* json){
    if(!watcher_initialized)
    {
        printf("Watcher not initialized.. call initWatcher\n");
        return RESULT_NOT_INITIALIZED;
    }
    updateWatcherServerSettingsJson(serverSettings, json);
    return saveWatcherServerSettings();
}

//Set settings
void setWatcherSettings(WatcherSettings* sets){
    settings = sets;
}
void setWatcherServerSettings(WatcherServerSettings* sets){
    settings = sets;
}

WatcherSettings* getWatcherSettings(){
    return settings;
}
WatcherServerSettings* getWatcherServerSettings(){
    return serverSettings;
}


//Watcher Life
void handleWatchAction(){
    camera_fb_t *pic = esp_camera_fb_get();
    uploadImage(serverSettings, pic->buf, pic->len);
    esp_camera_fb_return(pic);

    printf("Picture taken! Its size was: %zu bytes\n", pic->len);
}

void watcherTask(int* taskID){
    int task = watchTaskIDPass;

    printf("Watcher Task started %hu\n", task);
    int wait = 5000;
    bool canUpload = true;
    while(watchTaskID == task){
        canUpload = true;
        if(!serverSettings || 
            strncmp(serverSettings->ServerUrl, "http", 4) != 0 ||
            strncmp(serverSettings->ServerKey, "\0", 1) == 0)
        {
            canUpload = false;
            printf("Cannot upload cuz no serverurl or key\n");
            printf("FoundServer: %s\n", serverSettings->ServerUrl);
            printf("FoundKey: %s\n", serverSettings->ServerKey);
        }   
        
        if(canUpload)
        {
            printf("Making Picture\n");
            handleWatchAction();
        }

        if(settings->Interval != 0)
            wait = settings->Interval;
        vTaskDelay(wait / portTICK_RATE_MS);
    }
    printf("Watcher Task ended\n");
}

void startWatcher(){
    watchTaskID++;
    printf("New BaseTaskID %hu", watchTaskID);
    watchTaskIDPass = watchTaskID;
    watcherTask(NULL);
}

void stopWatcher(){
    watchTaskID++;
}
