# Regenerate on .env changes for both build.ps1 and direct idf.py builds.
set(LOCAL_ENV "${CMAKE_CURRENT_LIST_DIR}/../.env")
set(LOCAL_GENERATOR "${CMAKE_CURRENT_LIST_DIR}/../tools/generate_config.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${LOCAL_ENV}" "${LOCAL_GENERATOR}")
execute_process(
    COMMAND "${PYTHON}" "${LOCAL_GENERATOR}" --env "${LOCAL_ENV}"
            --output "${CMAKE_CURRENT_BINARY_DIR}/local_settings.h"
    RESULT_VARIABLE LOCAL_SETTINGS_RESULT
)
if(NOT LOCAL_SETTINGS_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not load local ESP32 settings. Check .env values.")
endif()

idf_component_register(
    SRCS "app_main.c" "settings.c" "network.c" "metrics.c" "ui.c" "wifi_ui.c"
    INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
    REQUIRES esp_wifi esp_event esp_netif esp_http_client
             nvs_flash esp_timer lwip esp_system esp_hw_support
)
