// main.cpp - screen and user interface for the StickS3 IMU viewer and logger
//
// Pages on the StickS3 (KEY2, the side button, cycles them):
//   1 Accel   acceleration in g
//   2 Gyro    angular velocity (deg/s) and the angle about each axis since zeroing:
//             theta = integral of (omega - offset) dt, per axis (ignores rotation order)
//   3 Orient  orientation as roll / pitch / yaw (Z-Y-X Euler angles) from a quaternion updated
//             with every gyro sample in order, so any rotation is handled. Two modes (hold KEY1):
//             fused (default): gravity from the accelerometer corrects roll and pitch, so they
//                    show the true tilt (which side is up); yaw is relative to the zeroing heading
//             gyro:  gyro only, everything relative to the zeroing pose; drifts slowly
//   4 Record  record ax..wz at 100 Hz to flash as RUNnn.CSV
//
// KEY1 (the blue button):
//   pages 2-3  measure the gyro offset (hold still about 1 s), then zero the angles
//              (Orient, fused: roll/pitch snap to the measured tilt, yaw = 0)
//   page 3     hold: switch Orient between fused and gyro-only
//   page 4     press: start recording; hold: change the length (5/10/20/30/60 s)
//
// The Wi-Fi hotspot starts at power-on. Every page streams live to http://192.168.4.1, and the
// web page follows the page shown here. Sampling runs on its own task (imu.cpp) and the
// web server in net.cpp; this file only draws the screen and reacts to button events.
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#include <M5Unified.h>
#include "app.h"

static constexpr int PAGES = 4;
enum { PAGE_ACCEL, PAGE_GYRO, PAGE_ORIENT, PAGE_RECORD };
static constexpr int DURATIONS[] = {5, 10, 20, 30, 60};    // seconds, cycled by holding KEY1
static constexpr int N_DUR = sizeof(DURATIONS) / sizeof(DURATIONS[0]);
static constexpr int DEFAULT_DUR_I = 1;                    // 10 s

// Published for the web status line (see app.h).
std::atomic<int> ui_page{PAGE_ACCEL};
std::atomic<int> ui_dur_s{DURATIONS[DEFAULT_DUR_I]};
std::atomic<int> ui_last_rows{0};
std::atomic<int> ui_last_run{0};
std::atomic<bool> ui_orient_fused{true};

static int page = PAGE_ACCEL;
static int dur_i = DEFAULT_DUR_I;
static bool storage_ok = false;
// Recorder error shown on the Record page until the next KEY1 press ("" = none).
static char rec_error[24] = "";

static constexpr float DEG = 180.0f / (float)M_PI;

// Screen layout: 240 x 135 landscape, text size 2 (12 x 16 px characters, 20 per line).
static constexpr int Y_TITLE = 4, Y_ROW1 = 34, ROW_STEP = 26, Y_BOTTOM = 116;

// Convert the orientation quaternion to Z-Y-X Euler angles in degrees:
// e[0] = roll about X, e[1] = pitch about Y, e[2] = yaw about Z.
static void quat_to_euler(const float* q, float* e)
{
    const float w = q[0], x = q[1], y = q[2], z = q[3];
    e[0] = std::atan2(2 * (w * x + y * z), 1 - 2 * (x * x + y * y)) * DEG;
    e[1] = std::asin(std::fmax(-1.0f, std::fmin(1.0f, 2 * (w * y - z * x)))) * DEG;
    e[2] = std::atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z)) * DEG;
}

static void text(int y, uint16_t colour, const char* s)
{
    M5.Display.setTextColor(colour, TFT_BLACK);
    M5.Display.drawString(s, 4, y);
}

// printf-style line, padded to the full 20 characters so it overwrites the previous text
// without clearing the screen (avoids flicker).
static void line(int y, uint16_t colour, const char* fmt, ...)
{
    char buf[48];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) n = 0;
    if (n > (int)sizeof(buf) - 1) n = sizeof(buf) - 1;
    while (n < 20) buf[n++] = ' ';
    buf[n] = 0;
    text(y, colour, buf);
}

// Parts of a page that only change when the page changes.
static void draw_static(void)
{
    static const char* title[PAGES] = {
        "ACCEL (g)       1/4", "GYRO  dps   deg 2/4", "ORIENT fused    3/4", "RECORD          4/4"};
    M5.Display.fillScreen(TFT_BLACK);
    text(Y_TITLE, TFT_WHITE, page == PAGE_ORIENT && !ui_orient_fused ? "ORIENT gyro     3/4" : title[page]);
    if (page == PAGE_GYRO)   text(Y_BOTTOM, TFT_DARKGREY, "KEY1: zero angles");
    if (page == PAGE_ORIENT) text(Y_BOTTOM, TFT_DARKGREY, "KEY1 zero/hold mode");
    if (page == PAGE_ACCEL) {
        // Hotspot name and web address in smaller text (9 px per character) so both fit:
        // "IMU-xxxxxx  192.168.4.1" is 23 characters = 207 px.
        char foot[32];
        if (!net_ssid()[0])     snprintf(foot, sizeof(foot), "Wi-Fi not available");
        else if (!net_web_ok()) snprintf(foot, sizeof(foot), "%s  web page failed", net_ssid());
        else                    snprintf(foot, sizeof(foot), "%s  192.168.4.1", net_ssid());
        M5.Display.setTextSize(1.5f);
        text(Y_BOTTOM + 2, TFT_LIGHTGREY, foot);
        M5.Display.setTextSize(2);
    }
    if (page == PAGE_RECORD) {
        if (!net_ssid()[0])     line(30, TFT_RED, "Wi-Fi not available");
        else if (!net_web_ok()) line(30, TFT_RED, "Web page failed");
        else                    line(30, TFT_WHITE, "%s  ch%d", net_ssid(), net_channel());
    }
}

