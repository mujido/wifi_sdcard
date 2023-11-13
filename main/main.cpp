//
// @project     wifi_sdcard
//
// @author      Kurt Stutsman <kstutsman@gmail.com>
//
// @license     Apache-2.0
//

#include "wifi.h"

#include <esp_log.h>
#include <esp_netif.H>
#include <esp_event.h>

#include <nvs_flash.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include <cstdio>

inline constexpr const char* TAG = "main.cpp";

int smb2_ls_sync()
{
    struct smb2_context *smb2;
    struct smb2dir *dir;
    struct smb2dirent *ent;
    char link[1024];

    smb2 = smb2_init_context();
    if (smb2 == NULL)
    {
        ESP_LOGI(TAG, "Failed to init context\n");
        return -1;
    }

    smb2_set_security_mode(smb2, SMB2_NEGOTIATE_SIGNING_ENABLED);

    int rc = smb2_connect_share(
        smb2, 
        CONFIG_WIFI_SDCARD_SMB_HOST, 
        CONFIG_WIFI_SDCARD_SMB_SHARE, 
        CONFIG_WIFI_SDCARD_SMB_USERNAME);
    if (rc < 0)
    {
        ESP_LOGI(TAG, "smb2_connect_share failed. %s\n", smb2_get_error(smb2));
        return -1;
    }

    ESP_LOGI(TAG, "Connected\n");

    dir = smb2_opendir(smb2, "");
    if (dir == NULL)
    {
        ESP_LOGI(TAG, "smb2_opendir failed. %s\n", smb2_get_error(smb2));
        return -1;
    }

    ESP_LOGI(TAG, "Opened directory\n");

    while ((ent = smb2_readdir(smb2, dir)))
    {
        const char *type;
        switch (ent->st.smb2_type)
        {
        case SMB2_TYPE_LINK:
            type = "LINK";
            break;
        case SMB2_TYPE_FILE:
            type = "FILE";
            break;
        case SMB2_TYPE_DIRECTORY:
            type = "DIRECTORY";
            break;
        default:
            type = "unknown";
            break;
        }
        ESP_LOGI(TAG, "%-20s %-9s %15llu\n", ent->name, type, ent->st.smb2_size);
        if (ent->st.smb2_type == SMB2_TYPE_LINK)
        {
            char buf[256];

            sprintf(link, "/%s", ent->name);
            smb2_readlink(smb2, link, buf, 256);
            ESP_LOGI(TAG, "    -> [%s]\n", buf);
        }
    }

    ESP_LOGI(TAG, "readdir complete\n");

    smb2_closedir(smb2, dir);
    smb2_disconnect_share(smb2);
    smb2_destroy_context(smb2);

    return 0;
}

static void main_task(__unused void *params)
{
    printf("Connecting to WiFi...\n");

    smb2_ls_sync();
    vTaskDelete(NULL);
}

extern "C" int app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    ESP_ERROR_CHECK(wifi_connect());

    wifi_wait_for_ip();

    ESP_LOGI(TAG, "Connected to AP");

    xTaskCreate(main_task, "SMBTask", 8192, nullptr, 5, nullptr);

    return 0;
}
