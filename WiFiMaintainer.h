#include <ESP8266WiFi.h>

// Define this in your program
void reportWiFiStatus(int wiFiStatus);

struct WiFiConfig {
  const char *ssid;
  const char *password;
};

class WiFiMaintainer {
  WiFiConfig *configs;
  size_t configCount;
  int lastReportedWiFiStatus = -1;
  int lastAttemptedConfigIndex = -1;
  int lastSuccessfulConfigIndex = -1;
  unsigned int failureCount = 0;
  long lastWiFiConnectAttempt = -10000;
  int pickConfig() {
    if( this->configCount == 0 ) return -1;
    int base = this->lastSuccessfulConfigIndex < 0 ? 0 : this->lastSuccessfulConfigIndex;
    return (base + failureCount) % this->configCount;
  }
public:
  WiFiMaintainer(WiFiConfig *configs, size_t configCount) : configs(configs), configCount(configCount) {}  
  
  int maintainWiFiConnection(long currentTime);
};
