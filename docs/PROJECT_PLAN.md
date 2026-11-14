# CrowPanel ESP32-S3 5.79" 電子紙 AI 訂閱額度與氣象桌鐘 專案規劃書

> **文件版本**：`v1.0.0`  
> **目標硬體**：Elecrow CrowPanel ESP32-S3 5.79" E-Paper HMI（`792 × 272` 黑白雙色，雙 SSD1683 驅動）  
> **開發框架**：PlatformIO（VS Code）+ Arduino-ESP32 + LVGL / GFX 繪圖層  
> **供電與模式**：USB-C 長時間插電桌鐘模式（局刷更新時間倒數＋定時全刷除殘影）

---

## 1. 專案目標與需求總覽

本專案旨在將 **CrowPanel ESP32-S3 5.79" 電子紙**打造為桌面專用「AI 算力額度與天氣資訊儀表板」，透過實體撥輪與按鍵切換「總覽主頁」與「各項詳細分頁」，並於前期使用 **Mock JSON** 完成所有 UI 繪製、倒數邏輯與刷新機制，確保未來可直接無縫串接區網／雲端中繼伺服器。

### 功能需求清單
| 模組 | 顯示項目與規格 | 備註 / 呈現重點 |
| :--- | :--- | :--- |
| **Codex Plus** | 1. **5hr Limit**：剩餘倒數 `HH:MM`、百分比、環形圖<br>2. **Weekly Limit**：剩餘 `Xd HH:MM`（或天/時/分）、百分比、環形圖<br>3. **Exact Credits Purchased**：精確已購買點數／金額（若無則顯示 `0`）<br>4. **用量上限重設**：可用重設次數（Available Resets）＋ 各次重設之 UTC+8 日期時間 | 參考附圖環形進度風格，針對純黑白螢幕優化粗環與高對比文字 |
| **Google Gemini Pro × 2** | 雙帳號獨立顯示（帳號 A / 帳號 B）：<br>1. **Weekly Limit Remaining**：百分比、環形圖、完整重設倒數（例：`6 days, 22 hours`）<br>2. **Five Hour Limit Remaining**：百分比、環形圖、完整重設倒數（例：`3 hours, 43 minutes`） | 總覽頁精簡呈現雙環＋簡短倒數；詳細頁顯示完整語句與額外細節 |
| **UTC+8 時鐘** | 透過 NTP 同步台灣標準時間（UTC+8）之日期、星期、`HH:MM` | 採分鐘級局部刷新，不顯示秒數以延長面板壽命 |
| **UTC+8 當地天氣** | OpenWeatherMap 2.5 免費版：<br>1. 即時天氣（溫度、體感、濕度、天氣狀態與圖示）<br>2. **5 日預報**（依 UTC+8 日期彙整高/低溫、代表圖示、降雨機率） | 預設 **Hsinchu, Taiwan（新竹）**，支援城市切換；主頁顯示今日摘要，分頁 3 顯示 5 日預報 |
| **多語系介面** | 支援 **繁體中文 / English 即時切換** | 透過按鍵選單或設定切換，Flash 內建精簡雙語字型 |

---

## 2. 硬體規格與腳位確認（CrowPanel 5.79"）

依據 Elecrow 官方 Wiki 與開源腳位定義，開發前於 Phase 1 須先以最小測試程式驗證以下硬體特性：

- **MCU**：ESP32-S3-WROOM-1-N8R8（Dual-core LX7 240 MHz，**8 MB Flash / 8 MB OPI PSRAM**）
- **螢幕解析度**：**792 × 272**（橫向寬螢幕，純黑白 1-bit，無灰階）
- **顯示驅動**：**雙 SSD1683 級聯控制**（螢幕左右半區分別由兩顆 IC 控制，務必選用支援雙 SSD1683 拼接與局部刷新的驅動，例如 Elecrow 官方範例驅動或已驗證的 `GxEPD2`／客製 SSD1683 驅動）

