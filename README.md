# 使用 Gattlib 的 C 語言 BLE Central

這是 BLE 作業的 Option 1 範例：在 Linux 主機上使用 C 與 Gattlib，作為 BLE Central 連線到手機上的 GATT Server。

操作流程和 Python basic 範例相近：

1. 掃描附近的 BLE 裝置。
2. 從清單中選擇手機並連線。
3. 顯示手機提供的 GATT services 與 characteristics。
4. 對指定 characteristic 啟用 indication。Gattlib 會透過 BlueZ 訂閱 indication，對應到 CCCD 的 `0x0002`。
5. 接收手機送出的 indication，並在 Pi 終端機顯示資料。

本程式使用 [Gattlib](https://github.com/labapart/gattlib)，它是建立在 BlueZ 上的 C 語言 BLE 函式庫。**請在 Raspberry Pi 或其他 Linux 主機上建置與執行；macOS 不提供這個程式所需的 BlueZ 環境。**

## 環境需求與測試狀態

- Raspberry Pi 3（或其他支援 BLE 的 Linux 主機），並使用 Raspberry Pi OS 或其他 Linux 發行版。
- BlueZ 與可用的 BLE adapter。
- 手機端 BLE GATT Server app；以下以 iPhone 的 LightBlue 為例，也可使用 Android BLE Tool。
- Pi 需要網路連線，供腳本安裝建置依賴及下載 Gattlib 原始碼。

建置腳本會下載執行當下 Gattlib 官方 GitHub `master` 分支的最新版本，並安裝到本專案的 `.local/`，不會安裝到系統目錄。因為 Gattlib 與 BlueZ 版本可能影響建置或連線行為，繳交報告或 demo 時，建議記錄：

```bash
uname -a
bluetoothctl --version
git -C gattlib-src rev-parse --short HEAD
```

**驗證範圍：**本專案已建立並推送到 GitHub；目前尚未在 Raspberry Pi 上實際建置或連接手機測試。請依下方步驟在目標 Pi 上建置，並以手機端 server log 和 Pi 終端輸出確認結果。不要將尚未完成的實機測試描述為已驗證。

## 1. 設定手機

### iPhone：LightBlue

1. 開啟 LightBlue，建立或選擇 `Blank` 虛擬 Peripheral。
2. 建立或選擇 service UUID `1111`。
3. 建立或選擇 characteristic UUID `b3fcaed7-8598-419f-89f9-ebf46229fc7d`，並開啟 **Read** 和 **Indicate**。
4. 開始廣播。Demo 期間保持 LightBlue 開啟，並讓手機維持解鎖狀態。

### Android：BLE Tool

啟動 GATT Server，並選擇具有 **Indicate** 屬性的 characteristic。BLE Tool 的預設 `FFF1` characteristic 可用；若 UUID 不同，執行程式時要傳入正確 UUID。

## 2. 在 Raspberry Pi 上建置

將整個 `ESlab_hw3_option1` 資料夾複製到 Pi，接著在 Pi 的終端機執行：

```bash
cd ~/ESlab_hw3_option1
bash build_on_raspberry_pi.sh
```

腳本會：

1. 安裝 Linux 建置工具和 Gattlib 所需的開發套件（會使用 `sudo apt`）。
2. 將 Gattlib 官方原始碼下載到 `gattlib-src/`。
3. 在本專案的 `.local/` 建置並安裝 Gattlib，同時建置 Gattlib 隨附的範例。
4. 建置本專案的 C BLE Central，輸出至 `build/ble_indication_central`。

建置產生的 `.local/`、`build/` 和 `gattlib-src/` 都是本機檔案，不需要提交到 GitHub。

## 3. 執行 demo

使用 iPhone LightBlue 時，執行：

```bash
cd ~/ESlab_hw3_option1
export LD_LIBRARY_PATH="$PWD/.local/lib:$PWD/.local/lib/aarch64-linux-gnu:$PWD/.local/lib/arm-linux-gnueabihf${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./build/ble_indication_central --char b3fcaed7-8598-419f-89f9-ebf46229fc7d
```

使用 BLE Tool 預設的 `FFF1` characteristic 時：

```bash
./build/ble_indication_central --char 0000fff1-0000-1000-8000-00805f9b34fb
```

不指定 `--char` 時，程式預設使用 `FFF1`：

```bash
./build/ble_indication_central
```

程式會掃描 10 秒並列出找到的 BLE 裝置。輸入手機旁邊的編號後，程式會連線、列出 GATT table、啟用 indication，並在接下來 30 秒等待資料。此時請在 LightBlue 選 **Indicate** 傳送資料；只送 Notification 不會觸發本範例的 indication callback。

## 如何確認成功

- Pi 顯示已連線並列出 services / characteristics。
- Pi 顯示 `Indications enabled (CCCD 0x0002).`
- 手機端 GATT Server log 顯示 indication 已啟用或 CCCD 被寫入 `02 00`。
- 從手機送出 indication 後，Pi 顯示 `<<< Indication` 和收到的資料 bytes。

CCCD 是 16-bit little-endian 值；`02 00` 代表啟用 indication。Gattlib 的公開 API 有 `gattlib_indication_start()` 和 indication callback，本範例透過這個 API 啟用，不自行猜 CCCD descriptor handle。此 C 範例不讀回 CCCD；請以手機端 server log 作為設定成功的佐證。Gattlib 與 CCCD 的行為可參考[官方專案](https://github.com/labapart/gattlib)及[官方 API header](https://github.com/labapart/gattlib/blob/master/include/gattlib.h)。

## 試跑 Gattlib 隨附範例

建置腳本也會建置 Gattlib 隨附的範例。要查看實際產生的執行檔：

```bash
find gattlib-src/build/examples -type f -executable
```

例如掃描附近裝置：

```bash
sudo gattlib-src/build/examples/ble_scan/ble_scan
```

不同 Gattlib 版本的範例路徑或參數可能不同；可不帶參數執行該範例查看用法。本專案的 C 程式本身也會掃描裝置、探索 GATT，並訂閱 indication。

## 常見問題

- **掃描不到手機：**確認手機正在廣播、Pi 的 Bluetooth 已啟用，並縮短手機與 Pi 的距離。可用 `bluetoothctl show` 檢查 adapter 狀態。
- **掃描得到但連不上：**手機的 BLE 位址可能變動，重新執行程式掃描；並確認沒有另一個 Central 正連著手機。
- **找不到 characteristic：**確認 `--char` UUID 和手機 GATT Server 顯示的 UUID 完全一致，必要時使用完整 UUID。
- **沒有收到資料：**確認 characteristic 有開啟 **Indicate**，並在程式等待的 30 秒內從手機送出 indication，而不是 notification。
- **找不到 BlueZ/Gattlib 或無法編譯：**確認是在 Linux Pi 上執行腳本，而不是 macOS；再檢查 Raspberry Pi OS、BlueZ 版本和錯誤訊息最後幾行。
- **執行時找不到 `libgattlib`：**確認執行前已設定上方的 `LD_LIBRARY_PATH`，且建置腳本成功完成。

## 檔案

- `src/ble_indication_central.c`：C 語言 BLE Central。
- `CMakeLists.txt`：本專案的 CMake 建置設定。
- `build_on_raspberry_pi.sh`：在 Pi 安裝建置依賴、建置安裝 Gattlib，並建置本範例的腳本。
