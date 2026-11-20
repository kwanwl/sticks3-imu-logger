# StickS3 IMU Viewer and Logger

[![Build](https://github.com/kwanwl/sticks3-imu-logger/actions/workflows/build.yml/badge.svg)](https://github.com/kwanwl/sticks3-imu-logger/actions/workflows/build.yml)

Firmware for the **M5Stack StickS3** that shows, records and streams data from its BMI270 motion sensor (IMU), for physics experiments with acceleration and angular velocity.

- 4 screen pages: acceleration, angular velocity and angles, 3-D orientation, recording
- Records 5–60 s runs at 100 Hz to the StickS3's flash as CSV files
- Battery level on every page
- Wi-Fi hotspot from power-on, with a web page that follows the StickS3: live graphs, a 3-D acceleration arrow, angle dials, a 3-D model that turns with the device, recordings to download, Save CSV
- No app or internet needed: any phone or laptop browser works; when Wi-Fi is busy, the same page works over the USB cable

**Contents:** [Buttons](#1-buttons) · [Pages](#2-the-pages) · [Web page](#3-the-web-page) · [Data format](#4-data-format) · [Installing the firmware](#5-installing-the-firmware) · [Building from source](#6-building-from-source) · [Arduino version](#arduino-version) · [How the code works](#7-how-the-code-works) · [Troubleshooting](#8-troubleshooting) · [Further reading](#9-further-reading)

---

## 1. Buttons

Button and port locations: see M5Stack's [StickS3 product page](https://docs.m5stack.com/en/core/StickS3).

| Button | Where it is | Action |
| --- | --- | --- |
| **KEY2** | Long side of the case | Next page (1 → 2 → 3 → 4 → 1). Ignored while recording or zeroing. |
| **KEY1**, press | Blue button below the screen | Pages 2–3: zero the angles (hold still about 1 s). Page 4: start a recording. |
| **KEY1**, hold | | Page 3: switch between sensor fusion and gyro only. Page 4: change the recording length (5 → 10 → 20 → 30 → 60 s) |
| **Power**, press / double-press | End of the case, next to USB-C | Turn on / turn off |
| **Power**, hold | | With USB connected: download mode for flashing (green LED blinks) |

---

## 2. The pages

**Top line of every page:** the page number and name on the left, and the **battery** on the right: an icon filled to the charge level (green above 50 %, yellow above 20 %, red below) and the percentage. **On USB power** the icon turns cyan and a yellow **lightning bolt** replaces the % sign (e.g. `87⚡`): the battery is charging. The percentage is estimated from the battery voltage and smoothed; it reads a little high while charging and a little low while Wi-Fi is busy.

### Page 1 – Acceleration (g)
Shows ax, ay, az in **g**, the standard-gravity unit (1 g = g<sub>n</sub> = 9.80665 m/s², by definition): X red, Y green, Z cyan. The bottom line shows the hotspot name and the web address, e.g. `IMU-DA5734  192.168.4.1`.

Lying still, the axis pointing up reads about **+1 g**. An accelerometer measures every force except gravity (here the push of the table), so it reads +1 g at rest and about 0 g in free fall.

### Page 2 – Angular velocity and angle (°/s, °)
Each row is one axis: **angular velocity** ω in °/s, then **angle** θ in degrees since the last zeroing.

- θ = ∫ (ω − offset) dt, worked out separately for each axis.
- **Press KEY1 and hold still for 1 s** to measure the gyro offset and set the angles to 0. If it moved, the screen shows *Moved – try again*.
- Accurate when the rotation is **mainly about one axis** (pendulum, turntable, door). With rotation about several axes, the per-axis sums are wrong because 3-D rotations depend on their order; compare with page 3.

### Page 3 – Orientation (roll / pitch / yaw)
The full 3-D orientation as **roll (X), pitch (Y), yaw (Z)** in degrees. Every gyro sample is applied, in order, to a *quaternion*, so rotations about several axes are combined correctly (unlike page 2). Two modes; **hold KEY1** to switch (the title shows `fused` or `gyro`):

| Mode | Roll and pitch | Yaw (heading) | Drift |
| --- | --- | --- | --- |
| **Sensor fusion** (default) | **True tilt relative to gravity**: shows which side is really up | Relative to the heading when KEY1 was pressed | Roll and pitch corrected continuously; yaw drifts slowly |
| **Gyro only** | Relative to the pose when KEY1 was pressed | Same | All three drift slowly (a few degrees per minute) |

- **How the fusion works:** the gyro drives the orientation; the accelerometer's gravity direction gently turns roll and pitch back toward the truth (a *Mahony complementary filter*, time constant about 1 s). While the device accelerates (reading more than 0.15 g away from 1 g, e.g. swinging, spinning, thrown) the accelerometer is not just gravity, so only the gyro is used until it settles. Yaw has no reference without a compass (the StickS3 has none).
- **KEY1 (press):** hold still about 1 s; measures the gyro offset, zeroes pages 2 and 3, and in fusion mode sets roll and pitch to the measured tilt and yaw to 0.
- Angles use the Z-Y-X (aircraft) convention: yaw, then pitch, then roll.
- Near **pitch ±90°**, roll and yaw jump around ("gimbal lock"): a limitation of Euler angles, not a fault.

**Class demos:** (1) zero, then turn +90° about X, +90° about Y, −90° about X, −90° about Y: page 2 shows about 0, 0, 0; page 3 shows that the StickS3 has really turned. (2) Leave it tilted for a few minutes in each mode: gyro-only roll and pitch drift, fused ones stay put; yaw drifts in both.

### Page 4 – Record
- Shows the hotspot name and Wi-Fi channel (`IMU-DA5734  ch6`), the address `192.168.4.1` and how many browsers are connected (`web:1`).
- **Hold KEY1** to choose the length (default **10 s**); **press KEY1** to start.
- A red bar shows progress. Then the screen shows the file and row count, e.g. `RUN03.CSV 1000 rows`.
- Files survive power-off and firmware updates. Two limits apply, whichever comes first: **99 files** (`RUN01`–`RUN99`) and the **~5 MB storage space**. A 10 s run is about 45 KB, a 60 s run about 270 KB, so roughly 15–18 runs of 60 s fit; the exact number depends on the data. When either limit is reached the screen shows an error; download and delete old runs.
- If saving fails (e.g. storage full), the error stays on the screen until the next KEY1 press, and no partial file is left behind.

---

## 3. The web page

The page works over **Wi-Fi** (any phone or laptop) or over the **USB cable** (Chrome or Edge on a computer); both show the same views.

**Over Wi-Fi**

1. Turn the StickS3 on. The hotspot starts at power-on; its name **IMU-xxxxxx** and the address are on page 1 (bottom line) and page 4.
2. On a phone or laptop, join that Wi-Fi network (no password). If the phone warns "no internet", choose to **stay connected**.
3. Open **http://192.168.4.1**.

**Over USB** (when Wi-Fi is busy, or for a laptop without Wi-Fi)

1. Connect the StickS3 to the computer with a USB-C data cable.
2. In Chrome or Edge open **<https://kwanwl.github.io/sticks3-imu-logger/usb/>** (needs internet once to load).
3. Click **Connect USB** and choose the port named like `USB JTAG/serial debug unit`.

Connecting does not restart the StickS3, so a recording in progress carries on. Live graphs, the views and downloading or deleting recordings work exactly as over Wi-Fi, and the live view gets every sample (100 Hz).

**Many devices in one room:** every StickS3 has its own name, made from the last 6 hex digits of its factory MAC address (e.g. `IMU-DA5734`), so names do not repeat. The name never changes and is the same whichever firmware version is flashed, so it can go on a sticker. The hotspots are spread over Wi-Fi channels 1, 6 and 11 (chosen from the same address; about a third of the devices on each), and each one is set up to use as little airtime as possible (see "How the code works"). Each StickS3 accepts up to 4 browsers. If the live view still lags with a whole class, use the USB connection.

The page has five buttons: **Accel, Gyro, Orient, Record, Live**. With **Follow StickS3** ticked (the default), KEY2 on the StickS3 also switches the browser; a dot marks the StickS3's current page. Tapping a button shows that view and unticks Follow until you tick it again.

| View | Shows | StickS3 page |
| --- | --- | --- |
| Accel | 3-D arrow of the acceleration vector on a StickS3 model with its X/Y/Z axes, \|a\| in g, acceleration graphs | 1 |
| Gyro | Dials for θx, θy, θz (positive = anticlockwise, right-hand rule), angular-velocity graphs with the offset removed | 2 |
| Orient | 3-D StickS3 model that follows the device's orientation (sensor fusion: the right side up, grey square = level ground; or gyro only: relative to the zeroing pose), roll / pitch / yaw, and which mode is active | 3 |
| Record | Recording progress, length and last file; saved runs with **Download** and **Delete** (deleting needs KEY1 on the StickS3, see below) | 4 |
| Live | Acceleration and angular-velocity graphs together | browser only |

- The live view shows the last 10 s: **25 samples per second over Wi-Fi** (every 4th sample, to keep Wi-Fi traffic low with many devices) and the **full 100 Hz over USB**. Over Wi-Fi, events shorter than about 40 ms (an impact, the moment of release) can be missed or look smaller. **Recordings always keep the full 100 Hz.**
- **Deleting a recording** needs the StickS3 in your hand: after **Delete**, its screen asks `Del RUN03.CSV? KEY1`. Press **KEY1** to delete; KEY2, or no answer within 10 s, cancels. This stops anyone else on the open hotspot from deleting your runs.
- **Downloads over USB are checked:** a file is saved only if it arrived complete; otherwise the page says *Download failed* and you can try again.
- **Drag** a 3-D view to turn the camera.
- **Pause** freezes the graphs. **Save CSV** downloads what is on them as `live_YYYYMMDD-HHMMSS.csv` (25 Hz over Wi-Fi, 100 Hz over USB; press Pause first to capture a moment). On iPhone the file goes to *Files → Downloads*.

### StickS3 axes
Measured on a StickS3 (19 Nov 2026) and used by the 3-D model. Hold it with the screen facing you and USB-C at the bottom:

| Axis | Points toward | Check: reads +1 g when |
| --- | --- | --- |
| **+X** | the right side, where KEY2 is | lying on the side opposite KEY2 (KEY2 facing up) |
| **+Y** | the top end, away from USB-C | standing on its end, USB-C down |
| **+Z** | out of the screen | lying flat, screen up |

These form a right-handed set. If a different board revision ever reads differently, change `MODEL` at the top of the script in `main/web/index.html` (`LONG`, `LONG_SIGN`, `SCREEN_SIGN`), then rebuild and flash.

---

## 4. Data format

Recordings (`RUNnn.CSV`) and Save CSV files have the same columns:

```csv
t_ms,ax_g,ay_g,az_g,wx_dps,wy_dps,wz_dps
0,0.0123,-0.0041,1.0034,0.312,-0.122,0.061
10,0.0118,-0.0052,1.0029,0.284,-0.154,0.073
```

| Column | Meaning | Unit |
| --- | --- | --- |
| `t_ms` | Time since the first row | ms |
| `ax_g`, `ay_g`, `az_g` | Acceleration along X, Y, Z | g (× 9.80665 for m/s²) |
| `wx_dps`, `wy_dps`, `wz_dps` | Angular velocity ω<sub>x</sub>, ω<sub>y</sub>, ω<sub>z</sub> about X, Y, Z (`w` stands for ω) | °/s (× π/180 for rad/s) |

- **Units and gravity:** readings are in g, the standard-gravity unit (9.80665 m/s², a fixed definition); convert with that number. Gravity where you are differs slightly (about 9.78 m/s² in Singapore), so use **local g** in physics predictions such as h = ½gT². At rest a StickS3 does not read exactly 1 g: per-axis offsets (up to about ±20 mg) and sensitivity error (about ±0.4 %) are larger than the gap between standard and local gravity.
- 100 Hz: one row about every 10 ms. The accelerometer itself runs at 200 Hz, so each row normally has a fresh reading. The Record view's *Sampling check* counts polls without new accelerometer data since power-on, and warns if the 200 Hz setting failed (the StickS3 then also shows *Accel not at 200 Hz* at start-up).
- Values are what the BMI270 reports, with no software offsets: the firmware clears M5Unified's start-up offsets, and the gyro offset measured by KEY1 is used only for the angle pages. Students can therefore find the sensor's own offsets (e.g. the mean gyro reading of a still recording).
- **Axes are M5Unified's StickS3 axes, not the BMI270 chip axes in the datasheet.** M5Unified turns the chip axes by 90° about Z for this board: X = chip +Y, Y = chip −X, Z = chip +Z. So the datasheet's orientation figures (pages 151–152) apply only after swapping X and Y as above. Check the directions on your device with the "six faces" test (lay it on each face and see which axis reads +1 g).

---

## 5. Installing the firmware

No software is needed: the firmware installs from a web page. You need **Chrome or Edge on a computer** (Windows, macOS, Linux or ChromeOS; not Safari, Firefox, phones or iPads) and a **USB-C data cable** (many cables only charge).

### Option A: one-click installer (recommended)

1. Open **<https://kwanwl.github.io/sticks3-imu-logger/flasher/>**.
2. Connect the StickS3 and close the Arduino IDE, serial monitors and other flashers.
3. Click **Install**, choose the port named like `USB JTAG/serial debug unit` (macOS `cu.usbmodem…`, Windows `COM…`) and confirm.
4. Wait for "Installation complete" (about 30 s). The StickS3 restarts on page 1.

The installer never erases the flash, so saved recordings are kept.

### Option B: Espressif's web flasher

The ready-built image is **`firmware/sticks3-imu-logger.bin`**: bootloader, partition table and program merged into one file, written at **address `0x0`**.

1. Download `firmware/sticks3-imu-logger.bin` from this repository (open the file, then the download button).
2. Open <https://espressif.github.io/esptool-js/>.
3. Connect the StickS3, click **Connect** and choose its port (as above).
4. Under the file list set **Flash Address** to `0x0` and choose `sticks3-imu-logger.bin`.
5. Click **Program** and wait for it to finish, then press the power button once (or double-press to turn off and press to turn on).

Do not click **Erase Flash**: it deletes the recordings saved on the StickS3.

### If the installer cannot connect, or the screen stays blank

| Problem | Fix |
| --- | --- |
| No port appears, or connecting fails | Enter **download mode**: with USB connected, hold the **power button** (next to USB-C) about 2 s until the internal green LED blinks, then try again |
| Finished, but the screen stays blank | Still in download mode: unplug, **double-press** the power button to turn off, press once to turn on |
| Still no port | Another cable or USB port; on Linux add your user to the `dialout` group and log in again |
| *Storage prepared* on the first start | Normal on a new or erased StickS3: the recordings area is being set up |

### Updating the hosted installer (maintainers)

After rebuilding the firmware:

1. Make the merged image: `idf.py merge-bin -o sticks3-imu-logger.bin && cp build/sticks3-imu-logger.bin firmware/`.
2. Change `"version"` in `flasher/manifest.json` (and the version line in `flasher/index.html`).
3. Commit and push; GitHub Pages serves the new image within a minute or two.

## 6. Building from source

Full step-by-step instructions (installing ESP-IDF, ports on each system, flashing, troubleshooting, making a release): **[BUILDING.md](BUILDING.md)**. The Arduino IDE version has its own short guide: **[arduino/README.md](arduino/README.md)**.

In brief, with **ESP-IDF v6.1** ready in the terminal:

```bash
idf.py set-target esp32s3                  # first time only
idf.py -p PORT flash monitor               # e.g. /dev/cu.usbmodem1101, /dev/ttyACM0, COM5
```

### Project structure

```
sticks3-imu-logger/
  CMakeLists.txt        project file (with the ESP-IDF v6 workaround)
  sdkconfig.defaults    StickS3 settings: 8 MB flash, octal PSRAM, partitions, WebSocket
  partitions.csv        3 MB app + ~4.9 MB FAT storage for recordings
  dependencies.lock     exact component versions
  main/
    app.h               shared definitions between the three source files
    main.cpp            screen pages, button actions, saving recordings
    imu.cpp             100 Hz IMU task: buttons, angles, quaternion, recording, live buffer
    net.cpp             storage, Wi-Fi hotspot, web server, live WebSocket stream
    usb.cpp             the same live stream and recordings over the USB cable
    web/index.html      the web page (built into the firmware; also served by GitHub Pages for USB)
    idf_component.yml   M5Unified dependency
  BUILDING.md           how to build and flash from source
  arduino/
    README.md           Arduino IDE guide
  arduino/sticks3_imu_logger/   Arduino IDE version (see "Arduino version")
    sticks3_imu_logger.ino      entry point: setup() calls app_run()
    sketch.yaml                 board settings and versions for arduino-cli
    app.h, *.cpp, partitions.csv, index_html.h   GENERATED copies of the files above
  tools/
    sync_arduino.py     copies main/ into the Arduino sketch, generates index_html.h
    make_docx.py        makes Word versions of the documents (needs pandoc)
  .github/workflows/build.yml   CI: builds both versions on every push (GitHub Actions)
  firmware/
    sticks3-imu-logger.bin   merged image for browser flashing (address 0x0)
  usb/
    index.html          short address for the USB version of the web page (GitHub Pages)
  flasher/
    index.html, manifest.json   one-click web installer (GitHub Pages, ESP Web Tools)
```

### Easy things to change

| Change | Where |
| --- | --- |
| Sample rate (100 Hz) | `SAMPLE_HZ` in `main/app.h` |
| Wi-Fi live view rate (25 Hz; USB always sends 100 Hz) | `LIVE_HZ` in `main/app.h` |
| Wi-Fi transmit power, beacon interval | `TX_POWER_QDBM`, `BEACON_TU` in `main/net.cpp` |
| Longest recording (60 s) | `MAX_REC_S` in `main/app.h` |
| Recording lengths offered | `DURATIONS[]` in `main/main.cpp` |
| Hotspot name, password, channel | `net_start()` in `main/net.cpp` |
| Graph window (10 s), colours, layout, 3-D model axes | `main/web/index.html` |

After any of these changes, run `python3 tools/sync_arduino.py` so the Arduino version gets them too.

### Arduino version

`arduino/sticks3_imu_logger/` is the same firmware for the Arduino IDE (same source files, behaviour, web page, CSV format and flash layout; recordings are kept when switching). Setup, the required Tools settings (ESP32S3 Dev Module, OPI PSRAM, 8 MB flash, Custom partitions) and the differences from the ESP-IDF build are in **[arduino/README.md](arduino/README.md)**. `main/` is the single source: edit there and run `python3 tools/sync_arduino.py`.

---

## 7. How the code works

Three FreeRTOS tasks share data through `main/app.h`:

| Task | File | Core / priority | Job |
| --- | --- | --- | --- |
| `imu` | `imu.cpp` | 1 / 10 | Every 10 ms: read buttons and IMU (the only I2C user), integrate θ and the quaternion, zeroing, recording, live ring buffer, publish a snapshot |
| `main` | `main.cpp` | default | Screen at 10 Hz, button actions, write the recording to `RUNnn.TMP`, then rename it to `RUNnn.CSV` |
| `live` + HTTP server | `net.cpp` | 0 / 5 | Every 100 ms send a status line and the new samples (thinned to 25 Hz) to all WebSocket clients; serve the page and files |
| `usb` | `usb.cpp` | 0 / 4 | Every 100 ms read commands from the USB cable; send files, and the live stream (all samples, 100 Hz) while a page asks for it |

- **Hand-over between tasks:** the IMU task publishes `ImuView` under a short spinlock (`imu_view()`); button events, page, recording progress and zeroing state are atomics.
- **Recording** fills a 168 KB PSRAM buffer at 100 Hz, then `main.cpp` writes the CSV, so flash writes never disturb the sample timing. The file is written as `.TMP` and renamed only if every write succeeded; leftover `.TMP` files are removed at start-up.
- **Storage** is mounted without formatting (with one retry). If that fails and the partition is **blank** (a new or erased device), it is formatted and the screen shows *Storage prepared*. If it **holds data** but will not mount (damage, or a future incompatible format), it is **never formatted automatically**: the screen shows *Storage error*, recording is off, and the user can erase it deliberately on the Record page (hold KEY1, then hold KEY1 again within 5 s).
- **Saving before starting:** the main loop saves a finished recording before it handles a new KEY1 press, so a quick second press cannot overwrite a run that has not been saved yet.
- **Live stream back-pressure:** at most 2 messages may wait to be sent and the send timeout is 1 s, so a sleeping phone or a download cannot make memory grow; the stream drops samples instead (recordings are unaffected).
- **Wi-Fi airtime:** with a whole class of hotspots the 2.4 GHz band is shared, so each StickS3 keeps its airtime small: 802.11b rates off (beacons and broadcasts at 6 Mbps instead of 1 Mbps), beacons every 200 TU instead of 100, transmit power 11 dBm instead of about 20 (students sit close; devices across the room interfere less), and the live view thinned to 25 Hz in 10 messages a second.
- **Battery:** read from the M5PM1 power chip every 2 s on the IMU task, which is the only task that uses the I2C bus. The percentage is smoothed (each reading moves it 20 % of the way). "Charging" means USB power is present (supply above 4.5 V): the chip's own charge flag flips between charging and not charging about every second on the StickS3, so it is not used.
- **USB link:** both builds install ESP-IDF's USB-Serial-JTAG driver and route the console through it. Without a computer, output is dropped, so the StickS3 never waits on battery. Opening the port normally does not reset the board; only switching the DTR/RTS signals off does (the flashers' auto-reset).
- **Sampling:** the BMI270 accelerometer is set to 200 Hz (`ACC_CONF = 0xA9`; reset value 100 Hz would beat against the 100 Hz polling). The setting is read back (one retry); if it did not stick, the device keeps running at 100 Hz and warns on screen and on the web page. Polls without new accelerometer data are counted (`stale` in the status line).
- **Angles:** θ per axis is a simple sum of ω·dt. Two quaternions are updated with every sample, each rotated by |ω|·dt about ω in the device's own frame (`quat_step()` in `imu.cpp`): a gyro-only one, and a fused one whose rate is first corrected toward the accelerometer's gravity direction (`fusion_correction()`, Mahony filter, gain 1/s, skipped when the reading is more than 0.15 g from 1 g). The screen and web page convert the selected one to Z-Y-X roll / pitch / yaw.

### Web interface

| Endpoint | Purpose |
| --- | --- |
| `GET /` | Web page (`main/web/index.html`) |
| `GET /api/files` | JSON list of recordings: `[{"name":"RUN01.CSV","size":61234}]` |
| `GET /dl?f=RUN01.CSV` | Download a recording |
| `POST /del?f=RUN01.CSV` | Delete a recording |
| `GET /ws` | WebSocket live stream |

Each WebSocket message (every 100 ms) is text: one status line, then one line per sample (25 Hz).

```text
#S,page,zero,rec_on,rec_n,rec_target,dur_s,last_rows,last_file,bias_x,bias_y,bias_z,theta_x,theta_y,theta_z,q_w,q_x,q_y,q_z,stale,acc200,fused,bat_pct,bat_mv,charging,del_seq,del_state,del_name
t_ms,ax,ay,az,wx,wy,wz
```

`page` 0–3 = Accel, Gyro, Orient, Record; `zero` 0 busy / 1 done / 2 moved; `q` is the quaternion the Orient page shows, fused when `fused` = 1, gyro-only when 0; `stale` = polls since power-on without new accelerometer data; `acc200` = 1 if the accelerometer was confirmed at 200 Hz; `bat_pct` 0–100 (−1 unknown), `bat_mv` millivolts, `charging` 0/1; sample `t_ms` is time since boot. Only names of the form `RUNnn.CSV` can be listed, downloaded or deleted. `del_seq`, `del_state`, `del_name`: the latest delete request and its state (1 waiting for KEY1, 2 deleted, 3 cancelled or timed out, 4 failed).

### USB interface

The USB page (Web Serial) sends one command per line; the StickS3 replies with lines starting with `@` (other lines, such as `# …` log messages, are ignored):

| Command | Reply |
| --- | --- |
| `HELLO` | `@HELLO,sticks3-imu-logger,<hotspot name>` |
| `LIST` | `@F,<name>,<size>` per recording, then `@FILES,<count>` |
| `GET <name>` | `@BEGIN,<name>,<size>`, exactly `<size>` bytes of the file, a newline, `@END,<name>`. If the computer stops reading, the StickS3 abandons the transfer without `@END`, and the page discards it |
| `DEL <name>` | `@OK,DEL,<request number>` (the StickS3 now asks for KEY1) or `@ERR,DEL,<name>` (bad name, or another delete is waiting). The outcome appears as `del_state` in the status line |
| `LIVE 1` / `LIVE 0` | Starts / stops the live stream: `@S,…` and `@L,…` lines with the same fields as the WebSocket stream, but every sample (100 Hz). The page repeats `LIVE 1` every 3 s; the stream stops by itself 10 s after the last one |

---

## 8. Troubleshooting

| Problem | Fix |
| --- | --- |
| Can't find the `IMU-xxxxxx` Wi-Fi | Wait a few seconds after power-on; the name is at the bottom of page 1 |
| Phone keeps leaving the Wi-Fi | Choose "stay connected", or turn off mobile data while using it |
| `192.168.4.1` won't load | Check the phone is still on `IMU-xxxxxx`; type `http://`, not `https://` |
| Browser does not change view with KEY2 | Tick **Follow StickS3** at the top of the page |
| 3-D model turns the wrong way | Check the axes against the "StickS3 axes" table; adjust `MODEL` in `main/web/index.html` |
| Graphs stutter or lag with many devices | Close other tabs; up to 4 browsers can connect to one StickS3. With a whole class on Wi-Fi, use the USB page instead |
| USB page: no port, or *USB needs Chrome or Edge* | Use Chrome or Edge on a computer and a USB-C data cable; close the Arduino IDE and other serial programs |
| Battery shows `--%` | The power chip did not answer; turn the StickS3 off and on |
| Angles drift on pages 2–3 | Normal for a gyro: re-zero with KEY1 (hold still) |
| Screen blank after flashing | Still in download mode: unplug, double-press power to turn off, press to turn on |
| `99 runs: delete some` or `Save failed (full?)` | Download, then delete old runs in the Record view |
| `Storage prepared` at start-up | The storage was blank (new or erased StickS3) and has been set up |
| `Storage error` | The storage holds data but cannot be read; it is left untouched and recording is off. To start again, go to the Record page, hold KEY1, then hold KEY1 again within 5 s: **this erases all runs** |
| `No storage` | The storage partition could not be used at all; re-flash with the web installer |
| Delete does nothing | Press KEY1 on the StickS3 within 10 s when it shows `Del RUNnn.CSV? KEY1` |
| *Download failed* (USB) | The transfer was interrupted; click Download again |
| `Wi-Fi not available` | The hotspot failed to start; turn the StickS3 off and on |
| `web page failed` / `Web page failed` | The hotspot is visible but the web server did not start; turn the StickS3 off and on |
| Battery runs down quickly | The hotspot is always on; charge between lessons |

---

## 9. Further reading

- Bosch Sensortec, [BMI270 datasheet](https://www.bosch-sensortec.com/products/motion-sensors/imus/bmi270/) (BST-BMI270-DS000): ranges and sensitivity p. 9–12, data format p. 26–27, axis directions p. 151–152 (page numbers from revision 1.2).
- Analog Devices, [Accelerometer and Gyroscopes Sensors: Operation, Sensing, and Applications](https://www.analog.com/en/resources/technical-articles/accelerometer-and-gyroscopes-sensors-operation-sensing-and-applications.html).
- Staacks et al., [Advanced tools for smartphone-based experiments: phyphox](https://arxiv.org/abs/1804.06239), Physics Education 53(4), 045009 (2018).
