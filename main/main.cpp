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

int smb2_ls_sync(char* user_url)
{
    struct smb2_context *smb2;
    struct smb2_url *url;
    struct smb2dir *dir;
    struct smb2dirent *ent;
    char link[1024];

    smb2 = smb2_init_context();
    if (smb2 == NULL)
    {
        ESP_LOGI(TAG, "Failed to init context\n");
        return -1;
    }

    url = smb2_parse_url(smb2, user_url);
    if (url == NULL)
    {
        ESP_LOGI(TAG, "Failed to parse url: %s\n",
               smb2_get_error(smb2));
        return -1;
    }

    smb2_set_security_mode(smb2, SMB2_NEGOTIATE_SIGNING_ENABLED);

    if (smb2_connect_share(smb2, url->server, url->share, url->user) < 0)
    {
        ESP_LOGI(TAG, "smb2_connect_share failed. %s\n", smb2_get_error(smb2));
        return -1;
    }

    ESP_LOGI(TAG, "Connected\n");

    dir = smb2_opendir(smb2, url->path);
    if (dir == NULL)
    {
        ESP_LOGI(TAG, "smb2_opendir failed. %s\n", smb2_get_error(smb2));
        return -1;
    }

    ESP_LOGI(TAG, "Opened directory\n");

    while ((ent = smb2_readdir(smb2, dir)))
    {
        ESP_LOGI(TAG, "Got entry\n");

        const char *type;
        time_t t;

        t = (time_t)ent->st.smb2_mtime;
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

            if (url->path && url->path[0])
            {
                sprintf(link, "%s/%s", url->path, ent->name);
            }
            else
            {
                sprintf(link, "%s", ent->name);
            }
            smb2_readlink(smb2, link, buf, 256);
            ESP_LOGI(TAG, "    -> [%s]\n", buf);
        }
    }

    ESP_LOGI(TAG, "readdir complete\n");

    smb2_closedir(smb2, dir);
    smb2_disconnect_share(smb2);
    smb2_destroy_url(url);
    smb2_destroy_context(smb2);

    return 0;
}

static void main_task(__unused void *params)
{
    printf("Connecting to WiFi...\n");

    smb2_ls_sync("smb://MUJIDO;guest@tequila.mujido.com/shared/");
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
