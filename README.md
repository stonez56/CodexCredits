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
   - **日間自動輪播**：日間預設每 30 秒自動換頁（Page 1 → 2 → 3 → 4）。
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

### 無線韌體更新 (ArduinoOTA)
只要裝置已連上區網 Wi-Fi，日後更新程式碼完全不需連接 USB 線：
```bash
pio run -t upload --upload-port crowpanel-epaper.local
```

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
