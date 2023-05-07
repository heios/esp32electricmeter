
#include "WiFi.h"
#include "AsyncUDP.h"

#include "time.h"

#include "network_cfg.h"

#include <cstring>
#include <string.h>
#include <array>


void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(NetworkCfg::self_hostname);
  WiFi.begin(NetworkCfg::wifi_ssid, NetworkCfg::wifi_password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.getHostname());
}



uint64_t getLocalTimeMicroseconds() {
  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  return (uint64_t)tv_now.tv_sec * 1000000L + (uint64_t)tv_now.tv_usec;
}


constexpr size_t kBuffSize = std::strlen("2023-05-07T23:46:59.123456") + sizeof('\0');
auto formatTimestamp(uint64_t microseconds) -> std::array<char, kBuffSize> {
  auto buff = std::array<char, kBuffSize>{};
  const time_t t = microseconds / 1000000;

  struct tm timeinfo;
  localtime_r(&t, &timeinfo);

  const auto offset = strftime(buff.data(), buff.size(), "%FT%T.", &timeinfo);
  
  const auto microseconds_part = microseconds % 1000000;
  snprintf(buff.data() + offset, buff.size() - offset, "%06" PRIu64, microseconds_part);
  
  buff.back() = '\0';
  return buff;
}



#pragma pack(push,1)
struct LuminocityRecord {
  uint64_t microseconds_since_epoch = 0;
  int16_t luminocity_value = 0;
};
#pragma pack(pop)

constexpr int kPhotoresPin = 34;
static constexpr int PollingFreq = 32;
static constexpr int LogCapacity = PollingFreq * 3;
auto gLuminocityLog = std::array<LuminocityRecord, LogCapacity>{};
auto gLogIndex = unsigned{ 0 };
uint64_t gLoops = 0;
uint64_t startTime = 0;


AsyncUDP gAsyncUDP;

void setup() {
  Serial.begin(115200);
  initWiFi();
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());

  {
    const long gmtOffset_sec = 0;
    const int daylightOffset_sec = 3600;
    const char* ntpServer = "pool.ntp.org";
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  }

  const uint16_t UDPListenPort = 4567;
  while (!gAsyncUDP.listen(UDPListenPort)) {
    Serial.printf("Error of listening UDP on port %d\n", (int)UDPListenPort);
    delay(500);
  }

  gAsyncUDP.onPacket([](AsyncUDPPacket packet) {
  #define SERIAL_VERBOSE 1
  #ifdef SERIAL_VERBOSE
    Serial.print("UDP Packet Type: ");
    Serial.print(packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast"
                                                                           : "Unicast");
    Serial.print(", From: ");
    Serial.print(packet.remoteIP());
    Serial.print(":");
    Serial.print(packet.remotePort());
    Serial.print(", To: ");
    Serial.print(packet.localIP());
    Serial.print(":");
    Serial.print(packet.localPort());
    Serial.print(", Length: ");
    Serial.print(packet.length());
    Serial.print(", Data: ");
    Serial.write(packet.data(), packet.length());
    Serial.println();
  #endif

    // Reply:
    if (0 == strncmp("status\n", (const char*)packet.data(), packet.length())) {
      const auto currentIndex = (gLogIndex - 1) % LogCapacity;
      const auto lastTimestampMicroseconds = gLuminocityLog[currentIndex].microseconds_since_epoch;
      const auto lastDateTime = formatTimestamp(lastTimestampMicroseconds);
      const auto startDateTime = formatTimestamp(startTime);
      const auto lastLuminocity = gLuminocityLog[currentIndex].luminocity_value;
      packet.printf("loops: %" PRIu64 ", logs: %u, photoresistor pin: %d, polling freq: %d, "
                    "start time: %s, last time: %s, last luminocity: %" PRIu16 "\n", 
                    gLoops, gLogIndex, kPhotoresPin, PollingFreq, 
                    startDateTime.data(), lastDateTime.data(), lastLuminocity);
    } else {
      packet.write((const uint8_t*)gLuminocityLog.data(), sizeof(gLuminocityLog));
    }
  });

  analogReadResolution(12);
}


void loop() {
  ++gLoops;

  const auto nowMicroseconds = getLocalTimeMicroseconds();
  if (nowMicroseconds < 100 * 1000 * 1000) {
    Serial.printf("NTP didn't updated current time: %" PRIu64 "\n", nowMicroseconds);
    delay(500);
    return;
  }
  if (startTime == 0) {
    startTime = nowMicroseconds;
  }
  constexpr int kPhotoresPin = 34;

  const auto currentIndex = gLogIndex % LogCapacity;
  gLuminocityLog[currentIndex].microseconds_since_epoch = nowMicroseconds;
  gLuminocityLog[currentIndex].luminocity_value = analogRead(kPhotoresPin);
  ++gLogIndex;

  delay(1000 / PollingFreq);
}
