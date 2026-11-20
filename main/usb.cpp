// usb.cpp - recordings and the live stream over the USB-C cable
//
// For classrooms where Wi-Fi is congested: the web page, opened from GitHub Pages in Chrome or
// Edge, connects to the StickS3 with Web Serial and uses this simple text protocol.
//
// Commands (one line each, from the computer):
//   HELLO          -> @HELLO,sticks3-imu-logger,<hotspot name>
//   LIVE 1 | 0     start / stop the live stream (the page repeats LIVE 1 every few seconds;
//                  the stream stops by itself LIVE_TIMEOUT_MS after the last one)
//   LIST           -> @F,<name>,<size> for each recording, then @FILES,<count>
//   GET <name>     -> @BEGIN,<name>,<size>  then exactly <size> bytes of the file,
//                     then a newline and @END,<name>. If the computer stops reading, the
//                     transfer is abandoned without @END; the page then discards it.
//   DEL <name>     -> @OK,DEL,<request number> (the StickS3 asks for KEY1; the outcome is
//                     del_state in the status line) or @ERR,DEL,<name> (bad name or busy)
// Live stream (every 100 ms while on): @S,<status fields> and @L,<sample> lines, the same
// fields as the Wi-Fi stream (see net.cpp), but with every sample (100 Hz): a USB cable has
// plenty of capacity and does not share the crowded Wi-Fi band, so it is not thinned. Other lines (e.g. "# ..." log lines) are ignored
// by the page.
//
// Both builds (ESP-IDF and Arduino) install ESP-IDF's USB-Serial-JTAG driver and route the
// console (printf) through it, so commands can be read without blocking and all output
// shares one buffer. The driver never stalls the firmware without a computer: output is
// dropped when no USB host is connected, and after at most one 50 ms wait when the host is
// not reading. (Arduino's own Serial object is not used: in tests its USB output lost lines.)
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_err.h"
#include "esp_timer.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app.h"

static constexpr int64_t LIVE_TIMEOUT_MS = 10000;     // stop streaming if the page goes quiet

// ------------------------------------------------------------------------------- transport
// One message is written as a whole, so other tasks' log lines cannot land inside it.
static void out_begin(void)
{
    flockfile(stdout);
}

// Returns false if the computer did not take the data within 50 ms (the rest is dropped).
static bool out(const char* data, size_t n)
{
    fflush(stdout);                                   // anything printf has buffered goes first
    while (n > 0) {                                   // straight into the driver's buffer
        const int k = usb_serial_jtag_write_bytes(data, n, pdMS_TO_TICKS(50));
        if (k <= 0) return false;
        data += k;
        n -= k;
    }
    return true;
}

static void out_end(void)
{
    funlockfile(stdout);
}

static void out_line(const char* line)
{
    out_begin();
    out(line, strlen(line));
    out("\n", 1);
    out_end();
}

// Non-blocking read of what the computer has sent; returns the number of bytes (0 if none).
static int in_read(char* buf, int n)
{
    const int k = usb_serial_jtag_read_bytes(buf, n, 0);
    return k > 0 ? k : 0;
}

// ------------------------------------------------------------------------------- commands
static void cmd_list(void)
{
    int count = 0;
    char line[64];
    DIR* d = opendir(DATA_DIR);
    if (d) {
        struct dirent* e;
        while ((e = readdir(d))) {
            if (!is_run_name(e->d_name)) continue;
            char path[300];
            struct stat st;
            snprintf(path, sizeof(path), "%s/%s", DATA_DIR, e->d_name);
            if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;
            snprintf(line, sizeof(line), "@F,%.12s,%ld", e->d_name, (long)st.st_size);  // run names are 9 chars
            out_line(line);
            count++;
        }
        closedir(d);
    }
    snprintf(line, sizeof(line), "@FILES,%d", count);
    out_line(line);
}