### 核心腳位對照表
| 功能分類 | 訊號名稱 | ESP32-S3 GPIO | 備註 |
| :--- | :--- | :---: | :--- |
| **電子紙 SPI** | `SCK` (CLK) | `GPIO 12` | 硬體 SPI 時脈 |
| | `MOSI` (SDI) | `GPIO 11` | 硬體 SPI 資料 |
| | `CS` | `GPIO 45` | 片選 |
| | `DC` | `GPIO 46` | 資料/指令切換 |
| | `RST` | `GPIO 47` | 硬體重置 |
| | `BUSY` | `GPIO 48` | 忙碌狀態腳位（刷新時不可阻塞看門狗） |
| | `PWR` | `GPIO 7` | 面板供電開關（使用前須拉 HIGH） |
| **實體操作鍵** | `EXIT` 按鍵 | `GPIO 1` | 短按：快速返回 Page 0 主頁 |
| | `MENU` 按鍵 | `GPIO 2` | 短按：手動全刷除殘影；長按：系統狀態彈窗 |
| | 撥輪 `DOWN` | `GPIO 4` | 切換下一頁（Page + 1） |
| | 撥輪 `CLICK` | `GPIO 5` | 立即強制向 API / Mock 端點同步資料 |
| | 撥輪 `UP` | `GPIO 6` | 切換上一頁（Page - 1） |

> [!IMPORTANT]
> **實作前確認事項**：雙 SSD1683 在局部刷新（Partial Refresh）時，若刷新區域跨過左右兩顆 IC 的交界線（`x = 396` 附近），部分非官方驅動容易出現邊界撕裂或殘影。因此規劃版面時，**儘量讓局部刷新區塊避開正中央交界，或將局刷邏輯包成抽象層以便實機驗證後微調**。

---

## 3. 系統架構與資料流設計

### 為何需要中繼站（Relay）？（僅限 AI 訂閱額度，天氣與時間不需經過中繼）
- **NTP 時間與 OpenWeatherMap 天氣**：皆為公開標準協定/REST API，**ESP32-S3 直接連線抓取**，不需中繼站。
- **Codex Plus 與兩組 Google Gemini Pro 額度**：目前 OpenAI 與 Google **皆未針對個人訂閱方案（Plus / Pro）提供官方公開 REST API**。這些額度頁面通常位於 OAuth2 登入、瀏覽器 Session Cookie、Cloudflare 防護或本機 CLI/IDE 憑證之後，ESP32-S3 受限於記憶體與無完整瀏覽器引擎，無法直接安全處理雙 Google 帳號登入與反爬蟲驗證。
- **推薦銜接模式（結合您的 `stonez56.com`）**：
  - **模式 A（Local Python Pusher → `stonez56.com` PHP/Python API → ESP32）**：由本機電腦（已有登入 Session/CLI 憑證的環境）執行 Python 腳本定期取得 3 組額度，將**去識別化後的精簡 JSON** 透過 `Bearer Token` 推送至 `stonez56.com/api/quota`；ESP32 只需向 `stonez56.com` 拉取 JSON，**不需在公網伺服器存放任何 OpenAI/Google 帳號密碼**，且桌鐘在任何 Wi-Fi 下都能獨立運作。
  - **模式 B（純區網 Python Relay）**：本機或 NAS 直接開區網 HTTP 端點給 ESP32 拉取（僅限同區網且主機開機時）。
- **現階段開發策略**：先採用 **Mock JSON** 開發完整韌體與 UI，待 Phase 1～4 完成後，再無痛接上 `stonez56.com` API。

```text
┌──────────────────────────────┐        ┌──────────────────────────────────┐
│  外部資料源 (Mock → 真實中繼) │        │  OpenWeatherMap 2.5 & NTP        │
│  1. 本地 HTTP Mock Server     │        │  - /data/2.5/weather (即時)       │
│     (或 ESP32 LittleFS 備援)  │        │  - /data/2.5/forecast (3hr×40筆)  │
│  2. 未來替換為 PC/NAS Relay   │        │  - pool.ntp.org (UTC+8)          │
└──────────────┬───────────────┘        └────────────────┬─────────────────┘
               │ HTTPS/HTTP JSON                         │ HTTPS / UDP
               ▼                                         ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                     CrowPanel ESP32-S3 韌體核心                          │
│                                                                          │
│  [Network & Sync Manager]                                                │
│   ├─ Config Loader (secrets.h / Fail-Closed 檢查)                        │
│   ├─ Schema Validator (ArduinoJson + 長度/上限邊界防禦)                   │
│   └─ Time-Based Countdown Engine (以 expires_at Epoch 本地即時推算倒數)   │
│                                                                          │
│  [State Store (PSRAM / RAM)]                                             │
│   ├─ CodexPlusState (5hr, weekly, credits, resets[])                     │
│   ├─ GeminiProState[2] (account_label, weekly, 5hr)                      │
│   └─ WeatherState (current + 5-day UTC+8 aggregated forecast[])          │
│                                                                          │
│  [UI & Display Scheduler]                                                │
│   ├─ Input Handler (Debounce + 撥輪切換 Page 0~3 / 手動同步 / 全刷)        │
│   ├─ Canvas Renderer (1-bit 環形圖繪製、中英混排字型、天氣向量圖示)         │
│   └─ Refresh Policy (局刷：1~5 min；全刷：每 30 min 或累計 12 次局刷)      │
└──────────────────────────────────────────────────────────────────────────┘
```

