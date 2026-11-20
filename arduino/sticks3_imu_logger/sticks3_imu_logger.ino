// StickS3 IMU viewer and logger - Arduino version
//
// Same firmware as the ESP-IDF project in the repository root: the other files in this
// folder (app.h, main.cpp, imu.cpp, net.cpp, index_html.h, partitions.csv) are copied or
// generated from main/ by tools/sync_arduino.py. Edit them in main/, not here.
//
// Arduino IDE settings (Tools menu):
//   Board               ESP32S3 Dev Module   (core "esp32" by Espressif, version 3.3.x)
//   USB CDC On Boot     Enabled
//   USB Mode            Hardware CDC and JTAG
//   CPU Frequency       240MHz (WiFi)
//   Flash Size          8MB (64Mb)
//   Partition Scheme    Custom   (uses partitions.csv in this folder)
//   PSRAM               OPI PSRAM
// Library: M5Unified 0.3.0 (Library Manager; installs M5GFX with it).
#include <M5Unified.h>
#include "app.h"

void setup()
{
    app_run();      // starts the IMU task, storage and Wi-Fi, then runs the screen; never returns
}

void loop()
{
    // Not reached: app_run() keeps running the user interface.
}
