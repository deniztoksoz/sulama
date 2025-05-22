/*
Aşağıda, Arduino Nano, DS3231 saat modülü ve Bluetooth modülü (örneğin HC-05) kullanarak oluşturabileceğin bir otomatik sulama sistemi için örnek bir kod veriyorum. Bu sistem:
Bluetooth üzerinden gelen komutu alır (örneğin: "2,14:00,5" → 2 gün arayla, saat 14:00’te başlasın ve 5 saat çalışsın).
Bu komutu EEPROM’a kaydeder.
DS3231'den zamanı kontrol ederek zamanı geldiğinde bir röle çıkışını aktif eder.
Çalışma süresi bittiğinde röleyi kapatır.
Not: EEPROM'u uzun ömürlü kullanmak için sadece ayar değiştiğinde yazmak gerekir. Aşağıdaki kodda bu dikkate alınmıştır.

Donanım Bağlantıları
DS3231 → I2C (A4-SDA, A5-SCL)
HC-05 Bluetooth → RX-TX (SoftwareSerial)
Röle → Dijital pin (örneğin D8)

*/
//------------------------------------------------------------------------------------------------------------------------
// manuel saatli açma kapamalı..
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
bool manualMode = false;
unsigned long manualStartTime = 0;
unsigned long manualDurationMillis = 0;

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
  lastWatered = rtc.now() - TimeSpan(intervalDays * 86400); // geçmişte başlat

  sendProgramInfo();
}

void loop() {
  checkBluetooth();

  if (manualMode && isWatering && millis() - manualStartTime >= manualDurationMillis) {
    stopWatering();
    manualMode = false;
    bluetooth.println("MANUEL süre tamamlandı. Sulama kapatıldı.");
  }

  if (!manualMode) {
    checkWateringSchedule();
  }

  delay(1000);
}

void checkBluetooth() {
  if (bluetooth.available()) {
    String input = bluetooth.readStringUntil('\n');
    input.trim();
   //input.toUpperCase();

    if (input.startsWith("ac")) {
      int commaIndex = input.indexOf(',');
      if (commaIndex != -1) {
        int manuelSure = input.substring(commaIndex + 1).toInt();
        manualDurationMillis = (unsigned long)manuelSure * 3600000UL;
      } else {
        manualDurationMillis = 0xFFFFFFFF; // süresiz açık kalır
      }

      manualStartTime = millis();
      manualMode = true;
      startWatering(rtc.now());
      bluetooth.println("MANUEL: Sulama açıldı.");
    }
    else if (input == "kapa") {
      stopWatering();
      manualMode = false;
      bluetooth.println("MANUEL: Sulama kapatıldı.");
    }
    else if (input.indexOf(',') != -1) {
      // Zamanlı program ayarı
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
        manualMode = false;

        bluetooth.println("ZAMANLI AYAR: Kaydedildi.");
        sendProgramInfo();
      } else {
        bluetooth.println("Geçersiz format. Örnek: 2,14:00,5");
      }
    } else {
      bluetooth.println("Bilinmeyen komut.");
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
  Serial.println("Sulama kapandı.");
  bluetooth.println("Sulama kapandı.");
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

void sendProgramInfo() {
  bluetooth.print("Kayıtlı Program: ");
  bluetooth.print(intervalDays);
  bluetooth.print(" gün arayla, saat ");
  if (startHour < 10) bluetooth.print("0");
  bluetooth.print(startHour);
  bluetooth.print(":");
  if (startMinute < 10) bluetooth.print("0");
  bluetooth.print(startMinute);
  bluetooth.print(", ");
  bluetooth.print(durationHours);
  bluetooth.println(" saat sulama.");
}


//--------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------
//manuel açam kapamalı


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
bool manualMode = false;

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
  lastWatered = rtc.now() - TimeSpan(intervalDays * 86400); // geçmişte başlat
}

void loop() {
  checkBluetooth();
  if (!manualMode) {
    checkWateringSchedule();
  }
  delay(10000); // burayı denemek lazım 1 dakikda da bir çalışırsa daha iyi olur.....
}

void checkBluetooth() {
  if (bluetooth.available()) {
    String input = bluetooth.readStringUntil('#');
    input.trim();
    //input.toUpperCase(); // buna gerek yok bence...................

    if (input == "1") {
      manualMode = true;
      startWatering(rtc.now());
      bluetooth.println("MANUEL: Sulama açıldı.");
    }
    else if (input == "2") {
      stopWatering();
      manualMode = false;
      bluetooth.println("MANUEL: Sulama kapatıldı.");
    }
    else if (input.indexOf(',') != -1) {
      // Zamanlı komut
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
        manualMode = false;

        bluetooth.println("ZAMANLI AYAR: Kaydedildi.");
      } else {
        bluetooth.println("Geçersiz format. Örnek: 2,14:00,5");
      }
    } else {
      bluetooth.println("Bilinmeyen komut.");
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
  Serial.println("Sulama kapandı.");
  bluetooth.println("Sulama kapandı.");
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



//-------------------------------------------------------------------------------------------------------------------------

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