static void cmd_get(const char* name)
{
    char path[40], line[64];
    struct stat st;
    snprintf(path, sizeof(path), "%s/%s", DATA_DIR, name);
    FILE* f = is_run_name(name) && stat(path, &st) == 0 ? fopen(path, "r") : nullptr;
    if (!f) { snprintf(line, sizeof(line), "@ERR,GET,%s", name); out_line(line); return; }

    // The page reads exactly <size> bytes after the @BEGIN line, so the whole transfer is
    // one locked message; the live stream is paused because it runs on this same task.
    // If any write fails, stop at once and send no @END: the page only keeps a file whose
    // exact size is followed by the matching @END line, so a gap can never be saved.
    static char buf[1024];
    out_begin();
    int n = snprintf(line, sizeof(line), "@BEGIN,%s,%ld\n", name, (long)st.st_size);
    bool ok = out(line, n);
    size_t k;
    while (ok && (k = fread(buf, 1, sizeof(buf), f)) > 0) ok = out(buf, k);
    if (ok) {
        n = snprintf(line, sizeof(line), "\n@END,%s\n", name);
        out(line, n);
    }
    out_end();
    fclose(f);
}

static void cmd_del(const char* name)
{
    char line[64];
    const int seq = is_run_name(name) ? request_delete(name) : 0;
    if (seq) snprintf(line, sizeof(line), "@OK,DEL,%d", seq);
    else     snprintf(line, sizeof(line), "@ERR,DEL,%.40s", name);
    out_line(line);
}

// ------------------------------------------------------------------------------- task
static void usb_task(void*)
{
    char cmd[80];
    int cmd_len = 0;
    bool live = false;
    int64_t live_until = 0;
    uint32_t tail = live_head;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));
        const int64_t now_ms = esp_timer_get_time() / 1000;

        // Commands
        char in[64];
        int n;
        while ((n = in_read(in, sizeof(in))) > 0) {
            for (int i = 0; i < n; i++) {
                const char c = in[i];
                if (c == '\r') continue;
                if (c != '\n') { if (cmd_len < (int)sizeof(cmd) - 1) cmd[cmd_len++] = c; continue; }
                cmd[cmd_len] = 0;
                cmd_len = 0;
                if (strcmp(cmd, "HELLO") == 0) {
                    char line[64];
                    snprintf(line, sizeof(line), "@HELLO,sticks3-imu-logger,%s", net_ssid());
                    out_line(line);
                } else if (strcmp(cmd, "LIVE 1") == 0) {
                    if (!live) tail = live_head;
                    live = true;
                    live_until = now_ms + LIVE_TIMEOUT_MS;
                } else if (strcmp(cmd, "LIVE 0") == 0) {
                    live = false;
                } else if (strcmp(cmd, "LIST") == 0) {
                    cmd_list();
                } else if (strncmp(cmd, "GET ", 4) == 0) {
                    cmd_get(cmd + 4);
                } else if (strncmp(cmd, "DEL ", 4) == 0) {
                    cmd_del(cmd + 4);
                } else if (cmd[0]) {
                    out_line("@ERR,UNKNOWN");
                }
            }
        }

        // Live stream at the full sample rate
        if (live && now_ms > live_until) live = false;
        if (!live) { tail = live_head; continue; }
        const uint32_t head = live_head;
        if (head - tail > LIVE_N - 8) tail = head - (LIVE_N - 8);
        static char msg[2048];
        int len = 0;
        const int st = status_fields(msg + 3, 300);
        if (st > 0) { memcpy(msg, "@S,", 3); len = 3 + st; msg[len++] = '\n'; }
        for (; tail != head; tail++) {
            if (len > (int)sizeof(msg) - 80) break;      // rest goes in the next message
            memcpy(msg + len, "@L,", 3);
            len += 3;
            len += sample_line(msg + len, 64, live_ring[tail % LIVE_N]);
        }
        out_begin();
        out(msg, len);
        out_end();
    }
}

void usb_start(void)
{
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.rx_buffer_size = 1024;
    cfg.tx_buffer_size = 4096;
    const esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    if (err == ESP_OK) usb_serial_jtag_vfs_use_driver();
    else printf("# usb driver not installed: %s\n", esp_err_to_name(err));
    xTaskCreatePinnedToCore(usb_task, "usb", 6144, nullptr, 4, nullptr, 0);
}
