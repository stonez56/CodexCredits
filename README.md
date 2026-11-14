# CodexCredits — CrowPanel ESP32-S3 5.79" 電子紙 AI 額度與氣象桌鐘

[![Release](https://img.shields.io/github/v/release/stonez56/CodexCredits?include_prereleases)](https://github.com/stonez56/CodexCredits/releases)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32--S3-orange.svg)](https://platformio.org/)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

專為 **Elecrow CrowPanel ESP32-S3 5.79 吋電子紙（792×272，雙 SSD1683 控制晶片）** 打造的桌面智能儀表板。具備 **OpenAI Codex Plus** 與 **雙 Google Gemini Pro** 訂閱額度追蹤、精準 **NTP UTC+8 時鐘**、**OpenWeatherMap 即時天候與 5 日預報**，並提供免接線的 **Wi-Fi Manager 配網** 與 **ArduinoOTA 無線升級**。

---

## 核心功能特色 (Core Features)

1. **AI 訂閱額度一覽與倒數**
   - **Codex Plus**：5 小時用量限制（剩餘百分比、圓環進度、`HH:MM` 倒數）、每週用量限制（剩餘百分比、圓環進度、天/時/分倒數）、精確已購點數（Exact Credits Purchased）、可用重設次數及各次排程重設之 UTC+8 時間。
   - **雙 Google Gemini Pro**：帳號獨立顯示每週用量（Weekly Limit Remaining）與 5 小時用量（Five Hour Limit Remaining）之百分比、圓環圖與完整重設倒數語句。
   - **離線平滑倒數**：韌體於同步時取得 `expires_at_epoch`，平日每分鐘依 NTP 本地推算倒數，極低網路負荷。
2. **多分頁與 UI/UX Pro Max 視覺優化**
   - **Page 1（總覽 Overview）**：頂部狀態列＋三欄資訊卡，一眼綜覽 Codex 與兩組 Gemini 核心狀態。
   - **Page 2（Codex Plus 詳細）**：大環形圖、精確倒數、已購金額與多筆用量上限重設歷史排程。
   - **Page 3（雙 Gemini Pro 詳細）**：左右對稱分割，清楚呈現兩組帳號完整重設語句與進度。
   - **Page 4（氣象 Weather）**：即時溫度、體感、濕度、風速、天候向量圖示與未來 5 天日曆分組預報（高/低溫、降雨機率 PoP）。
3. **高品質雙語點陣字庫（Switchable Language）**
   - 整合 `FreeSansBold` 英文粗體與專屬產生的 **20×20 微軟正黑體粗體**（`tc_font.h`），徹底解決電子紙缺字、疊字與對比度不足問題。
   - 支援 **繁體中文 (ZH) / 英文 (EN)** 一鍵即時切換。
4. **靈活的自動輪播與護屏排程**
   - **日間自動輪播**：日間預設每 30 秒自動換頁（Page 1 → 2 → 3 → 4 → 5）。
   - **台股與美股即時報價（Page 5）**：單頁雙欄顯示 10 檔標的（`0050`、`006208`、`0056`、`00878`、`00919`、`2330`、`00937B`、`00719B`、加權指數、標普 500）；盤中每 **5 分鐘**更新，非交易時段自動沿用快取、不浪費網路流量。
   - **手動優先防跳頁（Hold）**：手動撥輪換頁自動暫停輪播 3 分鐘，確保閱讀不被打斷。
   - **夜間休眠護屏（Sleep）**：`00:00 ~ 07:00` 自動鎖定 Page 1，頂部標記 `SLEEP`，降頻更新以延長電子紙壽命並避免干擾睡眠。
5. **免重燒 Wi-Fi 配置與無線 OTA 更新**
   - **Wi-Fi Manager**：連線失敗或更換環境時，自動開啟 `CrowPanel-Setup` 熱點，透過手機/電腦網頁配置 Wi-Fi、城市代碼與 API 金鑰。
   - **ArduinoOTA**：支援同區網透過 mDNS（`crowpanel-epaper.local`）無線上傳韌體。

---

## 硬體規格與腳位對照

- **開發板**：Elecrow CrowPanel ESP32-S3 5.79" E-Paper HMI
- **微控制器**：ESP32-S3-WROOM-1-N8R8（8MB Flash, 8MB OPI PSRAM）
- **顯示面板**：792 × 272 純黑白雙色電子紙（雙 SSD1683 串聯）

| 訊號名稱 | ESP32 GPIO | 說明 |
| :--- | :---: | :--- |
| `EPD_SCK` | GPIO 12 | SPI 時脈 |
| `EPD_MOSI` | GPIO 11 | SPI 資料 |
| `EPD_CS` | GPIO 45 | 片選 |
| `EPD_DC` | GPIO 46 | 資料/指令切換 |
| `EPD_RST` | GPIO 47 | 螢幕重設 |
| `EPD_BUSY` | GPIO 48 | 忙碌狀態腳位 |
| `EPD_PWR` | GPIO 7 | 面板電源控制（操作前拉 HIGH） |
| **頂部按鍵** (`MENU`) | GPIO 2 | 資料強制同步 / 城市循環切換 |
| **底部按鍵** (`EXIT`) | GPIO 1 | 一鍵返回 Page 1 並恢復輪播 |
| **撥輪滾動** (`UP` / `DOWN`) | GPIO 6 / 4 | 切換上/下一頁（暫停輪播 3 分鐘） |
| **撥輪按壓** (`CLICK`) | GPIO 5 | **短按**：中英切換；**長按 1 秒**：輪播開/關 (AUTO/LOCK) |

---

## 快速安裝與燒錄教學 (Getting Started)

### 1. 必備環境
- 安裝 [VS Code](https://code.visualstudio.com/) 與 [PlatformIO IDE 擴充套件](https://platformio.org/)
- 具備資料傳輸功能的 USB-C 連接線

### 2. 複製專案
```bash
git clone https://github.com/stonez56/CodexCredits.git
cd CodexCredits
```

### 3. 設定機密組態 (Secrets Setup)
複製範本檔案建立本機私有設定：
```bash
cp firmware/include/secrets.example.h firmware/include/secrets.h
```
編輯 `firmware/include/secrets.h` 填入您的資訊（此檔案已被 `.gitignore` 排除，絕不外流）：
```cpp
// Wi-Fi 預設連線（若留空可改用手機 Wi-Fi Manager 配網）
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// OpenWeatherMap 2.5 API Key (可於 openweathermap.org 免費取得)
#define OWM_API_KEY   "YOUR_OPENWEATHER_API_KEY"

// AI 額度端點（若尚未建置中繼站，請保持預設或留空，韌體會自動使用內建 Mock 數據）
#define QUOTA_API_URL   ""
#define QUOTA_API_TOKEN ""
```

### 4. 編譯與燒錄 (Firmware Upload)
使用 USB-C 線將 CrowPanel 連接至電腦：
```bash
cd firmware
# 編譯並上傳韌體
pio run -t upload

# 開啟序列埠除錯監聽 (Baud Rate: 115200)
pio device monitor -b 115200
```
*(若上傳失敗，請確認側邊開關為 ON，或按住 `BOOT` 鍵再短按一下 `RESET` 進入燒錄模式)*

---

## 網路配置與使用說明 (How to Use)

### Wi-Fi 配網 (Captive Portal)
若尚未在 `secrets.h` 寫入 Wi-Fi，或將裝置帶至新環境（例如辦公室、咖啡廳）：
1. 裝置開機連線超時後，螢幕會顯示 **Wi-Fi Setup Mode**。
2. 用手機或電腦 Wi-Fi 搜尋並連線至熱點：**`CrowPanel-Setup`**。
3. 瀏覽器自動彈出（或手動開啟 `http://192.168.4.1`）。
4. 選擇 Wi-Fi 基地台、輸入密碼，並可在此直接設定/修改：
   - OpenWeather API Key
   - 城市代碼（`0`=新竹, `1`=台北, `2`=台中, `3`=高雄）
   - Quota API URL 與 Bearer Token
5. 按下 **Save**，裝置自動重啟並完成同步！

### 實體按鍵操作指南
- **撥輪短按 (`CLICK`)**：在 **繁體中文 (`ZH`)** 與 **英文 (`EN`)** 之間即時切換。
- **撥輪長按 (`CLICK` 1 秒)**：在 **自動輪播 (`AUTO`)** 與 **鎖定當前頁 (`LOCK`)** 之間切換。
- **撥輪滾動 (`UP` / `DOWN`)**：手動切換至上一頁／下一頁，並自動進入 **`HOLD`** 模式（暫停輪播 3 分鐘）。
- **頂部按鍵 (`MENU`)**：立即強制連網同步所有 AI 額度與氣象；在 **Page 4 天氣頁** 按下可循環切換預設城市（新竹 → 台北 → 台中 → 高雄）。
- **底部按鍵 (`EXIT`)**：一鍵跳回 **Page 1 總覽頁** 並立刻恢復 30 秒自動輪播。

### 無線韌體更新 (HTTP Push OTA + 版本自動檢查)
只要 ESP32 已完成過一次 USB 燒錄並連上區網 Wi-Fi（與電腦在**同一個區網**），日後更新程式碼完全不需插拔 USB 線，也不受 Windows 防火牆反向連線阻擋：

1. **透過 mDNS 主機名稱進行無線更新（預設）**：
   ```bash
   cd firmware
   pio run -e crowpanel_579_ota -t upload
   ```
   - 腳本（`firmware/scripts/ota_upload.py`）會先查詢 `http://crowpanel-epaper.local/version`；若裝置已是相同版本，會**自動跳過上傳**。
   - 若為新版本，裝置螢幕會顯示 **`OTA Updating...`**，傳送完成後自動重啟，並在右上角狀態列顯示最新韌體版本號（例如 `v0.6.2`）。
2. **若 Windows 無法解析 `crowpanel-epaper.local`**，可改以區網 IP 指定上傳：
   ```bash
   pio run -e crowpanel_579_ota -t upload --upload-port 192.168.x.x
   ```
3. **注意事項**：
   - 更新期間請維持穩定供電（若使用行動電源，請確認不會因低電流自動斷電），完成重啟前勿斷電。

---

## Stonez56.com 中繼站部署教學 (PHP 8.3 Server + Pusher)

本專案提供輕量級 PHP 8.3 中繼 API（位於 `server/`）與本機 Python 推送腳本（位於 `pusher/`），讓 ESP32 透過 HTTPS `Bearer Token` 安全讀取您帳號的即時額度，同時將機密留在伺服器與本機，不進入 Git 版本庫。

### 1. 產生高強度 Bearer Token
在終端機執行以下指令產生 256-bit 隨機金鑰（**ESP32、伺服器、Pusher 三端必須一致**）：
```bash
python -c "import secrets; print(secrets.token_urlsafe(32))"
```

### 2. 部署 PHP 8.3 伺服器端 (`server/`)
為落實機密與公開程式分離（縱深防禦），建議將 `config.php` 與快取 `.json` 放在上層目錄，API 執行檔放在 `api/` 子目錄：
```text
stonez56.com/codexcredit/
├── .htaccess          (封鎖外部直讀 config.php 與 *.json)
├── config.php         (機密 Token 與 data_file 路徑，不上傳 Git)
├── quota_data.json    (由 PHP 自動建立寫入)
├── stocks_cache.json  (由 stocks.php 自動建立快取)
└── api/
    ├── .htaccess      (保留 Authorization Header)
    ├── quota.php      (AI 額度 API，會自動優先讀取 ../config.php)
    └── stocks.php     (台/美股 API，會自動優先讀取 ../config.php)
```
1. 在 `stonez56.com` 建立 `codexcredit/` 與 `codexcredit/api/` 兩個目錄。
2. 將 `server/quota.php`、`server/stocks.php` 與 `server/.htaccess` 上傳至 `codexcredit/api/`；並在 `codexcredit/` 放置 `.htaccess` 封鎖 `config.php` 與 `*.json`（若 `codexcredit/` 位於網頁根目錄外則更佳）。
3. 複製 `server/config.example.php` 為 **`codexcredit/config.php`**（本機測試檔已由 `.gitignore` 排除，切勿提交），並填入剛剛產生的 Token：
   ```php
   <?php
   declare(strict_types=1);
   return [
       'api_token' => '貼上剛剛產生的_32_字元以上_TOKEN',
       'data_file' => __DIR__ . '/quota_data.json',
   ];
   ```
4. 確認 `codexcredit/` 目錄對 PHP 執行身分（如 `www-data`）具有寫入 `quota_data.json` 的權限（建議資料夾 `755`、`config.php` `640`）。
5. 驗證 Fail-Closed 安全機制：
   ```bash
   # 未帶 Token 應回傳 401 Unauthorized
   curl -i https://stonez56.com/codexcredit/api/quota.php
   # 上層 config.php 應回傳 403 Forbidden
   curl -i https://stonez56.com/codexcredit/config.php
   ```

### 3. 設定本機 Pusher (`pusher/`) 與 ESP32
1. 複製 `pusher/.env.example` 為 **`pusher/.env`**（已被 `.gitignore` 排除），設定 `QUOTA_PUSH_URL=https://stonez56.com/codexcredit/api/quota.php` 與同一組 `QUOTA_API_TOKEN`。
2. 執行推送測試（預設讀取 `mock_server/mock_quota.json` 或您的真實額度資料）：
   ```bash
   cd pusher
   pip install requests python-dotenv
   python push_quota.py
   ```
3. 於 ESP32 Captive Portal（或 `firmware/include/secrets.h`）設定：
   - **Quota URL**：`https://stonez56.com/codexcredit/api/quota.php`
   - **Quota Token**：同一組 `Bearer Token`
   - 關閉 Mock 模式（或等待正式資料寫入），即可在裝置上看見動態模型池（`Gemini Models`、`Claude & GPT`、`Gemini 4 Argon` 等）與 Codex Plus 即時倒數。

---

## 本地測試 Mock 伺服器 (Optional)

專案內附 `mock_server/`，供無正式後端 API 時本地測試即時倒數與動態額度：
```bash
cd mock_server
python serve.py --port 8080
```
啟動後會自動以「目前時間」動態推算未來的到期時間，供 ESP32 進行完整倒數渲染驗證。

---

## 授權條款 (License)

本專案採用 [MIT License](LICENSE) 授權釋出。