### 關鍵設計亮點：本地 Epoch 倒數推算
若僅依賴伺服器回傳「剩餘幾小時幾分」，每次畫面更新都要打 API。本設計要求 JSON 優先提供 **`expires_at`（Unix Timestamp，秒）**，ESP32 僅在每次同步（例如每 15 分鐘）更新該時間戳記，而在每分鐘局部刷新時，直接用 `expires_at - now_utc_epoch` 動態算出精準的 `HH:MM` 或 `Xd Xh Xm`，達成**離線平滑倒數、極低 API 請求量**。

---

## 4. 畫面排版與分頁規劃（`792 × 272` 黑白電子紙）

針對 `792 × 272` 寬矮比例與純黑白特性，採用 **1 頁總覽 + 3 頁詳細** 的 4 分頁架構：

### Page 0：總覽主頁（Overview — 常駐預設頁）
頂部狀態列（高度 `44px`）與下方三欄卡片（高度 `228px`，每欄寬 `264px`）：

```text
+-------------------------------------------------------------------------------+
| 2026-11-14 (六)  13:05  |  Taipei 24°C 多雲  [☁]  |  WiFi OK  Sync 13:00 [P0]  |
+-------------------------+-------------------------+---------------------------+
| CODEX PLUS              | GEMINI PRO #1           | GEMINI PRO #2             |
|                         |                         |                           |
|  [○ 100%]  5hr Limit    |  [○ 100%]  Weekly       |  [○  85%]  Weekly         |
|            03:42 left   |            6d 22h       |            4d 11h         |
|                         |                         |                           |
|  [○  78%]  Weekly       |  [○  99%]  5hr Limit    |  [○  60%]  5hr Limit      |
|            4d 18:10     |            3h 43m       |            2h 15m         |
|                         |                         |                           |
| Credits: $12.50         |                         |                           |
| Resets : 2 avail (→P1)  |                         |                           |
+-------------------------+-------------------------+---------------------------+
```

### Page 1：Codex Plus 詳細與重設紀錄頁
- **左側欄（約 300px）**：大尺寸 5hr Limit 與 Weekly Limit 環形圖、百分比、精確剩餘時間、到期時間點（UTC+8）與 `Exact Credits Purchased` 大字顯示。
- **右側欄（約 492px）**：**「用量上限重設（Quota Resets）」明細區塊**：
  - 頂部標題顯示：`可用重設次數：N 次`
  - 表格式條列每一次重設時間（例如 `#1 2026-11-15 04:30 (UTC+8)`、`#2 2026-11-18 19:00 (UTC+8)`），支援顯示 4～6 筆清晰紀錄。

### Page 2：雙 Google Gemini Pro 詳細頁
- 左右對半分割（各 `396 × 228px`，剛好貼合雙 IC 中線！）：
  - **左側：Gemini Pro #1（可自訂別名）**
  - **右側：Gemini Pro #2（可自訂別名）**
- 各自呈現完整資訊：
  - **Weekly Limit Remaining**：大環形圖 + `100%` + 完整文字：`It will fully refresh in 6 days, 22 hours.`
  - **Five Hour Limit Remaining**：大環形圖 + `99%` + 完整文字：`It will fully refresh in 3 hours, 43 minutes.`

