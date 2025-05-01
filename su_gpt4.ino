/* GEMİNİ
Açıklamalar:
Kütüphaneler: Gerekli kütüphaneler dahil edildi.
Pin Tanımları: RTC, röle ve Bluetooth modülünün Arduino üzerindeki bağlantı pinleri tanımlandı. SoftwareSerial kütüphanesi kullanılarak Bluetooth için yazılımsal seri port oluşturuldu (RX ve TX pinlerini Arduino'nun dijital pinlerine bağlamanız gerekecek).
EEPROM Adresleri: EEPROM'da hangi bilgilerin nereye kaydedileceği tanımlandı. Bu, bilgilerin düzenli bir şekilde saklanmasını sağlar.
Çalışma Günleri (workingDays): Hangi günlerde sulama yapılacağını tutan bir byte değişkeni. Her bit bir günü temsil eder (Pazar en sağdaki bit).
Zaman Dilimleri (timeSlots): Sulamanın başlayacağı ve biteceği saat ve dakikaları tutan bir yapı dizisi. Maksimum 5 farklı zaman dilimi tanımlanabilir.
timeSlotCount: Kaydedilmiş zaman dilimi sayısını tutar.
RTC Nesnesi: RTC_DS1307 sınıfından bir nesne oluşturuldu.
setup() Fonksiyonu:
Seri port ve Bluetooth seri port başlatıldı.
RTC başlatıldı.
Röle pini çıkış olarak ayarlandı ve başlangıçta kapatıldı.
loadSettingsFromEEPROM() fonksiyonu çağrılarak EEPROM'daki ayarlar yüklendi.
sendSettingsViaBluetooth() fonksiyonu çağrılarak mevcut ayarlar Bluetooth üzerinden gönderildi.
loop() Fonksiyonu:
RTC'den güncel saat bilgisi alındı.
checkWateringSchedule() fonksiyonu çağrılarak sulama zamanı kontrol edildi.
receiveDataFromBluetooth() fonksiyonu çağrılarak Bluetooth'tan gelen veri kontrol edildi.
1 saniye gecikme eklendi.
loadSettingsFromEEPROM() Fonksiyonu: EEPROM'dan kayıtlı çalışma günleri ve zaman dilimi bilgileri okunur.
saveSettingsToEEPROM() Fonksiyonu: Çalışma günleri ve zaman dilimi bilgileri EEPROM'a kaydedilir.
sendSettingsViaBluetooth() Fonksiyonu: Mevcut çalışma günleri ve zaman dilimi bilgileri Bluetooth üzerinden cep telefonuna gönderilir.
receiveDataFromBluetooth() Fonksiyonu: Bluetooth seri portundan veri gelirse okur ve parseBluetoothData() fonksiyonuna gönderir.
parseBluetoothData() Fonksiyonu: Bluetooth'tan gelen komutları ayrıştırır:
GUNLER=D1D2D3D4D5D6D7: Çalışma günlerini ayarlar. Örneğin, GUNLER=1010010 Pazartesi, Çarşamba ve Cumartesi günleri anlamına gelir (Pazar=0, Pazartesi=1, ..., Cumartesi=6).
EKLE=G,SS:DD,SS:DD: Yeni bir zaman dilimi ekler. Örneğin, EKLE=1,08:00,08:15 Pazartesi günü 08:00 - 08:15 arası sulama yapar
*/


#include <RTClib.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

// RTC modülü bağlantı pinleri
#define RTC_RST_PIN 8
#define RTC_DAT_PIN 9
#define RTC_CLK_PIN 10

// Röle kontrol pini
#define RELAY_PIN 7

// Bluetooth modülü bağlantı pinleri (Örnek olarak SoftwareSerial kullanılıyor)
#define RX_PIN 2
#define TX_PIN 3
SoftwareSerial bluetoothSerial(RX_PIN, TX_PIN); // RX, TX

