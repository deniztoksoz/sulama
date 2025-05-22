#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

RTC_DS3231 rtc;
SoftwareSerial bluetooth(10, 11); // RX, TX

const int relayPin = 8;

// EEPROM adresleri
const int addrIntervalDays = 0;
const int addrStartHour = 1;
const int addrStartMinute = 2;
const int addrDuration = 3;

int intervalDays = 0;
int startHour = 0;
int startMinute = 0;
int durationHours = 0;

DateTime lastWatered;
bool isWatering = false;

void setup() {
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
  
  bluetooth.begin(9600);
  Serial.begin(9600);
  
  if (!rtc.begin()) {
    Serial.println("DS3231 bulunamadı!");
    while (1);
  }

  loadFromEEPROM();
  lastWatered = rtc.now() - TimeSpan(intervalDays * 86400); // başlangıç için geçmişte
}

void loop() {
  checkBluetooth();
  checkWateringSchedule();
  delay(1000);
}

void checkBluetooth() {
  if (bluetooth.available()) {
    String input = bluetooth.readStringUntil('\n');
    input.trim();
    
    int sep1 = input.indexOf(',');
    int sep2 = input.indexOf(',', sep1 + 1);
    
    if (sep1 > 0 && sep2 > sep1) {
      intervalDays = input.substring(0, sep1).toInt();
      String timePart = input.substring(sep1 + 1, sep2);
      durationHours = input.substring(sep2 + 1).toInt();

      startHour = timePart.substring(0, timePart.indexOf(':')).toInt();
      startMinute = timePart.substring(timePart.indexOf(':') + 1).toInt();

      saveToEEPROM();
      lastWatered = rtc.now() - TimeSpan(intervalDays * 86400); // sıfırla

      bluetooth.println("Ayarlar kaydedildi.");
    } else {
      bluetooth.println("Geçersiz format. Örnek: 2,14:00,5");
    }
  }
}

void checkWateringSchedule() {
  DateTime now = rtc.now();
  TimeSpan sinceLast = now - lastWatered;

  if (!isWatering && sinceLast.days() >= intervalDays &&
      now.hour() == startHour && now.minute() == startMinute) {
    startWatering(now);
  }

  if (isWatering && (now - lastWatered).totalseconds() >= durationHours * 3600) {
    stopWatering();
  }
}

void startWatering(DateTime now) {
  digitalWrite(relayPin, HIGH);
  lastWatered = now;
  isWatering = true;
  Serial.println("Sulama başladı.");
  bluetooth.println("Sulama başladı.");
}

void stopWatering() {
  digitalWrite(relayPin, LOW);
  isWatering = false;
  Serial.println("Sulama bitti.");
  bluetooth.println("Sulama bitti.");
}

void saveToEEPROM() {
  EEPROM.update(addrIntervalDays, intervalDays);
  EEPROM.update(addrStartHour, startHour);
  EEPROM.update(addrStartMinute, startMinute);
  EEPROM.update(addrDuration, durationHours);
}

void loadFromEEPROM() {
  intervalDays = EEPROM.read(addrIntervalDays);
  startHour = EEPROM.read(addrStartHour);
  startMinute = EEPROM.read(addrStartMinute);
  durationHours = EEPROM.read(addrDuration);
}
