#include <SoftwareSerial.h>
#include <Servo.h>
#include <DHT.h>

// 소프트웨어 시리얼 핀 설정
#define RX_PIN 5  // Arduino의 디지털 핀 5
#define TX_PIN 6  // Arduino의 디지털 핀 6
#define SERVO_PIN A5
#define DC_PIN_A1 9
#define DC_PIN_A2 10
#define DHT_PIN A0
#define DUST_PIN A1
#define WATER_PIN A3
#define LED_PIN 3

// DHT 온습도 센서 설정
DHT dht(DHT_PIN, DHT11);

float dustTmp = 0;

Servo servo;
SoftwareSerial espSerial(RX_PIN, TX_PIN);  // RX, TX 핀 설정

void splitString(String data, char delimiter, String result[], int &count) {
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

// 미세먼지 데이터를 읽어오는 함수
float read_dust() {
  delayMicroseconds(280);
  int voValue = analogRead(DUST_PIN);

  float voltage = voValue * (5.0 / 1024.0);

  float dustDensity = (voltage - 0.1) / 0.005;  // 미세먼지 농도 계산 (mg/m^3)
  if (dustDensity < 0) dustDensity = 0;

  if (dustDensity <= 1) return dustTmp;

  float molecularWeight = 18.0;  // PM2.5의 평균 분자량
  float ppm = (dustDensity * 24.45) / (molecularWeight);

  dustTmp = ppm;

  return ppm;
}

void setup() {
  Serial.begin(4800);
  espSerial.begin(4800);

  servo.attach(SERVO_PIN);
  delay(100);
  servo.write(0);
  servo.detach();
  

  dht.begin();

  pinMode(LED_PIN, OUTPUT);
  pinMode(DC_PIN_A1, OUTPUT);
  pinMode(DC_PIN_A2, OUTPUT);
  digitalWrite(DC_PIN_A1, LOW);
  digitalWrite(DC_PIN_A2, LOW);

  Serial.println("Arduino Ready");
}

void loop() {
  if (espSerial.available()) {
    String data = espSerial.readStringUntil('\n');  // ESP8266으로부터 데이터 읽기
    Serial.println("Received from ESP8266: " + data);

    String parts[10];
    int partCount = 0;

    splitString(data, ':', parts, partCount);

    String cmd = parts[0];

    if (cmd == "DC") {
      if (parts[1] == "1") {
        Serial.println("dc on");
        digitalWrite(DC_PIN_A2, HIGH);
      } else if (parts[1] == "0") {
        Serial.println("dc off");
        digitalWrite(DC_PIN_A2, LOW);
      }
    }
    if (cmd == "LED") {
      if (parts[1] == "1") {
        Serial.println("led on");
        digitalWrite(LED_PIN, HIGH);
      } else if (parts[1] == "0") {
        Serial.println("led off");
        digitalWrite(LED_PIN, LOW);
      }
    }
    if (cmd == "SERVO") {
      if (parts[1] == "1") {
        Serial.println("servo on");
        servo.attach(SERVO_PIN);
        delay(100);
        servo.write(60);
        delay(100);
        servo.detach();
      } else if (parts[1] == "0") {
        Serial.println("servo off");
        servo.attach(SERVO_PIN);
        delay(100);
        servo.write(0);
        delay(100);
        servo.detach();
      }
    }
  }

  // 미세먼지 데이터 읽기
  float dust = read_dust();

  // 온도 및 습도 읽기
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int waterLevel = analogRead(WATER_PIN);

  espSerial.print("TEMP:");
  espSerial.println(temperature);
  espSerial.print("HUMI:");
  espSerial.println(humidity);
  espSerial.print("WATER:");
  espSerial.println(waterLevel);
  espSerial.print("DUST:");
  espSerial.println(dust);

  delay(500);
}