// EEPROM adres tanımları
#define EEPROM_START_ADDRESS 0
#define DAYS_OFFSET 0
#define TIME_SLOT_COUNT_OFFSET 7
#define TIME_SLOT_START_OFFSET 8
#define TIME_SLOT_SIZE 5 // Hafta günü (1 byte), Başlangıç Saati (1 byte), Başlangıç Dakika (1 byte), Bitiş Saati (1 byte), Bitiş Dakika (1 byte)

// Çalışma günleri (7 bit, her bit bir günü temsil eder. Pazar=0, Pazartesi=1, ..., Cumartesi=6)
byte workingDays = 0b0000000;

// Çalışma zaman dilimleri (maksimum 5 zaman dilimi)
struct TimeSlot {
  byte dayOfWeek; // 0-6 (Pazar-Cumartesi)
  byte startHour;
  byte startMinute;
  byte endHour;
  byte endMinute;
};

TimeSlot timeSlots[5];
byte timeSlotCount = 0;

RTC_DS1307 rtc;

void setup() {
  Serial.begin(9600);
  bluetoothSerial.begin(9600);
  Wire.begin();
  rtc.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Başlangıçta röleyi kapalı tut

  loadSettingsFromEEPROM();
  sendSettingsViaBluetooth();
}

void loop() {
  DateTime now = rtc.now();
  checkWateringSchedule(now);
  receiveDataFromBluetooth();
  delay(1000); // Her saniyede bir kontrol et
}

void loadSettingsFromEEPROM() {
  workingDays = EEPROM.read(DAYS_OFFSET);
  timeSlotCount = EEPROM.read(TIME_SLOT_COUNT_OFFSET);
  if (timeSlotCount > 5) {
    timeSlotCount = 0; // Güvenlik kontrolü
  }
  for (int i = 0; i < timeSlotCount; i++) {
    int address = TIME_SLOT_START_OFFSET + i * TIME_SLOT_SIZE;
    timeSlots[i].dayOfWeek = EEPROM.read(address);
    timeSlots[i].startHour = EEPROM.read(address + 1);
    timeSlots[i].startMinute = EEPROM.read(address + 2);
    timeSlots[i].endHour = EEPROM.read(address + 3);
    timeSlots[i].endMinute = EEPROM.read(address + 4);
  }
  Serial.println("EEPROM'dan ayarlar yüklendi.");
}

void saveSettingsToEEPROM() {
  EEPROM.write(DAYS_OFFSET, workingDays);
  EEPROM.write(TIME_SLOT_COUNT_OFFSET, timeSlotCount);
  for (int i = 0; i < timeSlotCount; i++) {
    int address = TIME_SLOT_START_OFFSET + i * TIME_SLOT_SIZE;
    EEPROM.write(address, timeSlots[i].dayOfWeek);
    EEPROM.write(address + 1, timeSlots[i].startHour);
    EEPROM.write(address + 2, timeSlots[i].startMinute);
    EEPROM.write(address + 3, timeSlots[i].endHour);
    EEPROM.write(address + 4, timeSlots[i].endMinute);
  }
  Serial.println("Ayarlar EEPROM'a kaydedildi.");
}