// Parts of a page that change with every refresh (10 times a second).
static void draw_values(const ImuView& v)
{
    static const uint16_t colour[3] = {TFT_RED, TFT_GREEN, TFT_CYAN};
    static const char axis[3] = {'X', 'Y', 'Z'};
    static const char* name[3] = {"Roll ", "Pitch", "Yaw  "};
    const float a[3] = {v.s.ax, v.s.ay, v.s.az};
    const float w[3] = {v.s.wx - v.bias[0], v.s.wy - v.bias[1], v.s.wz - v.bias[2]};

    if (page != PAGE_RECORD) {
        float e[3];
        quat_to_euler(ui_orient_fused ? v.qf : v.q, e);
        for (int i = 0; i < 3; i++) {
            const int y = Y_ROW1 + i * ROW_STEP;
            if (page == PAGE_ACCEL)     line(y, colour[i], "%c: %+7.3f", axis[i], a[i]);
            else if (page == PAGE_GYRO) line(y, colour[i], "%c%+7.1f%+8.1f", axis[i], w[i], v.theta[i]);
            else                        line(y, colour[i], "%s %c %+7.1f", name[i], axis[i], e[i]);
        }
        return;
    }

    // Record page: address and number of connected browsers, then the recorder.
    line(50, TFT_DARKGREY, "192.168.4.1  web:%d", net_ws_clients());
    line(72, TFT_WHITE, "Length: %d s (hold)", DURATIONS[dur_i]);
    if (imu_rec_running()) {
        const float t = imu_rec_count() / (float)SAMPLE_HZ;
        line(94, TFT_RED, "REC %4.1f / %d s", t, DURATIONS[dur_i]);
        const int target = imu_rec_target() > 0 ? imu_rec_target() : 1;
        M5.Display.fillRect(4, Y_BOTTOM, 232 * imu_rec_count() / target, 14, TFT_RED);   // progress bar
    } else {
        if (rec_error[0])          line(94, TFT_RED, "%s", rec_error);
        else if (ui_last_run > 0)  line(94, TFT_GREEN, "RUN%02d.CSV %d rows", ui_last_run.load(), ui_last_rows.load());
        else                       line(94, TFT_DARKGREY, "");
        line(Y_BOTTOM, TFT_DARKGREY, "KEY1: record");
    }
}

// Write the finished recording to the next free RUNnn.CSV (RUN01 ... RUN99).
// Same columns as the web page's Save CSV: t_ms, ax_g, ay_g, az_g, wx_dps, wy_dps, wz_dps.
// The data goes to RUNnn.TMP first and is renamed only when every write and the close
// succeeded, so a failed save (storage full, power cut) never leaves a partial .CSV for
// download. On failure the .TMP is deleted and the error stays on screen (rec_error).
static void save_recording(void)
{
    line(94, TFT_YELLOW, "Saving...");
    M5.Display.fillRect(0, Y_BOTTOM - 4, 240, 23, TFT_BLACK);   // clear the progress bar
    char path[40], tmp[40];
    int k;
    struct stat st;
    for (k = 1; k < 100; k++) {
        snprintf(path, sizeof(path), "%s/RUN%02d.CSV", DATA_DIR, k);
        if (stat(path, &st) != 0) break;                    // first name not in use
    }
    if (k == 100) { snprintf(rec_error, sizeof(rec_error), "99 runs: delete some"); return; }
    snprintf(tmp, sizeof(tmp), "%s/RUN%02d.TMP", DATA_DIR, k);

    FILE* f = fopen(tmp, "w");
    if (!f) { snprintf(rec_error, sizeof(rec_error), "Save failed"); return; }
    bool ok = fprintf(f, "t_ms,ax_g,ay_g,az_g,wx_dps,wy_dps,wz_dps\n") > 0;
    const Sample* r = imu_rec_data();
    const int n = imu_rec_count();
    for (int i = 0; i < n && ok; i++)
        ok = fprintf(f, "%lu,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f\n", (unsigned long)r[i].t_ms,
                     r[i].ax, r[i].ay, r[i].az, r[i].wx, r[i].wy, r[i].wz) > 0;
    ok = (fclose(f) == 0) && ok;
    if (ok) ok = rename(tmp, path) == 0;
    if (!ok) {
        unlink(tmp);
        snprintf(rec_error, sizeof(rec_error), "Save failed (full?)");
        printf("# save failed: %s\n", path);
        return;
    }

    ui_last_rows = n;
    ui_last_run = k;
    printf("# saved %s, %d rows\n", path, n);
}

