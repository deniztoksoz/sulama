/* DEEPSEEK
Bluetooth Komutları
Program Ayarlama:

SET day startH startM endH endM enabled

Örnek: SET 0 8 0 8 30 1 (Pazartesi 08:00-08:30 açık)

Programı Görüntüleme:

GET (Tüm programı gönderir)

Zamanı Görüntüleme:

TIME (Şu anki zamanı gönderir)

Röle Kontrolü:

ON (Röleyi açar)

OFF (Röleyi kapatır)

Durum Sorgulama:

STATUS (Röle durumu ve zamanı gönderir)
  */




#include <DS1302.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

// Pin tanımlamaları
#define RELAY_PIN 7
#define RTC_RST_PIN 2
#define RTC_DAT_PIN 3
#define RTC_CLK_PIN 4
#define BT_RX_PIN 10
#define BT_TX_PIN 11

// EEPROM adresleri
#define EEPROM_START_ADDR 0
#define EEPROM_MAGIC_NUMBER 0xAA
#define SCHEDULE_SIZE 7 // 7 gün için

// Program yapısı
struct Schedule {
  byte startHour;
  byte startMinute;
  byte endHour;
  byte endMinute;
  bool enabled;
};

// Nesneler
DS1302 rtc(RTC_RST_PIN, RTC_DAT_PIN, RTC_CLK_PIN);
SoftwareSerial bluetooth(BT_RX_PIN, BT_TX_PIN); // RX, TX

// Değişkenler
Schedule weeklySchedule[SCHEDULE_SIZE];
bool relayState = false;

void setup() {
  // Pin ayarları
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  // Seri iletişim başlat
  Serial.begin(9600);
  bluetooth.begin(9600);
  
  // RTC başlat
  rtc.halt(false);
  rtc.writeProtect(false);
  
  // EEPROM'dan programı yükle
  loadScheduleFromEEPROM();
  
  // Bluetooth'a mevcut programı gönder
  sendScheduleToBluetooth();
  
  Serial.println("Sulama Sistemi Hazir");
}

void loop() {
  // Bluetooth'dan gelen verileri kontrol et
  checkBluetooth();
  
  // Zamanı kontrol et ve röleyi yönet
  checkTimeAndControlRelay();
  
  // Küçük bir bekleme
  delay(1000);
}

void checkBluetooth() {
  if (bluetooth.available()) {
    String command = bluetooth.readStringUntil('\n');
    command.trim();
    
    if (command.startsWith("SET")) {
      // Format: SET day(0-6) startH startM endH endM enabled(0/1)
      int day = command.substring(4, 5).toInt();
      if (day >= 0 && day < SCHEDULE_SIZE) {
        int space1 = command.indexOf(' ', 5);
        int space2 = command.indexOf(' ', space1+1);
        int space3 = command.indexOf(' ', space2+1);
        int space4 = command.indexOf(' ', space3+1);
        
        weeklySchedule[day].startHour = command.substring(space1+1, space2).toInt();
        weeklySchedule[day].startMinute = command.substring(space2+1, space3).toInt();
        weeklySchedule[day].endHour = command.substring(space3+1, space4).toInt();
        weeklySchedule[day].endMinute = command.substring(space4+1).toInt();
        weeklySchedule[day].enabled = command.substring(space4+1).toInt() == 1;
        
        saveScheduleToEEPROM();
        bluetooth.println("OK");
      } else {
        bluetooth.println("ERROR: Invalid day");
      }
    } else if (command == "GET") {
      sendScheduleToBluetooth();
    } else if (command == "TIME") {
      sendCurrentTime();
    } else if (command == "ON") {
      digitalWrite(RELAY_PIN, HIGH);
      relayState = true;
      bluetooth.println("RELAY ON");
    } else if (command == "OFF") {
      digitalWrite(RELAY_PIN, LOW);
      relayState = false;
      bluetooth.println("RELAY OFF");
    } else if (command == "STATUS") {
      bluetooth.print("RELAY ");
      bluetooth.println(relayState ? "ON" : "OFF");
      sendCurrentTime();
    }
  }
}