void sendSettingsViaBluetooth() {
  bluetoothSerial.print("Çalışma Günleri (Pz-Cmt): ");
  for (int i = 0; i < 7; i++) {
    if (bitRead(workingDays, i)) {
      switch (i) {
        case 0: bluetoothSerial.print("Pz "); break;
        case 1: bluetoothSerial.print("Pt "); break;
        case 2: bluetoothSerial.print("Sa "); break;
        case 3: bluetoothSerial.print("Ça "); break;
        case 4: bluetoothSerial.print("Pe "); break;
        case 5: bluetoothSerial.print("Cu "); break;
        case 6: bluetoothSerial.print("Ct "); break;
      }
    }
  }
  bluetoothSerial.println();

  bluetoothSerial.print("Kaydedilmiş Zaman Dilimleri: ");
  bluetoothSerial.println(timeSlotCount);
  for (int i = 0; i < timeSlotCount; i++) {
    bluetoothSerial.print(i + 1);
    bluetoothSerial.print(". Gün: ");
    switch (timeSlots[i].dayOfWeek) {
      case 0: bluetoothSerial.print("Pazar"); break;
      case 1: bluetoothSerial.print("Pazartesi"); break;
      case 2: bluetoothSerial.print("Salı"); break;
      case 3: bluetoothSerial.print("Çarşamba"); break;
      case 4: bluetoothSerial.print("Perşembe"); break;
      case 5: bluetoothSerial.print("Cuma"); break;
      case 6: bluetoothSerial.print("Cumartesi"); break;
    }
    bluetoothSerial.print(", Başlangıç: ");
    bluetoothSerial.print(timeSlots[i].startHour);
    bluetoothSerial.print(":");
    if (timeSlots[i].startMinute < 10) bluetoothSerial.print("0");
    bluetoothSerial.print(timeSlots[i].startMinute);
    bluetoothSerial.print(", Bitiş: ");
    bluetoothSerial.print(timeSlots[i].endHour);
    bluetoothSerial.print(":");
    if (timeSlots[i].endMinute < 10) bluetoothSerial.print("0");
    bluetoothSerial.println(timeSlots[i].endMinute);
  }
  bluetoothSerial.println("---");
}

void receiveDataFromBluetooth() {
  if (bluetoothSerial.available()) {
    String data = bluetoothSerial.readStringUntil('\n');
    data.trim();
    Serial.print("Bluetooth'tan gelen veri: ");
    Serial.println(data);
    parseBluetoothData(data);
  }
}

