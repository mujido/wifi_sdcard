#pragma once

#include <esp_err.h>

esp_err_t wifi_connect();
void wifi_shutdown();
void wifi_wait_for_ip();
