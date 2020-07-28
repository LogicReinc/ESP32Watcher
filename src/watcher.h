#include <nvs_flash.h>
#include "cJSON.h"
#include "watcher_server.h"

#define RESULT_NOT_INITIALIZED 1
#define RESULT_OK 10

//Initialize
bool initWatcher(nvs_handle_t* t);

//Update Settings
int updateWatcherSettings(cJSON* json);
int updateWatcherServerSettings(cJSON* json);

WatcherSettings* getWatcherSettings();
WatcherServerSettings* getWatcherServerSettings();

//Start/Stop Watcher
void startWatcher();
void stopWatcher();

