#include "freertos/FreeRTOS.h"
#include "esp_http_client.h"
#include "string.h"
#include "watcher_server.h"

char* replace_char(char* str, char find, char replace){
    char *current_pos = strchr(str,find);
    while (current_pos){
        *current_pos = replace;
        current_pos = strchr(current_pos,find);
    }
    return str;
}

//Server Calls
bool uploadImage(WatcherServerSettings* server, uint8_t* image, size_t imageSize){
    char url[255];
    strcpy(url, server->ServerUrl);
    strcat(url, "/upstream/upload?key=");
    strncat(url, server->ServerKey, 36);

    printf("POST=>%s\n", url);

    esp_http_client_config_t config_uploadImage = {
        .url =  url
    };
    esp_http_client_handle_t client = esp_http_client_init(&config_uploadImage);

    if(esp_http_client_set_post_field(client, (char*)image, imageSize) != ESP_OK)
        printf("Failed to set PostData\n");
    if(esp_http_client_set_method(client, HTTP_METHOD_POST) != ESP_OK)
        printf("Failed to set Method\n");
    esp_err_t err = esp_http_client_perform(client);
    if(err == ESP_OK){
        printf("Request succesful\n");
    }
    else{
        printf("Response:%hu\n", esp_http_client_get_status_code(client));
    }
    esp_http_client_cleanup(client);
    return err == ESP_OK;
}
