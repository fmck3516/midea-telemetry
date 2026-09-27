#include <Arduino.h>
#include <WiFi.h>

// Credentials live in secrets.h, which is gitignored. Copy secrets.h.example
// to secrets.h and fill in your network.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "secrets.h missing - copy secrets.h.example to secrets.h and set WIFI_SSID / WIFI_PASSWORD"
#endif

#define CONNECT_TIMEOUT_MS 20000
#define REPORT_INTERVAL_MS 2000

// RSSI is always 0 to -127dBm, so a histogram with one bucket per dBm holds
// every sample in fixed memory and gives exact percentiles however long the
// test runs. Bucket i counts readings of -i dBm.
#define RSSI_BUCKETS 128

uint32_t rssiHistogram[RSSI_BUCKETS];
uint32_t sampleCount = 0;
int32_t bestRssi = INT32_MIN;
int32_t worstRssi = INT32_MAX;
uint32_t lastReport = 0;

// Rough quality buckets for an RSSI reading, in dBm.
const char *signalQuality(int32_t rssi) {
  if (rssi >= -50) return "excellent";
  if (rssi >= -60) return "good";
  if (rssi >= -70) return "fair";
  if (rssi >= -80) return "weak";
  return "unusable";
}

void printRssi(int32_t rssi) {
  HWCDCSerial.print("rssi=");
  HWCDCSerial.print(rssi);
  HWCDCSerial.print("dBm (");
  HWCDCSerial.print(signalQuality(rssi));
  HWCDCSerial.print(")");
}

void printRuntime() {
  uint32_t seconds = millis() / 1000;
  char text[16];
  snprintf(text, sizeof(text), "%02lu:%02lu:%02lu",
           (unsigned long)(seconds / 3600), (unsigned long)(seconds / 60 % 60), (unsigned long)(seconds % 60));
  HWCDCSerial.print("runtime=");
  HWCDCSerial.print(text);
}

void recordRssi(int32_t rssi) {
  rssi = constrain(rssi, -(RSSI_BUCKETS - 1), 0);
  rssiHistogram[-rssi]++;
  sampleCount++;
  if (rssi > bestRssi) bestRssi = rssi;
  if (rssi < worstRssi) worstRssi = rssi;
}

// Nearest-rank percentile, counted from the best reading down: p10=-55
// means 10% of samples were -55dBm or better, p90=-70 means 90% were -70dBm
// or better. p100 is always the worst reading.
int32_t rssiPercentile(uint32_t percent) {
  uint32_t rank = (sampleCount * percent + 99) / 100;
  if (rank == 0) rank = 1;
  uint32_t seen = 0;
  for (int32_t bucket = 0; bucket < RSSI_BUCKETS; bucket++) {
    seen += rssiHistogram[bucket];
    if (seen >= rank) return -bucket;
  }
  return worstRssi;
}

void printReport(int32_t rssi) {
  printRuntime();
  HWCDCSerial.print(", ");
  printRssi(rssi);
  HWCDCSerial.print(", best=");
  HWCDCSerial.print(bestRssi);
  HWCDCSerial.print(", worst=");
  HWCDCSerial.print(worstRssi);
  HWCDCSerial.print(", samples=");
  HWCDCSerial.print(sampleCount);
  for (uint32_t percent = 10; percent <= 100; percent += 10) {
    HWCDCSerial.print(percent == 10 ? ", p" : " p");
    HWCDCSerial.print(percent);
    HWCDCSerial.print('=');
    HWCDCSerial.print(rssiPercentile(percent));
  }
  HWCDCSerial.println();
}

// Scans before connecting, so the access point's signal strength is shown
// even when the connection itself fails (e.g. wrong password).
void scanForNetwork() {
  HWCDCSerial.print("scanning for \"");
  HWCDCSerial.print(WIFI_SSID);
  HWCDCSerial.println("\"...");
  int count = WiFi.scanNetworks();
  bool found = false;
  for (int i = 0; i < count; i++) {
    if (WiFi.SSID(i) != WIFI_SSID) continue;
    found = true;
    HWCDCSerial.print("  found: bssid=");
    HWCDCSerial.print(WiFi.BSSIDstr(i));
    HWCDCSerial.print(", channel=");
    HWCDCSerial.print(WiFi.channel(i));
    HWCDCSerial.print(", ");
    printRssi(WiFi.RSSI(i));
    HWCDCSerial.println();
  }
  if (!found) {
    HWCDCSerial.println("  not found (out of range, hidden, or 5GHz only?)");
  }
  WiFi.scanDelete();
}

bool connectWifi() {
  HWCDCSerial.print("connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > CONNECT_TIMEOUT_MS) {
      HWCDCSerial.print(" failed, status=");
      HWCDCSerial.println(WiFi.status());
      WiFi.disconnect();
      return false;
    }
    HWCDCSerial.print('.');
    delay(500);
  }
  HWCDCSerial.print(" connected in ");
  HWCDCSerial.print(millis() - start);
  HWCDCSerial.print("ms, ip=");
  HWCDCSerial.print(WiFi.localIP());
  HWCDCSerial.print(", channel=");
  HWCDCSerial.print(WiFi.channel());
  HWCDCSerial.print(", ");
  printRssi(WiFi.RSSI());
  HWCDCSerial.println();
  return true;
}

void setup() {
  HWCDCSerial.begin();
  delay(2000);
  WiFi.mode(WIFI_STA);
  scanForNetwork();
  connectWifi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    HWCDCSerial.println("not connected, retrying");
    scanForNetwork();
    connectWifi();
    return;
  }

  // Keep reporting while connected, so the antenna / board can be moved
  // around and the effect watched live.
  if (millis() - lastReport >= REPORT_INTERVAL_MS) {
    lastReport = millis();
    int32_t rssi = WiFi.RSSI();
    recordRssi(rssi);
    printReport(rssi);
  }
}
