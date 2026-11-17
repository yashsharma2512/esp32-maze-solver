#include "WifiSerialConsole.h"
#include "WifiConsolePage.h"
#include "MotorStop.h"

#include <ArduinoOTA.h>
#include <WebServer.h>
#include <WiFi.h>
#include <atomic>
#include <strings.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#if __has_include("WifiSecrets.h")
#include "WifiSecrets.h"
#else
constexpr char MAZE_WIFI_SSID[] = "";
constexpr char MAZE_WIFI_PASSWORD[] = "";
#endif

namespace {
constexpr char AP_NAME[] = "MazeSolver";
constexpr char HOSTNAME[] = "mazesolver";
constexpr char WEB_USER[] = "maze";
constexpr char ACCESS_PASSWORD[] = "12345678";
constexpr char COMMAND_HEADER[] = "X-Maze-Console";
constexpr size_t LOG_CAPACITY = 4096;
constexpr size_t COMMAND_CAPACITY = 256;

WebServer server(80);
QueueHandle_t commands = nullptr;
StaticQueue_t usbQueueStorage;
uint8_t usbCommandBytes[COMMAND_CAPACITY];
QueueHandle_t usbCommands = nullptr;
std::atomic<bool> startupOpen{false};
bool otaOpen = false;

portMUX_TYPE logLock = portMUX_INITIALIZER_UNLOCKED;
char logBytes[LOG_CAPACITY];
uint32_t logNext = 0;
size_t logSize = 0;

bool authenticateConsole() {
  if (server.authenticate(WEB_USER, ACCESS_PASSWORD)) return true;
  server.requestAuthentication(BASIC_AUTH, "MazeSolver");
  return false;
}

void readUsbCommands() {
  static char line[40];
  static size_t length = 0;
  static bool tooLong = false;
  for (int byte = Serial.read(); byte >= 0; byte = Serial.read()) {
    if (byte == '\r' || byte == '\n') {
      line[length] = '\0';
      if (!tooLong && strcasecmp(line, "reset") == 0) {
        if (prepareRobotReset()) restartRobot();
      } else if (!tooLong && strcasecmp(line, "stop") == 0) {
        requestMotorStop();
        xQueueReset(usbCommands);
      } else if (!tooLong && strcasecmp(line, "start") == 0 && requestMotorResume()) {
        // The motor loop handles resuming.
      } else if (!motorStopRequested() && !tooLong && length != 0 &&
                 uxQueueSpacesAvailable(usbCommands) >= length + 1) {
        line[length++] = '\n';
        for (size_t index = 0; index < length; ++index) xQueueSend(usbCommands, &line[index], 0);
      }
      length = 0;
      tooLong = false;
    } else if (byte < ' ' && byte != '\t') {
      tooLong = true;
    } else if (length < sizeof(line) - 1) {
      line[length++] = char(byte);
    } else {
      tooLong = true;
    }
  }
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
  String command = server.arg("command");
  command.trim();
  if (command.equalsIgnoreCase("reset")) {
    const bool accepted = prepareRobotReset();
    server.send(202, "text/plain", "Rebooting ESP32. Reconnect after it starts.");
    if (accepted) restartRobot();
    return;
  }
  if (command.equalsIgnoreCase("stop")) {
    requestMotorStop();
    xQueueReset(commands);
    server.send(200, "text/plain", "Motor drive paused. Send start to resume.");
    return;
  }
  if (command.equalsIgnoreCase("start") && requestMotorResume()) {
    server.send(202, "text/plain", "Resume requested.");
    return;
  }
  if (!startupOpen.load() || motorStopRequested()) {
    server.send(409, "text/plain", "Startup commands are unavailable during a run. Use stop, start, or reset.");
    return;
  }
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
  usbCommands = xQueueCreateStatic(COMMAND_CAPACITY, sizeof(char), usbCommandBytes, &usbQueueStorage);
  readUsbCommands();
  Serial.onReceive(readUsbCommands);
  if (!WiFi.mode(WIFI_AP_STA)) {
    Serial.println("Wi-Fi mode unavailable.");
    return;
  }
  const bool apStarted = WiFi.softAP(AP_NAME, ACCESS_PASSWORD);
  if (MAZE_WIFI_SSID[0] != '\0') WiFi.begin(MAZE_WIFI_SSID, MAZE_WIFI_PASSWORD);
  if (!apStarted && MAZE_WIFI_SSID[0] == '\0') {
    Serial.println("MazeSolver hotspot unavailable.");
    return;
  }
  commands = xQueueCreate(COMMAND_CAPACITY, sizeof(char));
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
  Serial.printf("Wi-Fi and OTA password: %s | console user: %s\n", ACCESS_PASSWORD, WEB_USER);
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(ACCESS_PASSWORD);
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

int WifiSerialConsole::readUsb() {
  char byte;
  if (xQueueReceive(usbCommands, &byte, 0) != pdTRUE) return -1;
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
