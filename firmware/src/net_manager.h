#pragma once

#include <Arduino.h>
#include <functional>
#include "app_types.h"

class NetManager {
public:
  using PortalCallback = std::function<void(const char* apName, const char* apIp)>;

  void loadConfig(RuntimeConfig& cfg);
  void saveConfig(const RuntimeConfig& cfg);

  bool begin(RuntimeConfig& cfg, PortalCallback onPortal);
  bool startConfigPortal(RuntimeConfig& cfg, PortalCallback onPortal);

  bool syncNtp();
  time_t nowEpoch() const;
  bool isWifiConnected() const;
  int8_t wifiRssi() const;

  void beginOTA();
  void handleOTA();

  static bool httpGet(const char* url, const char* bearerToken, String& outPayload, size_t maxBytes);
  static bool isPlaceholder(const char* val);

private:
  bool otaStarted_ = false;
};