### Page 3：UTC+8 當地天氣與 5 日預報頁
- **左側今日詳情（約 232px）**：城市名稱、大字即時溫度（`24°C`）、體感溫度、濕度 `%`、風速、天氣圖示與描述、今日日出/日落（UTC+8）。
- **右側 5 日預報（約 560px，均分 5 欄每欄 `112px`）**：
  - 每天依序顯示：`MM/DD (週X)` → 黑白高對比天氣圖示（晴/雲/雨/雷/霧）→ `最高° / 最低°` → 降雨機率 `PoP %`。

### 黑白電子紙視覺規範（環形圖與中文字型）
1. **1-bit 環形圖（Ring Gauge）繪製演算法**：
   - 使用三角函數或中點圓弧演算法繪製 **粗圓環**（外半徑 `R=22px`，環寬 `5px`），從正上方 `-90°` 順時針繪製代表剩餘百分比的實心黑弧（`100%` 為完整黑環，`75%` 為 3/4 黑環）。
   - 底軌（未填滿的剩餘軌道）在無灰階的黑白螢幕上，採 **1px 虛線／棋盤格點狀網紋圓環** 呈現，視覺效果極接近附圖的圓環層次感。
   - 百分比數字置於圓環旁或圓環中央（大環置中、小環置旁，確保小字清晰可讀）。
2. **中文字型（繁體中文）**：
   - 使用 **LVGL 字型轉換器**或 **U8g2 / Adafruit_GFX 客製點陣字集**（例如：思源黑體 Noto Sans TC 或 俐方體/文泉驛），僅匯出 UI 所需漢字子集＋常用天氣字詞＋ASCII，控制字型大小約 150KB～400KB，放置於 Flash 節省寶貴 RAM。

---

## 5. JSON 資料契約（Mock & 未來後端標準介面）

為確保安全與未來中繼伺服器無痛對接，先行定義 `v1` JSON Schema，所有欄位須受邊界檢查（陣列上限、字串長度上限）：

```json
{
  "schema_version": "1.0",
  "generated_at_epoch": 1794632640,
  "codex_plus": {
    "status": "ok",
    "five_hour": {
      "remaining_pct": 100,
      "expires_at_epoch": 1794646020
    },
    "weekly": {
      "remaining_pct": 78,
      "expires_at_epoch": 1795043640
    },
    "purchased_credits": {
      "has_credits": true,
      "display_text": "$12.50"
    },
    "resets": {
      "available_count": 2,
      "schedule_epochs": [
        1794717000,
        1795018800
      ]
    }
  },
  "gemini_pro_accounts": [
    {
      "label": "Gemini Pro A",
      "status": "ok",
      "weekly": {
        "remaining_pct": 100,
        "expires_at_epoch": 1795230240
      },
      "five_hour": {
        "remaining_pct": 99,
        "expires_at_epoch": 1794646020
      }
    },
    {
      "label": "Gemini Pro B",
      "status": "ok",
      "weekly": {
        "remaining_pct": 85,
        "expires_at_epoch": 1795017840
      },
      "five_hour": {
        "remaining_pct": 60,
        "expires_at_epoch": 1794640740
      }
    }
  ]
}
```

### 邊界防禦與容錯設計（安全與穩健性）
- **輸入驗證**：使用 `ArduinoJson` 解析時，設定嚴格文件緩衝上限（如 `4096 bytes`）；拒絕超大 Payload（DoS 防護）。
- **欄位防呆**：
  - `remaining_pct` 限制夾擠於 `[0, 100]`。
  - `resets.schedule_epochs` 最多讀取 `10` 筆；`gemini_pro_accounts` 固定上限 `2` 筆。
  - 字串欄位如 `label`、`display_text` 截斷至 `<= 24` 字元以防 UI 溢位。
- **降級顯示（Graceful Degradation）**：若 HTTP 抓取失敗或 JSON 格式錯誤，保留最後一筆有效快取資料並於頂部狀態列顯示 `Sync Err` 與錯誤時間，不清空畫面。

---

## 6. 刷新排程、自動輪播與夜間護屏策略（USB-C 插電模式）

