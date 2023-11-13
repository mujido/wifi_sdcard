idf_component_register(SRCS "wifi.cpp" "main.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi libsmb2 nvs_flash)