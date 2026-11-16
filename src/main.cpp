// LED Alarm - ESP32-C3 Super Mini
// Turns an LED panel on with a relay at alarm time.
// Control: physical button + phone (ESP32 creates its own WiFi network).

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <sys/time.h>
#include <time.h>

#include "config.h"
#include "secrets.h"
#include "web_page.h"

// ====================== Global objects ======================
ThreeWire rtcWire(PIN_RTC_DAT, PIN_RTC_CLK, PIN_RTC_RST);
RtcDS1302<ThreeWire> rtc(rtcWire);
WebServer server(80);
DNSServer dns;
Preferences prefs;

struct DayAlarm {
  bool enabled;
  uint8_t hour;
  uint8_t minute;
};
DayAlarm alarms[7];  // index = weekday, 0 = Sunday ... 6 = Saturday

enum class Mode : uint8_t { Idle, Ringing, Snoozed };
Mode mode = Mode::Idle;
uint32_t modeStartMs = 0;

bool panelOn    = false;
bool clockValid = false;  // do we know the current time?
bool rtcOk      = false;  // is the DS1302 answering with sane values?
long lastAlarmKey = -1;   // prevents the same alarm from firing twice in one minute

// ====================== Panel (relay) ======================
void setPanel(bool on) {
  panelOn = on;
  digitalWrite(PIN_RELAY, (on == RELAY_ACTIVE_HIGH) ? HIGH : LOW);
  digitalWrite(PIN_STATUS_LED, on ? LOW : HIGH);  // on-board LED is active LOW
  Serial.printf("[panel] %s\n", on ? "ON" : "OFF");
}

// ====================== Clock ======================
// We keep LOCAL time in the ESP32 system clock (time zone = UTC, no conversions).
time_t makeEpoch(int y, int mo, int d, int h, int mi, int s) {
  struct tm t = {};
  t.tm_year = y - 1900;
  t.tm_mon  = mo - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min  = mi;
  t.tm_sec  = s;
  return mktime(&t);
}

void setSystemTime(time_t epoch) {
  timeval tv = {epoch, 0};
  settimeofday(&tv, nullptr);
  clockValid = true;
}

bool getLocalTm(struct tm &t) {
  if (!clockValid) return false;
  time_t now = time(nullptr);
  gmtime_r(&now, &t);
  return true;
}

// Reads the DS1302. Returns false if it is missing or contains garbage.
bool readRtc(time_t &out) {
  if (!rtc.IsDateTimeValid()) return false;
  RtcDateTime dt = rtc.GetDateTime();
  if (dt.Year() < 2024 || dt.Year() > 2099 || dt.Month() < 1 || dt.Month() > 12 ||
      dt.Day() < 1 || dt.Day() > 31 || dt.Hour() > 23 || dt.Minute() > 59 || dt.Second() > 59) {
    return false;
  }
  out = makeEpoch(dt.Year(), dt.Month(), dt.Day(), dt.Hour(), dt.Minute(), dt.Second());
  return true;
}

void writeRtc(const struct tm &t) {
  if (rtc.GetIsWriteProtected()) rtc.SetIsWriteProtected(false);
  if (!rtc.GetIsRunning()) rtc.SetIsRunning(true);
  rtc.SetDateTime(RtcDateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                              t.tm_hour, t.tm_min, t.tm_sec));
}

// ====================== Alarm settings (flash) ======================
void loadAlarms() {
  prefs.begin("alarm", false);
  size_t n = prefs.getBytes("days", alarms, sizeof(alarms));
  prefs.end();
  if (n != sizeof(alarms)) {  // first boot: defaults
    for (auto &a : alarms) a = {false, 7, 0};
  }
}

void saveAlarms() {
  prefs.begin("alarm", false);
  prefs.putBytes("days", alarms, sizeof(alarms));
  prefs.end();
}

// ====================== Alarm state machine ======================
void setMode(Mode m) {
  mode = m;
  modeStartMs = millis();
}

