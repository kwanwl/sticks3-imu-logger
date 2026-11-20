# Firmware sources. M5Unified comes from idf_component.yml; the web page is linked in as
# a text blob (symbols _binary_index_html_start/_end, used in net.cpp).
idf_component_register(
    SRCS "main.cpp" "imu.cpp" "net.cpp" "usb.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_timer esp_wifi esp_netif esp_event nvs_flash esp_http_server fatfs esp_driver_usb_serial_jtag
    EMBED_TXTFILES "web/index.html")
