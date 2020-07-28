
#include "freertos/FreeRTOS.h"
#include "lwip/sockets.h"
#include "string.h"
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "tcpip_adapter.h"
#include "provisioning.h"

extern const uint8_t provision_html_start[] asm("_binary_provision_html_start");
extern const uint8_t provision_html_end[] asm("_binary_provision_html_end");

//AP Settings
char* AP_NAME = "ESPWifi";
char* AP_PASSWORD = "password";

uint8_t AP_Gateway[4] = {192, 168, 4, 1};
uint8_t AP_Mask[4] = {255, 255, 255, 0};

uint8_t AP_LeaseStart[4] = {192, 168, 4, 100};
uint8_t AP_LeaseEnd[4] = {192, 168, 4, 110};

int AP_MaxCon = 1;

bool provisioned = false;

EventGroupHandle_t egWifi = 0;
const int BIT_SCAN = BIT0;
const int BIT_START = BIT1;
const int BIT_DISCONNECT = BIT2;
const int BIT_DHCP = BIT3;

WifiConfig con_descriptor;

WifiConfig* last_configs;
int last_configs_count = 0; 


//Set AP Mode
void setAPMode()
{
    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(AP_NAME),
            .channel = 1,
            .max_connection = AP_MaxCon,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    strcpy((char*)wifi_config.ap.ssid, AP_NAME);
    strcpy((char*)wifi_config.ap.password, AP_PASSWORD);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

//Handlers

//GET: /
//HTML Interface
esp_err_t get_html_handler(httpd_req_t *req){
    char* pointer = (char*)provision_html_start;
    httpd_resp_send(req,pointer , (provision_html_end - provision_html_start));
    return ESP_OK;
};
httpd_uri_t get_html = {
    .uri    = "/",
    .method = HTTP_GET,
    .handler= get_html_handler,
    .user_ctx = NULL
};

//GET: /ssids
//Returns json array of known ssids
esp_err_t get_ssids_handler(httpd_req_t *req){
    int strLength = 2;
    int i = 0;
    //Commas and quotes
    strLength += last_configs_count - 1 + last_configs_count*2;
    for(i = 0; i < last_configs_count; i++){
        strLength += strlen(last_configs[i].SSID);
    }
    char response[strLength];
    strcpy(response, "[");
    for(i = 0; i < last_configs_count; i++){
        strcat(response, "\"");
        strcat(response, last_configs[i].SSID);
        strcat(response, "\"");
        if(i != last_configs_count-1)
            strcat(response, ",");
    }
    strcat(response, "]");
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}
httpd_uri_t get_ssids = {
    .uri    = "/ssids",
    .method = HTTP_GET,
    .handler= get_ssids_handler,
    .user_ctx = NULL
};

//GET: /provision?ssid=a&pass=b&slot=c
//Allows provisioning of a new wifi with ssid a, pass b into slow c
//c<MAX_WIFI_SLOTS
esp_err_t get_provision_handler(httpd_req_t *req){
    int buff_len = httpd_req_get_url_query_len(req) + 1;
    char* buf;
    char ssid[32];
    char ssid_pass[64];
    char slot[2];
    int slotNum = 0;
    bool gotSSID = false;
    bool gotPass = false;
    bool gotSlot = false;
    if(buff_len > 1){
        buf = malloc(buff_len);
        if(httpd_req_get_url_query_str(req, buf, buff_len) == ESP_OK){
            if(httpd_query_key_value(buf, "ssid", ssid, 32) == ESP_OK)
                gotSSID = true;
            if(httpd_query_key_value(buf, "pass", ssid_pass, 32) == ESP_OK)
                gotPass = true;
            if(httpd_query_key_value(buf, "slot", slot, 2) == ESP_OK)
            {
                gotSlot = true;
                slotNum = atoi(slot);
            }
        }
        free(buf);
    }

    if(!gotSSID){
        const char resp[] = "{\"Success\":false, \"Error\":\"Missing SSID\"}";
        httpd_resp_send(req, resp, strlen(resp));
    }
    else if(!gotPass){
        const char resp[] = "{\"Success\":false, \"Error\":\"Missing Pass\"}";
        httpd_resp_send(req, resp, strlen(resp));
    }
    else if(!gotSSID){
        const char resp[] = "{\"Success\":false, \"Error\":\"Missing Slot\"}";
        httpd_resp_send(req, resp, strlen(resp));
    }
    else
    {
        printf("Provisioning succesful [%s, %s]...\n", ssid, ssid_pass);
        const char resp[] = "{\"Success\":true}";
        httpd_resp_send(req, resp, strlen(resp));
        strncpy(con_descriptor.SSID, ssid, strlen(ssid));
        strncpy(con_descriptor.SSID_Pass, ssid_pass, strlen(ssid_pass));
        con_descriptor.Slot = slotNum;
        con_descriptor.Usable = true;
        
    }
    return ESP_OK;
}
httpd_uri_t get_provision = {
    .uri    = "/provision",
    .method = HTTP_GET,
    .handler= get_provision_handler,
    .user_ctx = NULL
};


//Stop the server
void stop_server(httpd_handle_t server){
    if(server)
        httpd_stop(server);
}

//Handle Wifi events (Sets bit to indicate completions);
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
  //httpd_handle_t *server = (httpd_handle_t *)ctx;

  switch (event->event_id)
  {
    //Wifi Ready
    case SYSTEM_EVENT_STA_START:
        printf("STA_START\n");
        xEventGroupSetBits(egWifi, BIT_START);
        break;

    //Finished a scan
    case SYSTEM_EVENT_SCAN_DONE:
        printf("Finished Scan\n");
        xEventGroupSetBits(egWifi, BIT_SCAN);
        break;

    //Got an IP from an AP
    case SYSTEM_EVENT_STA_GOT_IP:
        printf("Got IP: %s\n", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(egWifi, BIT_DHCP);
        break;

    //Disconnected from an AP
    case SYSTEM_EVENT_STA_DISCONNECTED:
        printf("STA Disconnected\n");
        xEventGroupSetBits(egWifi, BIT_DISCONNECT);
        break;
    default:
        break;
  }
  return ESP_OK;
}
//Initialize WIFI and various state variables
bool initWifi(){
    egWifi = xEventGroupCreate();
    tcpip_adapter_init();
    esp_event_loop_init(event_handler, NULL);
    //esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_err_t status = esp_wifi_start();
    bool success = false;
    switch(status){
        case ESP_ERR_WIFI_NOT_INIT:
            printf("Wifi not initialized\n");
        break;
        case ESP_FAIL:
            printf("Failed to connect to wifi: ESP_FAIL\n");
            break;
        case ESP_ERR_INVALID_ARG:
            printf("Failed to connect to wifi: ESP_ERR_INVALID_ARG\n");
            break;
        case ESP_ERR_NO_MEM:
            printf("Failed to connect to wifi: ESP_ERR_NO_MEM\n");
            break;
        case ESP_OK:
            printf("Connected to wifi\n");
            success = true;
            break;
    }
    return success;
}

//Starts a provisioning webserver
WifiConfig* provision()
{
    con_descriptor.Usable = false;

    printf("Starting AP Mode..\n");
    setAPMode();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if(httpd_start(&server, &config) == ESP_OK){
        httpd_register_uri_handler(server, &get_html);
        httpd_register_uri_handler(server, &get_provision);
        httpd_register_uri_handler(server, &get_ssids);
    }
    
    printf("Waiting for provisioning...\n");
    while(!con_descriptor.Usable){
        vTaskDelay(3000 / portTICK_RATE_MS);
    }
    printf("Completed provisioning, stopping webserver...\n");
    stop_server(server);

    return &con_descriptor;
}

//Connect to given SSID
void connectSSID(char* ssid, char* pass){
    printf("Connecting to SSID...%s\n", ssid);

    esp_wifi_set_mode(WIFI_MODE_STA);
    wifi_config_t wifi_config = {
        .sta = {
        },
    };
    strncpy((char*)wifi_config.sta.ssid, ssid, 32);
    strncpy((char*)wifi_config.sta.password, pass, 64);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
}
//Connect to given WifiConfig
void connectSSIDCon(WifiConfig* con){
    connectSSID(con->SSID, con->SSID_Pass);
}
//Set current known WifiConfigs
void setSSIDCons(WifiConfig* cons, int conCount){
    last_configs = cons;
    last_configs_count = conCount;
}
//Connect to given WifiConfigs
int connectSSIDCons(WifiConfig* cons, int conCount){
    if(conCount == 0)
        return STATUS_NOAP;

    setSSIDCons(cons, conCount);

    //Itterators
    int i,j = 0;

    //Wait for Wifi to start
    xEventGroupWaitBits(egWifi, BIT_START, pdFALSE, pdFALSE, portMAX_DELAY);

    //Start Scan
    wifi_scan_config_t scanConf =
        { .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true };
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scanConf, 0));
    //Wait for scan to complete
    xEventGroupWaitBits(egWifi, BIT_SCAN, pdFALSE, pdFALSE, portMAX_DELAY);

    //Get number of found AP
    uint16_t apCount = 0;
    esp_wifi_scan_get_ap_num(&apCount);
    wifi_ap_record_t* aplist = NULL;

    //If no AP found, return NOAP;
    if(!apCount)
        return STATUS_NOAP;
    
    //Otherwise, continue to allocate the AP records
    free(aplist);
    aplist = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * apCount);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, aplist));

    //List strength & SSIDs
    for(i = 0; i < apCount; i++){
        printf("ScanResult: [%d] %s\n", aplist[i].rssi, aplist[i].ssid);
    }

    //Do untill connected:
    while(1){
        //Find what to connect to
        int toConnect = -1;
        for(i = 0; i < apCount && toConnect == -1; i++)
            for(j = 0; j < conCount && toConnect == -1; j++)
                if(cons[j].Usable && strcasecmp((char*)aplist[i].ssid, cons[j].SSID))
                    toConnect = j;
        //If no suitable AP was found, return NOAP
        if(toConnect == -1)
            return STATUS_NOAP;

        //Connect..
        connectSSIDCon(&cons[toConnect]);
        int result = xEventGroupWaitBits(egWifi, BIT_DHCP|BIT_DISCONNECT, pdFALSE, pdFALSE, portMAX_DELAY);
        
        //If succesfully gotten IP, done..
        if(result & BIT_DHCP)
            break;
    }
    free(aplist);
    return STATUS_CONNECTED;
}

