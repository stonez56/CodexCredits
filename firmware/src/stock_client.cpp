#include "stock_client.h"
#include "net_manager.h"
#include <ArduinoJson.h>

namespace {
  constexpr size_t MAX_STOCKS_JSON_BYTES = 4096;

  void setStock(StockItem& dst, const char* code, const char* zh, const char* en,
                uint8_t dec, bool valid, float price, float pct) {
    dst.valid = valid;
    strlcpy(dst.code, code, sizeof(dst.code));
    strlcpy(dst.zhName, zh, sizeof(dst.zhName));
    strlcpy(dst.enName, en, sizeof(dst.enName));
    dst.decimals = dec > 2 ? 2 : dec;
    dst.price = price;
    dst.changePct = pct;
  }
}

void StockClient::deriveStocksUrl(const char* quotaUrl, char* outUrl, size_t outSize) {
  if (!outUrl || outSize == 0) return;
  outUrl[0] = '\0';
  if (!quotaUrl || quotaUrl[0] == '\0') return;

  String s(quotaUrl);
  int idx = s.lastIndexOf("quota.php");
  if (idx >= 0) {
    s = s.substring(0, idx) + "stocks.php" + s.substring(idx + 9);
  } else if (s.endsWith("/api/quota")) {
    s = s.substring(0, s.length() - 5) + "stocks";
  } else {
    return;
  }
  strlcpy(outUrl, s.c_str(), outSize);
}

void StockClient::loadMock(time_t nowEpoch, StockState& state) {
  if (nowEpoch < 1700000000) {
    nowEpoch = 1794645000;
  }
  state.isMock = true;
  state.lastSyncOk = true;
  state.lastSyncEpoch = nowEpoch;
  state.itemCount = 10;

  setStock(state.items[0], "0050",   "元大台灣50",       "0050 Yuanta50",  2, true, 195.80f,  1.15f);
  setStock(state.items[1], "006208", "富邦台50",         "006208 Fubon50", 2, true, 112.45f,  1.12f);
  setStock(state.items[2], "0056",   "元大高股息",       "0056 Div",       2, true,  38.42f, -0.34f);
  setStock(state.items[3], "00878",  "國泰永續高股息",   "00878 ESG Div",  2, true,  22.85f,  0.44f);
  setStock(state.items[4], "00919",  "群益台灣精選高息", "00919 HiDiv",    2, true,  24.18f, -0.16f);
  setStock(state.items[5], "2330",   "台積電",           "2330 TSMC",      1, true,1055.0f,   1.93f);
  setStock(state.items[6], "00937B", "群益ESG投等債20+", "00937B Bond",    2, true,  15.88f,  0.25f);
  setStock(state.items[7], "00719B", "元大美債1-3",      "00719B UST1-3",  2, true,  31.65f,  0.06f);
  setStock(state.items[8], "TWII",   "加權指數",         "Tai-Index",      0, true,22850.0f,  0.92f);
  setStock(state.items[9], "GSPC",   "標普500",          "US S&P 500",     0, true, 5985.0f,  0.65f);
}

bool StockClient::parsePayload(const String& jsonStr, StockState& state) {
  if (jsonStr.length() == 0 || jsonStr.length() > MAX_STOCKS_JSON_BYTES) {
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err) {
    return false;
  }

  JsonArray items = doc["items"].as<JsonArray>();
  if (items.isNull() || items.size() == 0) {
    return false;
  }

  StockState next = {};
  next.isMock = false;
  next.lastSyncOk = true;
  next.lastSyncEpoch = doc["updated_epoch"] | 0;
  next.itemCount = 0;

  for (JsonObject it : items) {
    if (next.itemCount >= 10) break;
    const char* code = it["code"] | "";
    const char* zh   = it["zh"]   | code;
    const char* en   = it["en"]   | code;
    uint8_t dec      = static_cast<uint8_t>(it["dec"] | 2);
    bool valid       = it["valid"] | false;
    float price      = it["price"] | 0.0f;
    float pct        = it["pct"]   | 0.0f;
    setStock(next.items[next.itemCount++], code, zh, en, dec, valid, price, pct);
  }

  if (next.itemCount == 0) {
    return false;
  }
  state = next;
  return true;
}

bool StockClient::sync(const char* quotaUrl, const char* bearerToken, time_t nowEpoch, StockState& state) {
  char stocksUrl[180];
  deriveStocksUrl(quotaUrl, stocksUrl, sizeof(stocksUrl));
  if (stocksUrl[0] == '\0') {
    if (state.itemCount == 0) {
      loadMock(nowEpoch, state);
    }
    return false;
  }

  String body;
  if (!NetManager::httpGet(stocksUrl, bearerToken, body, MAX_STOCKS_JSON_BYTES)) {
    state.lastSyncOk = false;
    if (state.itemCount == 0) {
      loadMock(nowEpoch, state);
    }
    return false;
  }

  StockState parsed = {};
  if (!parsePayload(body, parsed)) {
    state.lastSyncOk = false;
    if (state.itemCount == 0) {
      loadMock(nowEpoch, state);
    }
    return false;
  }

  parsed.isMock = false;
  parsed.lastSyncOk = true;
  if (parsed.lastSyncEpoch < 1700000000) {
    parsed.lastSyncEpoch = nowEpoch;
  }
  state = parsed;
  return true;
}