### 6.1 分頁自動輪播與手動控制邏輯（Auto-Carousel + Manual Override）
為了兼顧「不按按鍵就能看到所有分頁」與「電子紙防殘影／閱讀不跳頁」體驗，設計以下狀態機：
1. **日間自動輪播（`07:00 ~ 23:59`）**：
   - 預設每 **30 秒（`AUTO_CYCLE_SEC = 30`，可設定為 30s / 60s / Off）** 自動切換至下一頁（`Page 0 → 1 → 2 → 3 → 0`）。
   - **頂部列鎖定＋下半部局刷**：換頁時維持頂部 `44px` 狀態列穩定，僅針對下半部 `228px` 內容區進行局部刷新（Partial Refresh），減少整屏黑閃干擾。
   - **防殘影補償**：因全頁內容變動較大，每累計 **8～12 次換頁／局刷** 強制觸發一次全刷（Full Refresh），清除累積灰霧殘影。
2. **實體按鍵優先與暫停機制（Manual Hold）**：
   - **撥輪上/下（`GPIO 6 / GPIO 4`）**：立即切換至上一頁／下一頁，並**自動暫停輪播 3 分鐘**（避免使用者正在閱讀詳細數據時突然跳頁）。
   - **撥輪按壓（`GPIO 5`）**：立即強制同步最新資料，或長按切換「輪播啟用／鎖定當前頁」（頂部列顯示 `[AUTO]` 或 `[HOLD]` 圖示）。
   - **`EXIT`（`GPIO 1`）**：一鍵跳回 Page 0 主頁並恢復自動輪播計時。
   - **`MENU`（`GPIO 2`）**：短按手動全刷除殘影；長按切換語系（`中文 / EN`）或循環切換預設城市。

### 6.2 夜間休眠顯示模式（`00:00 ~ 07:00` UTC+8）
- **畫面鎖定**：進入 `00:00`（12:00 AM）後，自動切換回 **Page 0（主頁總覽）** 並**停止自動輪播**。
- **夜間狀態標示**：頂部狀態列顯示 **月亮圖示＋休眠標籤（`☾ SLEEP` / `☾ 夜間休眠`）**，明確提示目前處於護屏節能狀態。
- **低頻更新**：夜間時段改為每 **30 分鐘** 僅執行一次安靜的全刷（同步時間與背景額度），避免整晚閃爍與加速面板老化。
- **臨時喚醒**：夜間若手動按下撥輪或按鍵，可臨時喚醒翻頁檢視 **60 秒**，無操作後自動切回 Page 0 夜間休眠畫面。

### 6.3 排程週期總表

| 事件類型 | 執行週期 | 刷新方式 | 執行內容 |
| :--- | :--- | :--- | :--- |
| **日間自動輪播** | 每 **30 秒**（可調 `30s/60s/Off`） | **下半部局刷 (Partial)** | 日間 `07:00~23:59` 依序輪播 `P0 → P1 → P2 → P3` |
| **本地時鐘與倒數** | 每 **1 分鐘** | **局部刷新 (Partial)** | 依 `expires_at` Epoch 即時重算剩餘時間並更新時鐘 |
| **AI 額度 JSON 同步** | 每 **15 分鐘** | 背景抓取 | 拉取 Mock / `stonez56.com` Relay JSON 更新數據 |
| **OpenWeather 氣象同步** | 每 **30 分鐘** | 併入定時全刷 | 呼叫 `/weather` 與 `/forecast`（約 `96 req/day`） |
| **定時全刷除殘影** | 每 **10 次局刷** 或 **30 分鐘** | **全刷 (Full Refresh)** | 完整黑白反轉清屏，消除電子紙殘影與電荷累積 |
| **夜間休眠模式** | **`00:00 ~ 07:00`** | **每 30 分鐘更新一次** | 鎖定 Page 0、停止輪播，頂部顯示 `☾ SLEEP` 標示 |
| **NTP 校時** | 開機時 + 每 **6 小時** | 無需額外刷屏 | 校正 ESP32 內部 RTC 飄移 |

---

## 7. OpenWeatherMap 5 日預報彙整邏輯（UTC+8）

OpenWeatherMap 免費版 `/data/2.5/forecast` 回傳未來 5 天內每 3 小時一筆共 40 筆 UTC 時間資料（`dt`）。ESP32 端須執行以下聚合演算法：
1. 將每筆預報的 `dt` 加上 `+28800` 秒（`+8 小時`）換算為當地日期 `YYYY-MM-DD`。
2. 略過（或區分）今日剩餘時段，依日期分組未來 **5 個完整日曆天**。
3. 針對每一天計算：
   - `temp_min`：當日所有時段 `main.temp_min` 的最小值
   - `temp_max`：當日所有時段 `main.temp_max` 的最大值
   - `pop_max`：當日最大降雨機率 `pop × 100%`
   - `icon_id`：優先取當日 **當地中午 12:00～15:00** 時段的 `weather[0].icon`（若為第一天不足時段則取該日出現頻率最高或最顯著天候）。
