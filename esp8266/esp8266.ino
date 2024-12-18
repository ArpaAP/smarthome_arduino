#include <ESP8266WiFi.h>
#include <SocketIoClient.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <TaskScheduler.h>

// 소프트웨어 시리얼 핀 설정
#define RX_PIN 14  // NodeMCU의 D5 핀
#define TX_PIN 12  // NodeMCU의 D6 핀

Scheduler scheduler;

SoftwareSerial arduinoSerial(RX_PIN, TX_PIN);

const char* ssid = "황부연의 iPhone";
const char* password = "gamemode1!";
const char* host = "172.20.10.4";
const int port = 8080;

float temperature = 0;
float humidity = 0;
int waterLevel = 0;
float dust = 0;
int pir = 0;

bool securityMode = false;
bool windowOpen = false;
bool fanOn = false;
bool lightOn = false;

StaticJsonDocument<256> jsonDoc;

SocketIoClient socketIO;

void splitString(String data, char delimiter, String result[], int& count) {
  int startIndex = 0;
  int delimiterIndex = 0;
  count = 0;  // 결과 배열에 저장된 파트 수 초기화

  // delimiter가 더 이상 없을 때까지 반복
  while ((delimiterIndex = data.indexOf(delimiter, startIndex)) != -1) {
    result[count++] = data.substring(startIndex, delimiterIndex);
    startIndex = delimiterIndex + 1;

    // 결과 배열이 꽉 찼으면 멈춤 (안전 장치)
    if (count >= 10) break;
  }
  // 마지막 부분 추가
  result[count] = data.substring(startIndex);
  result[count].trim();
  count++;
}

void messageEventHandler(const char* payload, size_t length) {
  Serial.printf("Message received: %s\n", payload);
}

void actionUpdatedEventHandler(const char* payload, size_t length) {
  StaticJsonDocument<256> jsonDoc;

  // JSON 문자열을 파싱
  DeserializationError error = deserializeJson(jsonDoc, payload);

  // 파싱 오류 처리
  if (error) {
    Serial.print("JSON Parsing failed: ");
    Serial.println(error.c_str());
    return;
  }

  // JSON 데이터 읽기
  bool newSecurityMode = jsonDoc["securityMode"];
  bool newWindowOpen = jsonDoc["windowOpen"];
  bool newFanOn = jsonDoc["fanOn"];
  bool newLightOn = jsonDoc["lightOn"];

  if (windowOpen != newWindowOpen) {
    if (newWindowOpen) {
      arduinoSerial.print("SERVO:1");
      arduinoSerial.print(";");
    } else {
      arduinoSerial.print("SERVO:0");
      arduinoSerial.print(";");
    }
  }

  if (fanOn != newFanOn) {
    if (newFanOn) {
      arduinoSerial.print("DC:1");
      arduinoSerial.print(";");
    } else {
      arduinoSerial.print("DC:0");
      arduinoSerial.print(";");
    }
  }

  if (lightOn != newLightOn) {
    if (newLightOn) {
      Serial.println("led = ");
      Serial.println(lightOn);
      arduinoSerial.print("LED:1");
      arduinoSerial.print(";");
    } else {
      arduinoSerial.print("LED:0");
      arduinoSerial.print(";");
    }
  }

  securityMode = newSecurityMode;
  windowOpen = newWindowOpen;
  fanOn = newFanOn;
  lightOn = newLightOn;
}

Task socketTask(TASK_IMMEDIATE, TASK_FOREVER, &socketTaskCallback);
Task serialTask(300, TASK_FOREVER, &serialTaskCallback);

void setup() {
  Serial.begin(4800);
  arduinoSerial.begin(4800);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  socketIO.begin(host, port, "/socket.io/?transport=websocket");
  socketIO.on("actionUpdated", actionUpdatedEventHandler);

  scheduler.addTask(socketTask);
  scheduler.addTask(serialTask);
  socketTask.enable();
  serialTask.enable();
}

void socketTaskCallback() {
  socketIO.loop();
}

void serialTaskCallback() {
  if (arduinoSerial.available()) {
    String data = arduinoSerial.readStringUntil(';');  // ESP8266으로부터 데이터 읽기
    Serial.println("Received from Arduino: " + data);

    String parts[10];
    int partCount = 0;

    splitString(data, ':', parts, partCount);

    String cmd = parts[0];

    if (cmd == "TEMP") {
      temperature = parts[1].toFloat();
      jsonDoc["temperature"] = temperature;
    }
    if (cmd == "HUMI") {
      humidity = parts[1].toFloat();
      jsonDoc["humidity"] = humidity;
    }
    if (cmd == "WATER") {
      waterLevel = parts[1].toInt();
      jsonDoc["waterLevel"] = waterLevel;
    }
    if (cmd == "DUST") {
      dust = parts[1].toFloat();
      jsonDoc["dust"] = dust;
    }
    if (cmd == "PIR") {
      pir = parts[1].toInt();
      Serial.println(pir);
      jsonDoc["pir"] = pir ? true : false;
    }

    String jsonString;
    serializeJson(jsonDoc, jsonString);

    socketIO.emit("updateSensorMeasurements", jsonString.c_str());
  }
}

void loop() {
  scheduler.execute();
}
