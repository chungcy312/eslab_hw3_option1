# BLE Central in C with Gattlib

This folder is the C-language optional project for the BLE lab. It follows the same central-side steps as the Python basic project:

1. Scan for nearby BLE devices.
2. Choose the phone from the list and connect.
3. Display the GATT services and characteristics.
4. Enable **indications** for the selected characteristic. This asks the phone's GATT server to set its CCCD to `0x0002`.
5. Show indication data received from the phone.

The program uses [Gattlib](https://github.com/labapart/gattlib), a C library built on BlueZ. Run it on a Linux computer such as a Raspberry Pi; it cannot use the Mac's Bluetooth stack directly.

## What you need

- Raspberry Pi with Bluetooth LE and Raspberry Pi OS / another Linux distribution using BlueZ.
- An Android or iPhone BLE GATT server app. The steps below use **LightBlue** on iPhone.
- Internet access on the Pi for the one-time package and Gattlib download.

## 1. Prepare the phone

### iPhone with LightBlue

1. Open LightBlue and create/open the `Blank` virtual peripheral.
2. Add or select service `1111`.
3. Add/select characteristic `b3fcaed7-8598-419f-89f9-ebf46229fc7d` and enable **Read** and **Indicate**.
4. Start advertising. Keep the app open and the phone unlocked during the demo.

For the Android BLE Tool app, start its GATT server and use a characteristic that has the **Indicate** property. Its default `FFF1` characteristic may also be used; see the alternative command below.

## 2. Build and install Gattlib, then build this program

Copy this whole `ESlab_hw3_option1` folder to the Pi. From a terminal on the Pi:

```bash
cd ~/ESlab_hw3_option1
bash build_on_raspberry_pi.sh
```

The script installs Linux build dependencies, downloads the official Gattlib source into `gattlib-src/`, builds and installs Gattlib locally into `.local/`, builds Gattlib's examples, and builds this C program. It does not replace the Python basic project or install Gattlib system-wide.

## 3. Run the demo

For the iPhone LightBlue characteristic:

```bash
cd ~/ESlab_hw3_option1
export LD_LIBRARY_PATH="$PWD/.local/lib:$PWD/.local/lib/aarch64-linux-gnu:$PWD/.local/lib/arm-linux-gnueabihf${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./build/ble_indication_central --char b3fcaed7-8598-419f-89f9-ebf46229fc7d
```

For BLE Tool's default `FFF1` characteristic:

```bash
./build/ble_indication_central --char 0000fff1-0000-1000-8000-00805f9b34fb
```

The program scans for 10 seconds, lists the devices it found, and asks you to enter the number beside your phone. Then it connects, prints the GATT table, enables indication on the chosen characteristic, and listens for 30 seconds. In LightBlue, send an **indication** (not only a notification) while it is listening. The received bytes appear in the Pi terminal.

If you omit `--char`, the program uses the default FFF1 characteristic:

```bash
./build/ble_indication_central
```

## What proves the demo worked

- The Pi prints `Connected.` and lists services/characteristics.
- The Pi prints `Indications enabled (CCCD 0x0002).`
- The LightBlue / BLE Tool server log shows the client enabling indication or a descriptor write of `02 00`.
- After you send an indication from the phone, the Pi prints a line beginning with `<<< Indication` and its received bytes.

Gattlib's public API provides `gattlib_indication_start()` and an indication callback. This project uses that API rather than guessing the CCCD descriptor handle. The CCCD is a two-byte little-endian value: `02 00` means indications enabled. The C example does not read the CCCD value back; use the phone app's GATT server log as the confirmation, just as the assignment permits.

## Try the upstream Gattlib examples

The build script also builds Gattlib's own examples. To try them, set the runtime library path as above, then inspect the available executable names:

```bash
find gattlib-src/build/examples -type f -executable
```

For example, scan for nearby devices:

```bash
sudo gattlib-src/build/examples/ble_scan/ble_scan
```

The exact example paths or command-line arguments can differ between Gattlib revisions. Run an example without arguments to see its usage. Our C demo itself performs a scan, GATT discovery, and indication subscription.

## Troubleshooting

- **No devices appear:** make sure advertising is on, keep the phone nearby, and retry. On Raspberry Pi OS, check that Bluetooth is enabled with `bluetoothctl show`.
- **The device appears but will not connect:** phone addresses may change. Run a fresh scan; avoid reusing an old address. Also close LightBlue's connection from any other central app.
- **The characteristic is missing:** confirm the UUID exactly matches the phone's GATT server. Use the full UUID shown by the app.
- **No indication arrives:** the chosen characteristic must have **Indicate** enabled; while the C program is listening, send an indication from the phone. A notification is not the same as an indication.
- **Gattlib build fails while configuring examples:** the script updates older example CMake references from PCRE1 to PCRE2 for newer Raspberry Pi OS package sets. If the error is different, save the final lines of the build output and check the Gattlib issue tracker.

## Source

- C central: `src/ble_indication_central.c`
- Build/setup script: `build_on_raspberry_pi.sh`
- Gattlib upstream: https://github.com/labapart/gattlib
- Official API header: https://github.com/labapart/gattlib/blob/master/include/gattlib.h