// Remove temporary files left by a save that was interrupted (e.g. power off while saving).
static void remove_stale_tmp(void)
{
    char tmp[40];
    for (int k = 1; k < 100; k++) {
        snprintf(tmp, sizeof(tmp), "%s/RUN%02d.TMP", DATA_DIR, k);
        unlink(tmp);                                        // fails harmlessly if absent
    }
}

void app_run(void)
{
    auto cfg = M5.config();
    M5.begin(cfg);                                          // detects the StickS3, powers LCD and IMU
    M5.Display.setRotation(1);                              // landscape, 240 x 135
    M5.Display.setTextSize(2);
    M5.Display.fillScreen(TFT_BLACK);

    if (!M5.Imu.isEnabled()) {
        text(50, TFT_RED, "IMU not found");
        while (true) M5.delay(1000);
    }
    if (!imu_start()) {                                     // sampling is essential: stop here
        text(50, TFT_RED, "IMU task failed");
        while (true) M5.delay(1000);
    }
    if (!imu_accel_200hz()) {                               // usable, but sampling less reliable
        text(30, TFT_ORANGE, "Accel not at 200 Hz");
        text(50, TFT_DARKGREY, "(see web Record)");
        M5.delay(3000);
        M5.Display.fillScreen(TFT_BLACK);
    }

    // Storage: tell the user if it had to be formatted (first boot, or damaged), because
    // any recordings on it are gone. Without storage the other pages still work.
    switch (storage_init()) {
    case STORAGE_OK:
        storage_ok = true;
        remove_stale_tmp();
        break;
    case STORAGE_RESET:
        storage_ok = true;
        printf("# storage was formatted\n");
        text(30, TFT_ORANGE, "Storage was reset");
        text(50, TFT_DARKGREY, "(old runs erased)");
        M5.delay(3000);
        M5.Display.fillScreen(TFT_BLACK);
        break;
    case STORAGE_FAILED:
        printf("# storage not available\n");
        snprintf(rec_error, sizeof(rec_error), "No storage");
        break;
    }

    text(50, TFT_YELLOW, "Starting Wi-Fi...");
    if (!net_start()) printf("# Wi-Fi / web server not available\n");   // screen shows it
    draw_static();

    bool zeroing = false, was_recording = false;
    while (true) {
        // KEY2: next page (not while a measurement is running).
        if (ev_key2_click.exchange(false) && !zeroing && !imu_rec_running()) {
            page = (page + 1) % PAGES;
            ui_page = page;
            draw_static();
        }
        const bool k1 = ev_key1_click.exchange(false);
        const bool k1_hold = ev_key1_hold.exchange(false);

        // KEY1 on Gyro / Orient: start zeroing, then wait for the IMU task's result.
        if ((page == PAGE_GYRO || page == PAGE_ORIENT) && k1 && !zeroing) {
            imu_request_zero();
            zeroing = true;
            line(Y_BOTTOM, TFT_YELLOW, "Hold still...");
        }
        if (zeroing && imu_zero_result() != 0) {
            zeroing = false;
            if (imu_zero_result() == 2) { line(Y_BOTTOM, TFT_ORANGE, "Moved - try again"); M5.delay(1000); }
            line(Y_BOTTOM, TFT_DARKGREY, page == PAGE_ORIENT ? "KEY1 zero/hold mode" : "KEY1: zero angles");
        }

        // Hold KEY1 on Orient: switch between fused and gyro-only orientation.
        if (page == PAGE_ORIENT && k1_hold && !zeroing) {
            ui_orient_fused = !ui_orient_fused;
            draw_static();
        }

        // KEY1 on Record: hold changes the length, press starts recording.
        if (page == PAGE_RECORD && !imu_rec_running()) {
            if (k1_hold) {
                dur_i = (dur_i + 1) % N_DUR;
                ui_dur_s = DURATIONS[dur_i];
            }
            if (k1 && !storage_ok) {
                snprintf(rec_error, sizeof(rec_error), "No storage");
            } else if (k1) {
                rec_error[0] = 0;                           // a new attempt clears the old error
                if (imu_rec_start(DURATIONS[dur_i])) {
                    was_recording = true;
                    M5.Display.fillRect(0, Y_BOTTOM - 4, 240, 23, TFT_BLACK);
                } else {
                    snprintf(rec_error, sizeof(rec_error), "No record buffer");
                }
            }
        }
        // Recording finished (the IMU task clears the flag): write the file.
        if (was_recording && !imu_rec_running()) {
            was_recording = false;
            save_recording();
        }

        ImuView v;
        imu_view(&v);
        draw_values(v);
        M5.delay(100);                                      // 10 screen refreshes per second
    }
}

#ifndef ARDUINO
// ESP-IDF entry point. (The Arduino build calls app_run() from setup() in the .ino sketch.)
extern "C" void app_main(void)
{
    app_run();
}
#endif
