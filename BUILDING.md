# Building the StickS3 IMU Logger

How to build the firmware from source and put it on a StickS3. To just **install** the ready-built firmware, no build is needed: use the [web installer](https://kwanwl.github.io/sticks3-imu-logger/flasher/) (see the [README](README.md#5-installing-the-firmware)).

| Toolchain | Good for | Folder |
| --- | --- | --- |
| **ESP-IDF v6.1** | Full control; the reference build; making the web-installer image | repository root (`main/`) |
| **Arduino IDE 2** (or arduino-cli) | Students new to embedded code; quick edits | `arduino/sticks3_imu_logger/` ([its README](arduino/README.md)) |

Both build the **same firmware** from the same source files, with the same flash layout, so recordings on a StickS3 are kept when you switch between them.

**Contents:** [ESP-IDF](#1-esp-idf) · [Arduino IDE](#2-arduino-ide) · [Changing the code](#3-changing-the-code) · [Making a release](#4-making-a-release-maintainers) · [Troubleshooting](#5-troubleshooting)

---

## 1. ESP-IDF

### 1.1 Install ESP-IDF v6.1 (once)

Follow Espressif's *Get Started* guide for your system and choose **ESP-IDF v6.1** and target **ESP32-S3**:
<https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/get-started/index.html>

- **Windows / macOS / Linux:** the guide's installer sets up ESP-IDF, its Python environment and the compilers.
- **VS Code users:** the *ESP-IDF* extension can install it for you and gives Build / Flash / Monitor buttons.

### 1.2 Open a terminal with ESP-IDF ready

Every new terminal needs the ESP-IDF environment first. The command depends on how it was installed:

| Installed with | Command |
| --- | --- |
| ESP-IDF Installation Manager (macOS / Linux) | `source ~/.espressif/tools/activate_idf_v6.1.sh` |
| Classic install in `~/esp/esp-idf` (macOS / Linux) | `. ~/esp/esp-idf/export.sh` |
| Windows installer | open the **ESP-IDF 6.1 PowerShell** (or CMD) shortcut it created |

Check it worked: `idf.py --version` prints `ESP-IDF v6.1`.

### 1.3 Get the code

```bash
git clone https://github.com/kwanwl/sticks3-imu-logger.git
cd sticks3-imu-logger
```

(Or download the ZIP from the GitHub page and unpack it.)

### 1.4 First build

```bash
idf.py set-target esp32s3      # once: makes sdkconfig from sdkconfig.defaults
idf.py build
```

- The first build **needs internet**: the component manager downloads M5Unified 0.3.0 and M5GFX 0.3.0 into `managed_components/` (exact versions are pinned in `dependencies.lock`). It takes a few minutes; later builds only recompile what changed.
- A successful build ends with `Project build complete` and the size of `build/sticks3_imu_logger.bin`.

### 1.5 Find the StickS3's port

Connect the StickS3 with a **USB-C data cable**.

| System | Port looks like | How to list |
| --- | --- | --- |
| macOS | `/dev/cu.usbmodem1101` | `ls /dev/cu.usbmodem*` |
| Linux | `/dev/ttyACM0` | `ls /dev/ttyACM*` (add yourself to the `dialout` group once) |
| Windows | `COM5` | Device Manager → Ports (COM & LPT) |

### 1.6 Flash and watch the output

```bash
idf.py -p PORT flash monitor    # e.g. idf.py -p /dev/cu.usbmodem1101 flash monitor
```

- `flash` writes the bootloader, partition table and program; the StickS3 restarts.
- `monitor` shows its output; healthy start-up lines look like `# BMI270 ACC_CONF = 0xA9` and `# hotspot IMU-DA5734, channel 6, http://192.168.4.1`. Leave the monitor with **Ctrl+]**.
- After the first time, `idf.py -p PORT app-flash` is quicker: it replaces only the program.
- **Recordings are kept** by both, unless `partitions.csv` changes.

If it cannot connect, put the StickS3 in **download mode**: with USB connected, hold the power button (next to USB-C) about 2 s until the green LED blinks, then flash again. Afterwards double-press the power button to turn it off and press once to turn it on.

### 1.7 Everyday commands

| Command | Does |
| --- | --- |
| `idf.py build` | Compile what changed |
| `idf.py -p PORT app-flash` | Replace the program only (fast) |
| `idf.py -p PORT flash` | Bootloader, partition table and program |
| `idf.py -p PORT monitor` | Show the output (Ctrl+] to leave) |
| `idf.py menuconfig` | Change low-level settings (writes `sdkconfig`) |
| `idf.py fullclean` | Delete the build folder: use after changing `sdkconfig.defaults` or when a build behaves oddly |
| `idf.py merge-bin -o sticks3-imu-logger.bin` | One file for the web installer, in `build/` (see section 4) |

