#include "WiFiMaintainer.h"

int WiFiMaintainer::maintainWiFiConnection(long currentTime) {
  int wiFiStatus = WiFi.status();
  if( wiFiStatus != lastReportedWiFiStatus ) {
    reportWiFiStatus(wiFiStatus);
    lastReportedWiFiStatus = wiFiStatus;
    if( wiFiStatus == WL_CONNECTED ) {
      failureCount = 0;
      if( lastAttemptedConfigIndex >= 0 ) {
	lastSuccessfulConfigIndex = lastAttemptedConfigIndex;
      }
    }
  }
  
  // Our first attempt is to let the thing connect
  // based on whatever was saved in EEPROM last time!
  if( wiFiStatus == WL_DISCONNECTED && lastWiFiConnectAttempt < 0 ) {
    lastWiFiConnectAttempt = currentTime;
    Serial.print("# Waiting to see if WiFi automatically connects with previous settings to ");
    Serial.print(WiFi.SSID());
    Serial.println("...");
    return wiFiStatus;
  }
  
  bool attemptConnect = false;
  if( wiFiStatus != WL_CONNECTED ) {
    // Give like 10 seconds for connection to work itself out
    // before trying a different config.
    if( currentTime - lastWiFiConnectAttempt >= 10000 ) {
      // If it's been 2 seconds since we attempted to connect,
      // and we're still not connected, move on to the next config.
      ++failureCount;
      Serial.print("# WiFi connection attempt ");
      Serial.print(failureCount);
      Serial.println("...");
      attemptConnect = true;
    }
  }
  if( attemptConnect ) {
    lastAttemptedConfigIndex = this->pickConfig();
    if( lastAttemptedConfigIndex >= 0 ) {
      const WiFiConfig &config = this->configs[lastAttemptedConfigIndex];
      Serial.print("# Attempting to connect to ");
      Serial.print(config.ssid);
      Serial.println("...");
      WiFi.begin(config.ssid, config.password);
    } else {
      Serial.println("# No WiFi networks configured!");
    }
    lastWiFiConnectAttempt = currentTime;
  }
  return wiFiStatus;
}