void parseBluetoothData(String data) {
  if (data.startsWith("GUNLER=")) {
    String daysStr = data.substring(7);
    workingDays = 0;
    if (daysStr.indexOf('1') != -1) bitSet(workingDays, 1); // Pazartesi
    if (daysStr.indexOf('2') != -1) bitSet(workingDays, 2); // Salı
    if (daysStr.indexOf('3') != -1) bitSet(workingDays, 3); // Çarşamba
    if (daysStr.indexOf('4') != -1) bitSet(workingDays, 4); // Perşembe
    if (daysStr.indexOf('5') != -1) bitSet(workingDays, 5); // Cuma
    if (daysStr.indexOf('6') != -1) bitSet(workingDays, 6); // Cumartesi
    if (daysStr.indexOf('0') != -1) bitSet(workingDays, 0); // Pazar
    saveSettingsToEEPROM();
    sendSettingsViaBluetooth();
  } else if (data.startsWith("EKLE=")) { // EKLE=G,SS:DD,SS:DD (G: Gün(0-6), SS:DD Başlangıç Saati:Dakika, SS:DD Bitiş Saati:Dakika)
    if (timeSlotCount < 5) {
      String timeSlotStr = data.substring(5);
      int comma1 = timeSlotStr.indexOf(',');
      int comma2 = timeSlotStr.indexOf(',', comma1 + 1);
      int colon1 = timeSlotStr.indexOf(':');
      int colon2 = timeSlotStr.indexOf(':', colon1 + 1);

      if (comma1 != -1 && comma2 != -1 && colon1 != -1 && colon2 != -1) {
        timeSlots[timeSlotCount].dayOfWeek = timeSlotStr.substring(0, comma1).toInt();
        timeSlots[timeSlotCount].startHour = timeSlotStr.substring(comma1 + 1, colon1).toInt();
        timeSlots[timeSlotCount].startMinute = timeSlotStr.substring(colon1 + 1, comma2).toInt();
        timeSlots[timeSlotCount].endHour = timeSlotStr.substring(comma2 + 1, colon2).toInt();
        timeSlots[timeSlotCount].endMinute = timeSlotStr.substring(colon2 + 1).toInt();

        if (timeSlots[timeSlotCount].dayOfWeek >= 0 && timeSlots[timeSlotCount].dayOfWeek <= 6 &&
            timeSlots[timeSlotCount].startHour >= 0 && timeSlots[timeSlotCount].startHour <= 23 &&
            timeSlots[timeSlotCount].startMinute >= 0 && timeSlots[timeSlotCount].startMinute <= 59 &&
            timeSlots[timeSlotCount].endHour >= 0 && timeSlots[timeSlotCount].endHour <= 23 &&
            timeSlots[timeSlotCount].endMinute >= 0 && timeSlots[timeSlotCount].endMinute <= 59) {
          timeSlotCount++;
          saveSettingsToEEPROM();
          sendSettingsViaBluetooth();
          Serial.println("Yeni zaman dilimi eklendi.");
        } else {
          bluetoothSerial.println("Hatalı zaman dilimi formatı veya değerleri.");
        }
      } else {
        bluetoothSerial.println("Hatalı zaman dilimi formatı.");
      }
    } else {
      bluetoothSerial.println("Maksimum zaman dilimi sayısına ulaşıldı.");
    }
  } else if (data.startsWith("SIL=")) { // SIL= sıra numarası (1'den başlar)
    int indexToDelete = data.substring(4).toInt() - 1;
    if (indexToDelete >= 0 && indexToDelete < timeSlotCount) {
      for (int i = indexToDelete; i < timeSlotCount - 1; i++) {
        timeSlots[i] = timeSlots[i + 1];
      }
      timeSlotCount--;
      saveSettingsToEEPROM();
      sendSettingsViaBluetooth();
      Serial.println("Zaman dilimi silindi.");
    } else {
      bluetoothSerial.println("Geçersiz sıra numarası.");
    }
  } else if (data.startsWith("TEMIZLE")) {
    workingDays = 0;
    timeSlotCount = 0;
    saveSettingsToEEPROM();
    sendSettingsViaBluetooth();
    Serial.println("Tüm ayarlar temizlendi.");
  } else if (data.startsWith("SAAT=")) { // SAAT=YY/AA/GG SS:DD:SS formatında RTC'yi ayarlamak için
    // Bu kısım isteğe bağlıdır, RTC'yi Bluetooth üzerinden ayarlamak isterseniz ekleyebilirsiniz.
    // Ancak RTC'nin doğru ayarlandığından emin olmak için ilk kurulumda seri port üzerinden ayarlamanız önerilir.
  } else {
    bluetoothSerial.println("Geçersiz komut.");
  }
}

void checkWateringSchedule(DateTime now) {
  if (bitRead(workingDays, now.dayOfTheWeek() - 1)) { // RTC haftanın gününü 1-7 (Pazar-Cumartesi) döndürür. Biz 0-6 kullandık.
    for (int i = 0; i < timeSlotCount; i++) {
      if (timeSlots[i].dayOfWeek == (now.dayOfTheWeek() - 1) &&
          now.hour() >= timeSlots[i].startHour && now.minute() >= timeSlots[i].startMinute &&
          now.hour() <= timeSlots[i].endHour && now.minute() <= timeSlots[i].endMinute) {
        // Başlangıç ve bitiş saatleri aynı olabilir, bu durumda sadece o dakikada çalışır.
        if (now.hour() > timeSlots[i].startHour || (now.hour() == timeSlots[i].startHour && now.minute() >= timeSlots[i].startMinute)) {
          if (now.hour() < timeSlots[i].endHour || (now.hour() == timeSlots[i].endHour && now.minute() <= timeSlots[i].endMinute)) {
            digitalWrite(RELAY_PIN, HIGH); // Röleyi aç (sulama başla)
            return; // Bir zaman dilimi eşleşti, diğerlerini kontrol etmeye gerek yok
          }
        }
      }
    }
  }
  digitalWrite(RELAY_PIN, LOW); // Hiçbir zaman dilimi eşleşmediyse röleyi kapat
}
