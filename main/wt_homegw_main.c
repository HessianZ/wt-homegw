#include <stdlib.h>

#include "nvs_flash.h"
#include "esp_log.h"
#include "app.h"
#include "wifi_mgr.h"
#include "mqtt.h"
#include "http_server.h"
#include "file_manager.h"
#include "bsp.h"
#include "settings.h"

static const char *TAG = "APP_MAIN";

void app_main(void) {

    esp_log_level_set("APP_ESPNOW_RECV", ESP_LOG_DEBUG);
    esp_log_level_set("ESPNOW", ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "[APP] Compile time: %s %s", __DATE__, __TIME__);// Initialize NVS

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(settings_read_parameter_from_nvs());
    settings_dump();

    bsp_spiffs_mount();

    TraverseDir("/spiffs", 0, 1);

    wifi_mgr_start();

    xTaskCreate(mqtt_task, "mqtt_task", 4096, NULL, 3, NULL);
    xTaskCreate(http_server_init, "http_server_task", 4096, NULL, 3, NULL);

    app_espnow_init();
}
