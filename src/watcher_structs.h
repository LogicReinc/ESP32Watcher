#include "cJson.h"

#define MAX_WatcherSettingsSize 100
#define MAX_WatcherServerSettingsSize 255

typedef struct WatcherServerSettings{
    char ServerUrl[255];
    char ServerKey[37];
} WatcherServerSettings;


typedef struct WatcherSettings{
    int Interval;
} WatcherSettings;



//WatcherServerSettings JSON
WatcherServerSettings* createWatcherServerSettings();
WatcherServerSettings* updateWatcherServerSettingsJson(WatcherServerSettings* s, cJSON* json);
WatcherServerSettings* parseWatcherServerSettingsJson(cJSON* json);
cJSON* serializeWatcherServerSettings(WatcherServerSettings* settings);

//WatcherSettings JSON
WatcherSettings* createWatcherSettings();
WatcherSettings* parseWatcherSettingsJson(cJSON* json);
WatcherSettings* updateWatcherSettingsJson(WatcherSettings* s, cJSON* json);
cJSON* serializeWatcherSettings(WatcherSettings* settings);
