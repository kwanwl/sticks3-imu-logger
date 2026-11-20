# StickS3 IMU viewer and logger (ESP-IDF v6.1, target esp32s3)
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Workaround for ESP-IDF v6: M5GFX/M5Unified include esp_psram.h without declaring the
# esp_psram component, which v6 rejects. Add it to every component's requirements.
# Remove once M5Stack declares the dependency themselves.
idf_build_set_property(__COMPONENT_REQUIRES_COMMON esp_psram APPEND)

project(sticks3_imu_logger)