4. 使用 `ArduinoJson` Filter（`DeserializationOption::Filter`）過濾不必要的長欄位，**避免 40 筆 Forecast JSON（約 15~20KB）撐爆內部 Heap**（同時善用 ESP32-S3 8MB PSRAM）。

---

## 8. PlatformIO 專案結構與安全規範

遵守**「無硬編碼機密（No Hardcoded Secrets）」**與**「模組 ≤ 600 行」**原則，規劃專案目錄如下：

```text
CodexCredits/
├── docs/
│   └── PROJECT_PLAN.md           # 本專案規劃書
├── mock_server/                  # 本地開發用 Mock JSON 測試伺服器
│   ├── mock_quota.json           # 符合 v1.0 Schema 的測試資料
│   └── serve.py                  # Python 輕量區網靜態/動態測試端點
├── firmware/                     # PlatformIO ESP32-S3 韌體專案
│   ├── platformio.ini            # esp32-s3-devkitc-1 (N8R8, OPI PSRAM, default_8MB.csv 支援 OTA)
│   ├── .gitignore                # 排除 include/secrets.h、.pio/
│   ├── include/
│   │   ├── config.h              # 非機密常數（刷新間隔、GPIO 腳位、時區 CST-8）
│   │   ├── secrets.example.h     # 機密範本（Wi-Fi、OpenWeather Key、Relay URL）
│   │   └── secrets.h             # [Git 忽略] 初始組態，支援後續由 Wi-Fi Manager 覆寫至 NVS
│   ├── data/                     # LittleFS 內建離線 fallback Mock JSON
│   ├── src/
│   │   ├── main.cpp              # 主排程器與狀態機
│   │   ├── net_manager.cpp/h     # Wi-FiManager (Captive Portal)、NTP 校時、OTA 更新、HTTP Client
│   │   ├── quota_client.cpp/h    # Quota JSON 抓取（未設定 URL 時自動降級為內建 Mock）、Schema 檢核
│   │   ├── weather_client.cpp/h  # OpenWeather 2.5 即時與 5 日 UTC+8 彙整
│   │   ├── input_handler.cpp/h   # 撥輪與 MENU/EXIT 按鍵去抖與事件派發
│   │   ├── display_driver.cpp/h  # CrowPanel 5.79 雙 SSD1683 初始化、局刷/全刷封裝
│   │   └── ui_renderer.cpp/h     # Page 0~3 繪圖、1-bit 環形圖、粗體字型與天候向量圖示
│   └── test/                     # PlatformIO 單元測試（Native/Target 驗證彙整與倒數邏輯）
└── .AGENT.md                     # 專案規範文件（繼承 Global 開發與資安準則）
```

### 安全實作與維運檢查點（Fail-Closed + Wi-Fi Manager + OTA）
- **Wi-Fi Manager（異地免重燒切換網路）**：開機優先讀取 NVS（`Preferences`）或 `secrets.h` 連線；若連線失敗或使用者於設定模式觸發，自動啟動 AP 熱點（`CrowPanel-Setup` / `192.168.4.1`）並在電子紙顯示配網提示，可於手機網頁修改 Wi-Fi SSID/密碼、城市與 `QUOTA_API_URL`／Token。
- **Firmware OTA（無線韌體更新）**：分區表採用 `default_8MB.csv`（雙 APP 分區 `app0` + `app1` + LittleFS），支援 **ArduinoOTA（mDNS `crowpanel-epaper.local`）** 與選配 **Web `/update` 頁面**，後續更新免接 USB 線。
- **Fail-Closed 與 Mock 降級**：若 `secrets.h` 尚未填入 `QUOTA_API_URL`（或留空），系統自動降級使用內建 Mock JSON 驅動畫面，不阻塞其他氣象與時鐘功能；日誌輸出嚴禁印出完整 `API_KEY`、Token 或 Wi-Fi 密碼。

---

## 9. 分階段實作與驗證計畫（Test-Driven Milestones）

