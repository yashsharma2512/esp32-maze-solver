#include "WifiSerialConsole.h"
#include "WifiConsolePage.h"

#include <ArduinoOTA.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <atomic>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#if __has_include("WifiSecrets.h")
#include "WifiSecrets.h"
#else
constexpr char MAZE_WIFI_SSID[] = "";
constexpr char MAZE_WIFI_PASSWORD[] = "";
#endif

#ifndef MAZE_ACCESS_PASSWORD
#define MAZE_ACCESS_PASSWORD ""
#endif

namespace {
constexpr char AP_NAME[] = "MazeSolver";
constexpr char HOSTNAME[] = "mazesolver";
constexpr char WEB_USER[] = "maze";
constexpr char FIXED_PASSWORD[] = MAZE_ACCESS_PASSWORD;
constexpr size_t FIXED_PASSWORD_LENGTH = sizeof(FIXED_PASSWORD) - 1;
static_assert(FIXED_PASSWORD_LENGTH == 0 ||
              (FIXED_PASSWORD_LENGTH >= 8 && FIXED_PASSWORD_LENGTH <= 63),
              "MAZE_ACCESS_PASSWORD must be empty or 8 to 63 characters.");
constexpr char COMMAND_HEADER[] = "X-Maze-Console";
constexpr size_t LOG_CAPACITY = 4096;

WebServer server(80);
QueueHandle_t commands = nullptr;
std::atomic<bool> startupOpen{false};
bool otaOpen = false;
const char *password = nullptr;

portMUX_TYPE logLock = portMUX_INITIALIZER_UNLOCKED;
char logBytes[LOG_CAPACITY];
uint32_t logNext = 0;
size_t logSize = 0;

const char *loadPassword() {
  if (FIXED_PASSWORD_LENGTH != 0) return FIXED_PASSWORD;

  static char generatedPassword[13];
  Preferences settings;
  const bool opened = settings.begin("maze_wifi", false);
  const String saved = opened ? settings.getString("pass") : String();
  if (saved.length() == sizeof(generatedPassword) - 1) {
    saved.toCharArray(generatedPassword, sizeof(generatedPassword));
  } else {
    snprintf(generatedPassword, sizeof(generatedPassword), "%08lx%04lx",
             static_cast<unsigned long>(esp_random()),
             static_cast<unsigned long>(esp_random() & 0xffff));
    if (opened) settings.putString("pass", generatedPassword);
  }
  if (opened) settings.end();
  return generatedPassword;
}

bool authenticateConsole() {
  if (server.authenticate(WEB_USER, password)) return true;
  server.requestAuthentication(BASIC_AUTH, "MazeSolver");
  return false;
}

void sendLog() {
  if (!authenticateConsole()) return;
  uint32_t from = strtoul(server.arg("from").c_str(), nullptr, 10);
  char batch[1025];
  portENTER_CRITICAL(&logLock);
  if (logNext - from > logSize) from = logNext - logSize;
  const size_t length = min(size_t(logNext - from), sizeof(batch) - 1);
  for (size_t i = 0; i < length; ++i) batch[i] = logBytes[(from + i) % LOG_CAPACITY];
  portEXIT_CRITICAL(&logLock);
  batch[length] = '\0';
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Log-Next", String(uint32_t(from + length)));
  server.send(200, "text/plain; charset=utf-8", batch);
}

void receiveCommand() {
  if (!authenticateConsole()) return;
  if (server.header(COMMAND_HEADER) != "1") {
    server.send(403, "text/plain", "Send commands from the robot console.");
    return;
  }
  if (!startupOpen.load()) {
    server.send(409, "text/plain", "The robot has started. Reset it to use startup commands.");
    return;
  }
  String command = server.arg("command");
  command.trim();
  if (command.length() == 0 || command.length() > 39) {
    server.send(400, "text/plain", "Enter one command of up to 39 characters.");
    return;
  }
  for (size_t i = 0; i < command.length(); ++i) {
    if (command[i] < ' ' || command[i] > '~') {
      server.send(400, "text/plain", "Enter one line of plain text.");
      return;
    }
  }
  if (uxQueueSpacesAvailable(commands) < command.length() + 1) {
    server.send(503, "text/plain", "Command queue full. Try again.");
    return;
  }
  for (size_t i = 0; i < command.length(); ++i) {
    const char byte = command[i];
    xQueueSend(commands, &byte, 0);
  }
  const char newline = '\n';
  xQueueSend(commands, &newline, 0);
  server.send(202, "text/plain", "Command queued.");
}

void runConsole(void *) {
  IPAddress lastStationIP;
  for (;;) {
    server.handleClient();
    const IPAddress stationIP = WiFi.status() == WL_CONNECTED ? WiFi.localIP() : IPAddress();
    if (stationIP != lastStationIP) {
      lastStationIP = stationIP;
      if (stationIP != IPAddress()) {
        SerialDebug.printf("Router console: http://%s\n", stationIP.toString().c_str());
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
}  // namespace

WifiSerialConsole SerialDebug;

void WifiSerialConsole::begin() {
  if (!WiFi.mode(WIFI_AP_STA)) {
    Serial.println("Wi-Fi mode unavailable.");
    return;
  }
  password = loadPassword();
  const bool apStarted = WiFi.softAP(AP_NAME, password);
  if (MAZE_WIFI_SSID[0] != '\0') WiFi.begin(MAZE_WIFI_SSID, MAZE_WIFI_PASSWORD);
  if (!apStarted && MAZE_WIFI_SSID[0] == '\0') {
    Serial.println("MazeSolver hotspot unavailable.");
    return;
  }
  commands = xQueueCreate(256, sizeof(char));
  if (!commands) {
    Serial.println("Wi-Fi command buffer unavailable.");
    return;
  }

  const char *headers[] = {COMMAND_HEADER};
  server.collectHeaders(headers, 1);
  server.on("/", HTTP_GET, [] {
    if (authenticateConsole()) {
      server.sendHeader("Cache-Control", "no-store");
      server.send_P(200, "text/html; charset=utf-8", WIFI_CONSOLE_PAGE);
    }
  });
  server.on("/logs", HTTP_GET, sendLog);
  server.on("/command", HTTP_POST, receiveCommand);
  server.begin();
  startupOpen.store(true);
  if (xTaskCreatePinnedToCore(runConsole, "Maze console", 6144, nullptr, 1, nullptr, 0) != pdPASS) {
    startupOpen.store(false);
    server.stop();
    Serial.println("Wi-Fi console task unavailable.");
  }

  if (apStarted) {
    SerialDebug.printf("Wi-Fi hotspot: %s | console: http://%s\n", AP_NAME, WiFi.softAPIP().toString().c_str());
  }
  if (FIXED_PASSWORD_LENGTH == 0) {
    Serial.printf("Wi-Fi and OTA password: %s | console user: %s\n", password, WEB_USER);
  } else {
    Serial.printf("Using the password from WifiSecrets.h | console user: %s\n", WEB_USER);
  }
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(password);
  ArduinoOTA.onStart([] { SerialDebug.println("OTA upload started."); });
  ArduinoOTA.onError([](ota_error_t error) { SerialDebug.printf("OTA error %u.\n", unsigned(error)); });
  ArduinoOTA.begin();
  otaOpen = true;
  SerialDebug.printf("OTA upload port: %s | available during startup\n", HOSTNAME);
}

void WifiSerialConsole::handleOta() {
  if (otaOpen) ArduinoOTA.handle();
}

void WifiSerialConsole::endStartup() {
  startupOpen.store(false);
  if (otaOpen) {
    ArduinoOTA.end();
    otaOpen = false;
  }
}

int WifiSerialConsole::readNetwork() {
  char byte;
  if (!commands || xQueueReceive(commands, &byte, 0) != pdTRUE) return -1;
  return static_cast<unsigned char>(byte);
}

size_t WifiSerialConsole::write(uint8_t byte) {
  return write(&byte, 1);
}

size_t WifiSerialConsole::write(const uint8_t *data, size_t length) {
  const size_t written = Serial.write(data, length);
  const size_t kept = min(length, LOG_CAPACITY);
  portENTER_CRITICAL(&logLock);
  logNext += uint32_t(length - kept);
  for (size_t i = length - kept; i < length; ++i) logBytes[logNext++ % LOG_CAPACITY] = char(data[i]);
  logSize = min(LOG_CAPACITY, logSize + kept);
  portEXIT_CRITICAL(&logLock);
  return written;
}
