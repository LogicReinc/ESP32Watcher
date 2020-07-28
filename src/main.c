#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp32/himem.h>
#include <esp32/spiram.h>
#include "cam.h"
#include "esp_camera.h"
#include "provisioning.h"
#include "webserver.h"
#include "watcher.h"

#define MAX_WIFI 5
#define CLEAR_NVS_ON_LAUNCH 0

WifiConfig wifiConfigs[MAX_WIFI]; 

void loadWifiConfigs(nvs_handle_t nvsHandle, WifiConfig* configs, int count)
{
    char ssid_key[7];
    char pass_key[7];
    for(int8_t i = 0; i < MAX_WIFI; i++)
    {
        ssid_key[0] = '\0';
        pass_key[0] = '\0';
        sprintf(ssid_key, "%s%hu", "SSID", i);
        sprintf(pass_key, "%s%hu", "PASS", i);
        printf("Looking up %s and %s\n", ssid_key, pass_key);
        size_t size = 32;
        if(nvs_get_str(nvsHandle, ssid_key, configs[i].SSID, &size) == ESP_OK)
        {
            size = 64;
            nvs_get_str(nvsHandle, pass_key, configs[i].SSID_Pass, &size);
            configs[i].Usable = true;
        }
    }
}
void saveWifiConfig(nvs_handle_t nvsHandle, WifiConfig* config)
{
    if(config->Slot >= MAX_WIFI){
        printf("Attempted to write slot > max slots\n");
        return;
    }
    char ssid_key[7];
    char pass_key[7];
    sprintf(ssid_key, "%s%hu", "SSID", config->Slot);
    sprintf(pass_key, "%s%hu", "PASS", config->Slot);
    ESP_ERROR_CHECK(nvs_set_str(nvsHandle, ssid_key, config->SSID));
    ESP_ERROR_CHECK(nvs_set_str(nvsHandle, pass_key, config->SSID_Pass));

    wifiConfigs[config->Slot] = *config;

    printf("New SSID [%s] in slot %d\n", wifiConfigs[config->Slot].SSID, config->Slot);
}
void app_main()
{

    vTaskDelay(5000 / portTICK_RATE_MS);

    //Initialize flash
    printf("Initialize Flash\n");
    ESP_ERROR_CHECK(nvs_flash_init());

    //Load NVS Handle
    nvs_handle_t* nvsHandle = malloc(sizeof(nvs_handle_t));
    ESP_ERROR_CHECK(nvs_open("Wifi", NVS_READWRITE, nvsHandle));

    if(CLEAR_NVS_ON_LAUNCH){
        printf("Erasing all NVS\n");
        nvs_erase_all(*nvsHandle);
        nvs_commit(*nvsHandle);
    }

    //Load Wifi
    loadWifiConfigs(*nvsHandle, &wifiConfigs[0], MAX_WIFI);



    
    printf("Attempt Start WIFI...\n");
    if(initWifi()){
        bool connected = false;
        while(!connected){
            int status = connectSSIDCons(&wifiConfigs[0], 5);
            WifiConfig* newConfig;
            switch(status){
                case STATUS_NOAP:
                    printf("No AP known\n");
                    newConfig = provision();
                    if(newConfig)
                        saveWifiConfig(*nvsHandle, newConfig);
                    else{
                        printf("No config from provisioning... Ending main\n");
                        return;
                    }
                    break;
                case STATUS_CONNECTED:
                    printf("Succesfully connected to a wifi\n");
                    connected = true;
                    break;
            }
        }
    }
    else {
        printf("Failed to initialize Wifi, ending main, fix your code\n");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        return;
    }

    //Initialize watcher
    if(!initWatcher(nvsHandle)){
        printf("Failed to initialize watcher, ending main\n");
        return;
    }

    
    //Initialize Camera
    while(init_camera() != ESP_OK)
    {
        printf("Failed to initialize camera...\n");
        vTaskDelay(5000 / portTICK_RATE_MS);
    }

    //Start webserver
    TaskHandle_t task_webServer;
    TaskHandle_t task_watcher;
    xTaskCreatePinnedToCore(startWebServer, "WebServerTask", 4096, NULL, 1, &task_webServer, 0);
    xTaskCreatePinnedToCore(startWatcher, "WatcherTask", 65536, NULL, 1, &task_watcher, 1);

    //startWebServer();
}