### Phase 1：環境建置與 CrowPanel 硬體驗證（Hello E-Paper）
- **工作內容**：建立 PlatformIO 環境（N8R8 / OPI PSRAM 設定）、確認雙 SSD1683 驅動、驗證 `GPIO 7` 供電與全部 5 個輸入鍵（撥輪上/下/按壓 + MENU + EXIT）。
- **成功標準 (Verification)**：
  1. 成功編譯燒錄，Serial 顯示 `PSRAM > 0`（確認 OPI PSRAM 啟用成功）。
  2. 螢幕可正常全刷顯示 `792 × 272` 邊框線，且局部刷新特定區域 5 次無花屏或當機。
  3. 按下撥輪與各按鍵於 Serial 正確印出對應事件。

### Phase 2：資料層開發與本地測試（NTP + OpenWeather + Mock Quota）
- **工作內容**：實作 `net_manager`、`weather_client`、`quota_client` 以及 `mock_server/serve.py`。
- **成功標準 (Verification)**：
  1. 單元測試驗證：OpenWeather 40 筆測試資料能正確依 UTC+8 分組產出 5 天高/低溫與代表圖示。
  2. 單元測試驗證：Quota JSON 邊界檢查能拒絕畸形或超長 Payload，並正確由 `expires_at_epoch` 推算出 `HH:MM` 與 `Xd HH:MM`。
  3. ESP32 實機成功連網取得正確 UTC+8 時間、實際所在地天氣與本地 Mock Server 回傳之額度資料。

### Phase 3：1-bit 繪圖引擎與 Page 0~3 版面實作
- **工作內容**：實作環形進度圖（對齊附圖視覺比例）、中英文字型渲染、黑白氣象向量圖示，以及 Page 0～3 完整畫面。
- **成功標準 (Verification)**：
  1. 撥輪上/下可流暢切換 Page 0～3，且文字無截斷、重疊或超出 `792 × 272` 邊界。
  2. 驗證 Codex Plus（含重設次數與日期時間、購買 Credits）與兩組 Gemini Pro（Weekly/5hr 百分比、環形圖與倒數）與 Mock 資料 100% 一致。

### Phase 4：刷新排程整合與長時間穩定性測試
- **工作內容**：串接 1 分鐘局刷倒數、15 分鐘 Quota 同步、30 分鐘氣象同步與自動全刷除殘影機制，以及異常降級提示。
- **成功標準 (Verification)**：
  1. 連續插電運行 **24 小時以上**無記憶體洩漏（Free Heap 與 PSRAM 穩定）、無 Watchdog 重啟。
  2. 模擬拔掉 Mock Server / 斷網，螢幕保留最後快取並顯示異常標記；恢復連線後於下一週期自動恢復同步。

---

## 10. 已確認規格與下一步

### 已確認之設計決策
1. **城市與時區**：預設 **Hsinchu, Taiwan（`Hsinchu,TW`，UTC+8）**，並於設定／選單中支援**城市切換**（可於預設城市清單中選擇，例如 Hsinchu / Taipei / Taichung / Kaohsiung 等）。
2. **雙語介面（Switchable）**：支援 **繁體中文 / English 即時切換**（透過 `MENU` 選單或實體按鍵組合切換語系並觸發重繪）。
3. **資料串接路線**：
   - **Phase 1～4**：使用本地 Mock Server（`mock_server/serve.py`）+ LittleFS 離線 Mock 驗證完整韌體、雙語 UI 與 4 分頁操作。
   - **Phase 5（已實作部署架構）**：於 `stonez56.com` 部署 PHP 8.3 中繼 API（`server/quota.php`、`server/.htaccess`、`server/config.php`），實施 Fail-Closed `Bearer Token` 鑑權（`hash_equals`）、Atomic 檔案寫入與禁止外部直讀敏感檔；本機端透過 `pusher/push_quota.py` + `pusher/.env`（皆由 `.gitignore` 排除）定期推送 Schema v1.1（含動態 `pools[]` 模型池陣列）至中繼站供 ESP32 安全輪詢。

若以上規劃確認無誤，我們即可開始建立 `.AGENT.md` 與 **Phase 1（PlatformIO 專案骨架＋CrowPanel 5.79" 螢幕與按鍵實機驗證）**！