void startAlarm()   { Serial.println("[alarm] RING");    setMode(Mode::Ringing); setPanel(true);  }
void snoozeAlarm()  { Serial.println("[alarm] SNOOZE");  setMode(Mode::Snoozed); setPanel(false); }
void dismissAlarm() { Serial.println("[alarm] DISMISS"); setMode(Mode::Idle);    setPanel(false); }

void checkAlarm() {
  struct tm t;
  if (!getLocalTm(t)) return;
  const DayAlarm &a = alarms[t.tm_wday];
  if (!a.enabled || t.tm_hour != a.hour || t.tm_min != a.minute) return;

  long key = (long)t.tm_yday * 1440 + t.tm_hour * 60 + t.tm_min;
  if (key == lastAlarmKey) return;
  lastAlarmKey = key;

  if (mode == Mode::Idle) startAlarm();
}

void updateMode() {
  uint32_t elapsed = millis() - modeStartMs;
  if (mode == Mode::Ringing && elapsed >= ALARM_DURATION_MS) {
    Serial.println("[alarm] auto-off");
    dismissAlarm();
  } else if (mode == Mode::Snoozed && elapsed >= SNOOZE_MS) {
    startAlarm();
  }
}

// ====================== Button ======================
bool btnStable = HIGH, btnLastRead = HIGH;
uint32_t btnChangeMs = 0;
bool pendingPress = false;  // first press during alarm, waiting for a possible second one
uint32_t firstPressMs = 0;

void onButtonPress() {
  switch (mode) {
    case Mode::Ringing:
      if (pendingPress) {         // second press -> stop alarm completely
        pendingPress = false;
        dismissAlarm();
      } else {                    // first press -> wait, maybe a double press
        pendingPress = true;
        firstPressMs = millis();
      }
      break;
    case Mode::Snoozed:           // any press while snoozed -> cancel the alarm
      dismissAlarm();
      break;
    case Mode::Idle:              // normal use -> toggle panel
      setPanel(!panelOn);
      break;
  }
}

void updateButton() {
  bool r = digitalRead(PIN_BUTTON);
  if (r != btnLastRead) {
    btnLastRead = r;
    btnChangeMs = millis();
  }
  if (millis() - btnChangeMs >= DEBOUNCE_MS && r != btnStable) {
    btnStable = r;
    if (btnStable == LOW) onButtonPress();  // pressed (button pulls pin to GND)
  }
  if (pendingPress && millis() - firstPressMs >= DOUBLE_PRESS_MS) {
    pendingPress = false;               // no second press came -> snooze
    if (mode == Mode::Ringing) snoozeAlarm();
  }
}

// ====================== Web server ======================
const char *modeName() {
  switch (mode) {
    case Mode::Ringing: return "ringing";
    case Mode::Snoozed: return "snoozed";
    default:            return "idle";
  }
}

void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

