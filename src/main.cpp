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

struct Settings {
  uint16_t alarmMin;
  uint16_t snoozeMin;
  uint16_t manualOffMin;
};
Settings cfg = {DEFAULT_ALARM_MIN, DEFAULT_SNOOZE_MIN, DEFAULT_MANUAL_OFF_MIN};

enum class Mode : uint8_t { Idle, Ringing, Snoozed };
Mode mode = Mode::Idle;
uint32_t modeStartMs = 0;
uint32_t panelOnSinceMs = 0;

bool panelOn    = false;
bool clockValid = false;  // do we know the current time?
bool rtcOk      = false;  // is the DS1302 answering with sane values?
long lastAlarmKey = -1;   // prevents the same alarm from firing twice in one minute

// ====================== Panel (relay) ======================
void setPanel(bool on) {
  if (on && !panelOn) panelOnSinceMs = millis();
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

void printRtcDiag(const char *tag) {
  RtcDateTime dt = rtc.GetDateTime();
  Serial.printf("[rtc:%s] valid=%d running=%d wp=%d raw=%04u-%02u-%02u %02u:%02u:%02u\n",
                tag,
                rtc.IsDateTimeValid() ? 1 : 0,
                rtc.GetIsRunning() ? 1 : 0,
                rtc.GetIsWriteProtected() ? 1 : 0,
                dt.Year(), dt.Month(), dt.Day(), dt.Hour(), dt.Minute(), dt.Second());
}

// ====================== Alarm & Settings (flash) ======================
void loadAlarms() {
  prefs.begin("alarm", false);
  size_t n = prefs.getBytes("days", alarms, sizeof(alarms));
  cfg.alarmMin     = prefs.getUShort("alarmMin", DEFAULT_ALARM_MIN);
  cfg.snoozeMin    = prefs.getUShort("snoozeMin", DEFAULT_SNOOZE_MIN);
  cfg.manualOffMin = prefs.getUShort("manOffMin", DEFAULT_MANUAL_OFF_MIN);
  prefs.end();
  if (n != sizeof(alarms)) {  // first boot: defaults
    for (auto &a : alarms) a = {false, 7, 0};
  }
  if (cfg.alarmMin > MAX_DURATION_MIN)     cfg.alarmMin = DEFAULT_ALARM_MIN;
  if (cfg.snoozeMin < 1 || cfg.snoozeMin > MAX_SNOOZE_MIN) cfg.snoozeMin = DEFAULT_SNOOZE_MIN;
  if (cfg.manualOffMin > MAX_DURATION_MIN) cfg.manualOffMin = DEFAULT_MANUAL_OFF_MIN;
}

void saveAlarms() {
  prefs.begin("alarm", false);
  prefs.putBytes("days", alarms, sizeof(alarms));
  prefs.end();
}

void saveSettings() {
  prefs.begin("alarm", false);
  prefs.putUShort("alarmMin", cfg.alarmMin);
  prefs.putUShort("snoozeMin", cfg.snoozeMin);
  prefs.putUShort("manOffMin", cfg.manualOffMin);
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
  if (mode == Mode::Ringing) {
    if (cfg.alarmMin > 0 && elapsed >= (uint32_t)cfg.alarmMin * 60000UL) {
      Serial.println("[alarm] auto-off");
      dismissAlarm();
    }
  } else if (mode == Mode::Snoozed) {
    if (elapsed >= (uint32_t)cfg.snoozeMin * 60000UL) {
      startAlarm();
    }
  } else if (panelOn && cfg.manualOffMin > 0) {
    if (millis() - panelOnSinceMs >= (uint32_t)cfg.manualOffMin * 60000UL) {
      Serial.println("[panel] manual auto-off");
      setPanel(false);
    }
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

void sendNoCache() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
}

void handleRoot() {
  sendNoCache();
  server.send_P(200, "text/html", INDEX_HTML);
}

struct NextAlarmInfo {
  int day;     // 0..6, or -1 if none
  int offset;  // 0..7 days ahead
  uint8_t h;
  uint8_t m;
  int inMin;
};

NextAlarmInfo findNextAlarm(const struct tm &t, bool valid) {
  NextAlarmInfo info = {-1, 0, 0, 0, 0};
  if (!valid) return info;
  int nowMin = t.tm_hour * 60 + t.tm_min;
  for (int offset = 0; offset <= 7; offset++) {
    int idx = (t.tm_wday + offset) % 7;
    if (!alarms[idx].enabled) continue;
    int alarmMin = alarms[idx].hour * 60 + alarms[idx].minute;
    if (offset == 0 && alarmMin <= nowMin) continue;
    info.day = idx;
    info.offset = offset;
    info.h = alarms[idx].hour;
    info.m = alarms[idx].minute;
    info.inMin = offset * 1440 + alarmMin - nowMin;
    return info;
  }
  return info;
}

uint32_t remainingSec(uint32_t startMs, uint16_t minutes) {
  if (minutes == 0) return 0;
  uint32_t durMs = (uint32_t)minutes * 60000UL;
  uint32_t elapsed = millis() - startMs;
  return (elapsed < durMs) ? (durMs - elapsed) / 1000 : 0;
}

void handleStatus() {
  sendNoCache();
  struct tm t;
  bool ok = getLocalTm(t);
  char timeStr[32] = "";
  if (ok) {
    snprintf(timeStr, sizeof(timeStr), "%04u-%02u-%02u %02u:%02u:%02u",
             (unsigned)(t.tm_year + 1900) % 10000U, (unsigned)(t.tm_mon + 1) % 100U,
             (unsigned)t.tm_mday % 100U, (unsigned)t.tm_hour % 100U,
             (unsigned)t.tm_min % 100U, (unsigned)t.tm_sec % 100U);
  }
  uint32_t remain = 0;
  if (mode == Mode::Ringing)      remain = remainingSec(modeStartMs, cfg.alarmMin);
  else if (mode == Mode::Snoozed) remain = remainingSec(modeStartMs, cfg.snoozeMin);
  else if (panelOn)               remain = remainingSec(panelOnSinceMs, cfg.manualOffMin);

  NextAlarmInfo nxt = findNextAlarm(t, ok);

  String json;
  json.reserve(640);
  json += "{\"time\":\"";   json += timeStr;
  json += "\",\"valid\":";  json += ok ? "true" : "false";
  json += ",\"rtc\":";      json += rtcOk ? "true" : "false";
  json += ",\"wday\":";     json += ok ? t.tm_wday : 0;
  json += ",\"panel\":";    json += panelOn ? "true" : "false";
  json += ",\"mode\":\"";   json += modeName();
  json += "\",\"remain\":"; json += remain;
  json += ",\"cfg\":{\"alarmMin\":";   json += cfg.alarmMin;
  json += ",\"snoozeMin\":";           json += cfg.snoozeMin;
  json += ",\"manualOffMin\":";        json += cfg.manualOffMin;
  json += "},\"next\":{\"day\":";      json += nxt.day;
  json += ",\"offset\":";              json += nxt.offset;
  json += ",\"h\":";                   json += nxt.h;
  json += ",\"m\":";                   json += nxt.m;
  json += ",\"inMin\":";               json += nxt.inMin;
  json += "},\"alarms\":[";
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
  sendNoCache();
  String s = server.arg("state");
  bool on = (s == "on") || (s == "toggle" && !panelOn);
  if (mode != Mode::Idle) setMode(Mode::Idle);  // manual control cancels the alarm
  setPanel(on);
  server.send(200, "text/plain", "OK");
}

void handleSnooze() {
  sendNoCache();
  if (mode == Mode::Ringing) snoozeAlarm();
  server.send(200, "text/plain", "OK");
}

void handleDismiss() {
  sendNoCache();
  if (mode != Mode::Idle) dismissAlarm();
  server.send(200, "text/plain", "OK");
}

void handleTime() {
  sendNoCache();
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
  printRtcDiag("sync");
  Serial.printf("[time] synced from phone: %04d-%02d-%02d %02d:%02d:%02d (rtc %s)\n",
                y, mo, d, h, mi, s, rtcOk ? "OK" : "NOT FOUND");
  server.send(200, "text/plain", "OK");
}

void handleAlarms() {
  sendNoCache();
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
  Serial.println("[alarm] alarms saved");
  server.send(200, "text/plain", "OK");
}

void handleSettings() {
  sendNoCache();
  if (!server.hasArg("alarmMin") || !server.hasArg("snoozeMin") || !server.hasArg("manualOffMin")) {
    server.send(400, "text/plain", "Missing settings");
    return;
  }
  int aMin = constrain(server.arg("alarmMin").toInt(), 0, MAX_DURATION_MIN);
  int sMin = constrain(server.arg("snoozeMin").toInt(), 1, MAX_SNOOZE_MIN);
  int mMin = constrain(server.arg("manualOffMin").toInt(), 0, MAX_DURATION_MIN);
  cfg.alarmMin     = (uint16_t)aMin;
  cfg.snoozeMin    = (uint16_t)sMin;
  cfg.manualOffMin = (uint16_t)mMin;
  saveSettings();
  Serial.printf("[cfg] alarm=%umin snooze=%umin manualOff=%umin\n",
                cfg.alarmMin, cfg.snoozeMin, cfg.manualOffMin);
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
  digitalWrite(PIN_STATUS_LED, HIGH);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_STATUS_LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);  // never block when USB serial host is not reading
  delay(1500);               // give the USB serial time to connect
  Serial.println("\n=== LED Alarm ===");
  setPanel(false);

  setenv("TZ", "UTC0", 1);  // we store local time directly, no time zone math
  tzset();

  loadAlarms();

  rtc.Begin();
  printRtcDiag("boot");
  time_t epoch;
  if (readRtc(epoch)) {
    setSystemTime(epoch);
    rtcOk = true;
    Serial.println("[time] loaded from DS1302");
  } else {
    rtc.SetIsWriteProtected(false);
    rtc.SetIsRunning(true);
    printRtcDiag("unlock");
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
  server.on("/api/alarms",   HTTP_POST, handleAlarms);
  server.on("/api/settings", HTTP_POST, handleSettings);
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
