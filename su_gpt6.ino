/* TALK AI
Gerekli Bağlantılar:
DS1302 RTC:
SCLK → Digital 4
RST → Digital 5
I/O → Digital 6
HC-05 Bluetooth:
RX → Digital 10
TX → Digital 11
Röle:
Rölenin giriş pini → Digital 7 (veya uygun bir pin)
EEPROM:
Arduino'nun dahili EEPROM’u kullanılacaktır
Kullanım:
Arduino’yu bağlayın ve kodu yükleyin.
Bluetooth cihazını (cep telefonu veya bilgisayar) Arduino’ya bağlayın.
İlk güç verildiğinde, Arduino EEPROM’dan sulama zamanını yükler ve Bluetooth ile gönderir.
Cep telefonunda bir terminal veya uygulama kullanarak "SET" komutlarıyla sulama zamanlarını ve günlerini ayarlayabilirsiniz. Örneğin:
 
SET 6 0 8 0 62
Burada:

6 0 → 06:00
8 0 → 08:00
62 → 0b0111110 (Pzt-Cuma)
*/





#include <EEPROM.h>
#include <DS1302.h>
#include <SoftwareSerial.h>

// RTC pinleri
#define SCLK_PIN 4
#define RST_PIN 5
#define I/O_PIN 6

// Röle pini
#define RELAY_PIN 7

// Bluetooth seri port
SoftwareSerial bluetoothSerial(10, 11); // RX, TX

DS1302 rtc(SCLK_PIN, RST_PIN, I/O_PIN);

// EEPROM adresleri
#define EEPROM_START_ADDR 0
// EEPROM'da saklanacak veriler:
struct Schedule {
  byte startHour; // Başlangıç saati (0-23)
  byte startMinute; // Başlangıç dakikası
  byte endHour;   // Bitiş saati
  byte endMinute; // Bitiş dakikası
  byte dayMask;   // Günler: bit mask (örneğin, 0b01111110 = Pazartesi-Cuma)
} schedule;

// Fonksiyonlar
void loadScheduleFromEEPROM();
void saveScheduleToEEPROM();
void sendScheduleOverBluetooth();
bool isTimeInSchedule(DateTime now);
byte getDayBitMask();

void setup() {
  Serial.begin(9600);
  bluetoothSerial.begin(9600);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Başlangıçta röle kapalı

  rtc.begin();

  // EEPROM'dan planı yükle
  loadScheduleFromEEPROM();

  // İlk çalıştırmada EEPROM'daki verileri Bluetooth ile gönder
  sendScheduleOverBluetooth();

  // Eğer EEPROM'da kayıt yoksa, varsayılan ayarları yap
  // (örneğin, hiç ayar yapılmamışsa)
  // Burada bir kontrol yapılabilir; basitçe her zaman gönderiyoruz
}

void loop() {
  DateTime now = rtc.now();

  // Sulama kontrolü
  if (isTimeInSchedule(now)) {
    digitalWrite(RELAY_PIN, HIGH); // Sulamayı başlat
  } else {
    digitalWrite(RELAY_PIN, LOW); // Sulamayı durdur
  }

  // Bluetooth bağlantısı ve komutlar
  if (bluetoothSerial.available()) {
    String command = bluetoothSerial.readStringUntil('\n');
    command.trim();
    handleBluetoothCommand(command);
  }

  delay(1000); // 1 saniye bekle
}

// Fonksiyonlar

void loadScheduleFromEEPROM() {
  EEPROM.get(EEPROM_START_ADDR, schedule);
  // Kontrol: Eğer başlangıç saati veya bitiş saati 255 ise, muhtemelen boş
  if (schedule.startHour == 255 || schedule.endHour == 255) {
    // Varsayılan ayarları yap
    schedule.startHour = 6;
    schedule.startMinute = 0;
    schedule.endHour = 8;
    schedule.endMinute = 0;
    schedule.dayMask = 0b01111110; // Pazartesi-Cuma
    saveScheduleToEEPROM();
  }
}

void saveScheduleToEEPROM() {
  EEPROM.put(EEPROM_START_ADDR, schedule);
}

void sendScheduleOverBluetooth() {
  String info = "Sulama Programi:\n";
  info += "Günler (Pzt=1, Sal=2,...): " + String(schedule.dayMask, BIN) + "\n";
  info += "Baslangic: " + String(schedule.startHour) + ":" + String(schedule.startMinute) + "\n";
  info += "Bitis: " + String(schedule.endHour) + ":" + String(schedule.endMinute) + "\n";

  bluetoothSerial.println(info);
}

bool isTimeInSchedule(DateTime now) {
  // Gün kontrolü
  byte todayBit = getDayBitMask();
  if ((schedule.dayMask & todayBit) == 0) {
    return false; // Bugün programlama yok
  }

  // Saat ve dakika kontrolü
  int currentMinutes = now.hour() * 60 + now.minute();
  int startMinutes = schedule.startHour * 60 + schedule.startMinute;
  int endMinutes = schedule.endHour * 60 + schedule.endMinute;

  if (startMinutes <= currentMinutes && currentMinutes <= endMinutes) {
    return true;
  }
  return false;
}

byte getDayBitMask() {
  // DS1302 ile alınan gün bilgisi
  // 0 = Pazar, 1 = Pazartesi, ..., 6 = Cumartesi
  byte dayOfWeek = rtc.getDay(); // 0=Pazar, 1=Pzt, ... 6=Cum
  // Günleri bit mask'e göre ayarla
  // Örnek: Pazartesi=bit 1, Sal=bit 2, ... Cuma=bit 5
  // Günleri uygun şekilde ayarlayın:
  switch (dayOfWeek) {
    case 0: return 0b00000001; // Pazar
    case 1: return 0b00000010; // Pzt
    case 2: return 0b00000100; // Sal
    case 3: return 0b00001000; // Çar
    case 4: return 0b00010000; // Per
    case 5: return 0b00100000; // Cum
    case 6: return 0b01000000; // Cmt
    default: return 0;
  }
}

void handleBluetoothCommand(String cmd) {
  // Komut örneği: SET 1 6 30 8 0 1
  // Bu örnek, günleri ve saatleri ayarlama komutudur
  // Format:
  // SET startHour startMinute endHour endMinute dayMask
  if (cmd.startsWith("SET")) {
    int vals[6];
    int idx = 0;
    int lastIdx = 0;
    for (int i=0; i<cmd.length(); i++) {
      if (cmd.charAt(i) == ' ' || i == cmd.length()-1) {
        String part = cmd.substring(lastIdx, i);
        lastIdx = i+1;
        if (part.length() > 0) {
          vals[idx++] = part.toInt();
        }
      }
    }
    if (idx == 5) {
      schedule.startHour = vals[0];
      schedule.startMinute = vals[1];
      schedule.endHour = vals[2];
      schedule.endMinute = vals[3];
      schedule.dayMask = vals[4];
      saveScheduleToEEPROM();
      bluetoothSerial.println("Program kaydedildi.");
    } else {
      bluetoothSerial.println("Gecersiz komut.");
    }
  } else {
    bluetoothSerial.println("Komut taninmadi.");
  }
}