void checkTimeAndControlRelay() {
  Time now = rtc.getTime();
  int currentDay = now.dow - 1; // DS1302 Pazar=1, biz 0-6 kullanıyoruz
  if (currentDay < 0) currentDay = 6; // Pazar günü için 6
  
  if (weeklySchedule[currentDay].enabled) {
    unsigned long currentMinutes = now.hour * 60 + now.min;
    unsigned long startMinutes = weeklySchedule[currentDay].startHour * 60 + weeklySchedule[currentDay].startMinute;
    unsigned long endMinutes = weeklySchedule[currentDay].endHour * 60 + weeklySchedule[currentDay].endMinute;
    
    if (currentMinutes >= startMinutes && currentMinutes < endMinutes) {
      digitalWrite(RELAY_PIN, HIGH);
      relayState = true;
    } else {
      digitalWrite(RELAY_PIN, LOW);
      relayState = false;
    }
  } else {
    digitalWrite(RELAY_PIN, LOW);
    relayState = false;
  }
}

void loadScheduleFromEEPROM() {
  // EEPROM'da veri olup olmadığını kontrol et
  if (EEPROM.read(EEPROM_START_ADDR) != EEPROM_MAGIC_NUMBER) {
    // Varsayılan program (her gün 08:00-08:30)
    for (int i = 0; i < SCHEDULE_SIZE; i++) {
      weeklySchedule[i].startHour = 8;
      weeklySchedule[i].startMinute = 0;
      weeklySchedule[i].endHour = 8;
      weeklySchedule[i].endMinute = 30;
      weeklySchedule[i].enabled = true;
    }
    saveScheduleToEEPROM();
  } else {
    // EEPROM'dan oku
    for (int i = 0; i < SCHEDULE_SIZE; i++) {
      int addr = EEPROM_START_ADDR + 1 + (i * sizeof(Schedule));
      EEPROM.get(addr, weeklySchedule[i]);
    }
  }
}

void saveScheduleToEEPROM() {
  EEPROM.write(EEPROM_START_ADDR, EEPROM_MAGIC_NUMBER);
  for (int i = 0; i < SCHEDULE_SIZE; i++) {
    int addr = EEPROM_START_ADDR + 1 + (i * sizeof(Schedule));
    EEPROM.put(addr, weeklySchedule[i]);
  }
}

void sendScheduleToBluetooth() {
  const char* days[] = {"Pazartesi", "Sali", "Carsamba", "Persembe", "Cuma", "Cumartesi", "Pazar"};
  
  for (int i = 0; i < SCHEDULE_SIZE; i++) {
    bluetooth.print(days[i]);
    bluetooth.print(": ");
    if (weeklySchedule[i].enabled) {
      bluetooth.print(weeklySchedule[i].startHour);
      bluetooth.print(":");
      if (weeklySchedule[i].startMinute < 10) bluetooth.print("0");
      bluetooth.print(weeklySchedule[i].startMinute);
      bluetooth.print("-");
      bluetooth.print(weeklySchedule[i].endHour);
      bluetooth.print(":");
      if (weeklySchedule[i].endMinute < 10) bluetooth.print("0");
      bluetooth.print(weeklySchedule[i].endMinute);
    } else {
      bluetooth.print("KAPALI");
    }
    bluetooth.println();
  }
}

void sendCurrentTime() {
  Time now = rtc.getTime();
  const char* days[] = {"Pazar", "Pazartesi", "Sali", "Carsamba", "Persembe", "Cuma", "Cumartesi"};
  
  bluetooth.print("Zaman: ");
  bluetooth.print(now.date);
  bluetooth.print("/");
  bluetooth.print(now.mon);
  bluetooth.print("/");
  bluetooth.print(now.year);
  bluetooth.print(" ");
  bluetooth.print(days[now.dow - 1]);
  bluetooth.print(" ");
  bluetooth.print(now.hour);
  bluetooth.print(":");
  if (now.min < 10) bluetooth.print("0");
  bluetooth.print(now.min);
  bluetooth.print(":");
  if (now.sec < 10) bluetooth.print("0");
  bluetooth.println(now.sec);
}