### 1.8 Notes on this project

- **C++ is fine.** The source files are `.cpp`; `idf.py build` compiles them with ESP-IDF's C++ compiler (C++26, `-std=gnu++26`). ESP-IDF's own C headers can be included directly. The entry point stays `extern "C" void app_main(void)` (at the end of `main/main.cpp`) because ESP-IDF calls it from C. C++ exceptions and RTTI are off by default (`idf.py menuconfig` can enable them); the project does not use them.
- **Settings** live in `sdkconfig.defaults` (flash size, PSRAM, partitions, WebSocket support). `sdkconfig` is generated and not in git; after editing `sdkconfig.defaults`, delete `sdkconfig` and run `idf.py set-target esp32s3` again.
- **ESP-IDF v6 workaround:** M5GFX/M5Unified include `esp_psram.h` without declaring the `esp_psram` component, which v6 rejects. The top-level `CMakeLists.txt` therefore contains `idf_build_set_property(__COMPONENT_REQUIRES_COMMON esp_psram APPEND)`. Keep it until M5Stack fixes their libraries.
- **Editor warnings:** VS Code's C/C++ checker (clang) may underline `-mlongcalls` or `sys/features.h`. Those come from the editor not knowing the ESP32 compiler; they are not build errors.

---

## 2. Arduino IDE

See **[arduino/README.md](arduino/README.md)**. In short: install the *esp32* board package 3.3.x and the *M5Unified* 0.3.0 library, open `arduino/sticks3_imu_logger/sticks3_imu_logger.ino`, choose **ESP32S3 Dev Module** with the listed Tools settings (OPI PSRAM, 8 MB flash, Custom partitions) and click Upload.

---

## 3. Changing the code

`main/` is the **single source** for both versions.

1. Edit files in `main/` (and `main/web/index.html` for the web page). Common changes are listed in the README under *Easy things to change*.
2. Build and flash with ESP-IDF to test.
3. Copy the changes into the Arduino sketch: `python3 tools/sync_arduino.py` (`--check` only reports whether it is out of date).

The web page is built into the firmware, so after editing `main/web/index.html` rebuild and flash. The USB version of the page on GitHub Pages updates when the repository is pushed.

Students who only use the Arduino IDE can edit their own copy of the sketch freely; changes meant for this repository belong in `main/`.

---

## 4. Making a release (maintainers)

```bash
python3 tools/sync_arduino.py                       # Arduino copy up to date
idf.py build
idf.py merge-bin -o sticks3-imu-logger.bin          # bootloader + partitions + app, flash at 0x0
cp build/sticks3-imu-logger.bin firmware/
```

1. Change `"version"` in `flasher/manifest.json` and the version line in `flasher/index.html`.
2. Optionally check the Arduino build: `cd arduino/sticks3_imu_logger && arduino-cli compile .`
3. Commit and push. GitHub Pages serves the new installer, image and USB page within a minute or two.
4. Check the **Build** status (green tick on the commit, or the badge in the README): GitHub Actions builds both versions on every push and checks that the Arduino copy matches `main/`. It does not test the hardware.

---

## 5. Troubleshooting

| Problem | Fix |
| --- | --- |
| `idf.py: command not found` | The terminal has no ESP-IDF environment: run the command from section 1.2 |
| `esp_psram.h: No such file or directory` | The `esp_psram` line in the top-level `CMakeLists.txt` is missing (section 1.8) |
| First build fails downloading components | It needs internet (and no blocking proxy) once; then `idf.py build` again |
| Settings changes in `sdkconfig.defaults` have no effect | Delete `sdkconfig`, then `idf.py set-target esp32s3` (or `idf.py fullclean`) |
| `Could not open PORT, the port is busy` | Close other monitors, the Arduino IDE, browser pages using the StickS3, then retry |
| Flashing never connects | Download mode (section 1.6); try another cable or USB port |
| Screen blank after flashing | Still in download mode: double-press power to turn off, press once to turn on |
| Monitor shows nothing | Press the power button once to restart; check the port |