void handleStatus() {
  struct tm t;
  bool ok = getLocalTm(t);
  char timeStr[24] = "";
  if (ok) {
    snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
  }
  uint32_t remain = 0;
  uint32_t elapsed = millis() - modeStartMs;
  if (mode == Mode::Ringing && elapsed < ALARM_DURATION_MS) remain = (ALARM_DURATION_MS - elapsed) / 1000;
  if (mode == Mode::Snoozed && elapsed < SNOOZE_MS)         remain = (SNOOZE_MS - elapsed) / 1000;

  String json;
  json.reserve(400);
  json += "{\"time\":\"";   json += timeStr;
  json += "\",\"valid\":";  json += ok ? "true" : "false";
  json += ",\"rtc\":";      json += rtcOk ? "true" : "false";
  json += ",\"wday\":";     json += ok ? t.tm_wday : 0;
  json += ",\"panel\":";    json += panelOn ? "true" : "false";
  json += ",\"mode\":\"";   json += modeName();
  json += "\",\"remain\":"; json += remain;
  json += ",\"alarms\":[";
  for (int i = 0; i < 7; i++) {
    if (i) json += ',';
    json += "{\"en\":"; json += alarms[i].enabled ? 1 : 0;
    json += ",\"h\":";  json += alarms[i].hour;
    json += ",\"m\":";  json += alarms[i].minute;
    json += '}';
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handlePanel() {
  String s = server.arg("state");
  bool on = (s == "on") || (s == "toggle" && !panelOn);
  if (mode != Mode::Idle) setMode(Mode::Idle);  // manual control cancels the alarm
  setPanel(on);
  server.send(200, "text/plain", "OK");
}

void handleSnooze() {
  if (mode == Mode::Ringing) snoozeAlarm();
  server.send(200, "text/plain", "OK");
}

void handleDismiss() {
  if (mode != Mode::Idle) dismissAlarm();
  server.send(200, "text/plain", "OK");
}

void handleTime() {
  int y = server.arg("y").toInt(), mo = server.arg("mo").toInt(), d = server.arg("d").toInt();
  int h = server.arg("h").toInt(), mi = server.arg("mi").toInt(), s = server.arg("s").toInt();
  if (y < 2024 || y > 2099 || mo < 1 || mo > 12 || d < 1 || d > 31 ||
      h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 59) {
    server.send(400, "text/plain", "Bad time");
    return;
  }
  setSystemTime(makeEpoch(y, mo, d, h, mi, s));
  struct tm t;
  getLocalTm(t);
  writeRtc(t);
  time_t check;
  rtcOk = readRtc(check);  // verify the DS1302 really stored it
  Serial.printf("[time] synced from phone: %04d-%02d-%02d %02d:%02d:%02d (rtc %s)\n",
                y, mo, d, h, mi, s, rtcOk ? "OK" : "NOT FOUND");
  server.send(200, "text/plain", "OK");
}

void handleAlarms() {
  for (int i = 0; i < 7; i++) {
    String en = server.arg("en" + String(i));
    String tm = server.arg("t" + String(i));  // "HH:MM"
    if (tm.length() < 5) continue;
    int h = tm.substring(0, 2).toInt();
    int m = tm.substring(3, 5).toInt();
    if (h < 0 || h > 23 || m < 0 || m > 59) continue;
    alarms[i] = {en == "1", (uint8_t)h, (uint8_t)m};
  }
  saveAlarms();
  Serial.println("[alarm] settings saved");
  server.send(200, "text/plain", "OK");
}

// Any unknown URL -> redirect to the main page.
// This makes phones open the page automatically ("sign in to network").
void handleNotFound() {
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

// ====================== Setup / Loop ======================
void setup() {
  // Relay OFF before anything else, so the panel doesn't flash at boot
  digitalWrite(PIN_RELAY, RELAY_ACTIVE_HIGH ? LOW : HIGH);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_STATUS_LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  setPanel(false);

  Serial.begin(115200);
  delay(1500);  // give the USB serial time to connect
  Serial.println("\n=== LED Alarm ===");

  setenv("TZ", "UTC0", 1);  // we store local time directly, no time zone math
  tzset();

  loadAlarms();

  rtc.Begin();
  time_t epoch;
  if (readRtc(epoch)) {
    setSystemTime(epoch);
    rtcOk = true;
    Serial.println("[time] loaded from DS1302");
  } else {
    Serial.println("[time] DS1302 missing or not set - open the web page to sync");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);  // known fix for unstable WiFi on C3 Super Mini boards
  dns.start(53, "*", WiFi.softAPIP());
  Serial.printf("[wifi] AP \"%s\" ready, open http://%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  server.on("/",            HTTP_GET,  handleRoot);
  server.on("/api/status",  HTTP_GET,  handleStatus);
  server.on("/api/panel",   HTTP_POST, handlePanel);
  server.on("/api/snooze",  HTTP_POST, handleSnooze);
  server.on("/api/dismiss", HTTP_POST, handleDismiss);
  server.on("/api/time",    HTTP_POST, handleTime);
  server.on("/api/alarms",  HTTP_POST, handleAlarms);
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop() {
  dns.processNextRequest();
  server.handleClient();
  updateButton();
  updateMode();

  static uint32_t lastCheck = 0;
  if (millis() - lastCheck >= 500) {
    lastCheck = millis();
    checkAlarm();
  }
}
