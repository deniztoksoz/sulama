/* COPİLOT
arduino nano, ds1302 rtc saat modülü, hc-05 bluetooth modülü ve röle kullanarak cep telefonundan bluetooth ile bağlanarak hangi gün ve günün hangi 
saatleri arasında çalışacağını belirleyebileceğim bu bilgileri EPROM'a kaydecek şekilde çalışacak bir sulama sistemi için gerekli kodları yaz.
Ayrıca kodlar ilk çalıştığında EPROM da kayıtlı gün ve saat bilgilerini bluetooth aygıtına göndersin ki aygıttan ne zaman çalıştığını görebileyim.

Ek Açıklamalar
DS1302 Kütüphanesi: DS1302 için kullandığınız kütüphanenin API’si farklılık gösterebilir. Örneğin, bazı kütüphanelerde tarih ve saat okuma metodu farklı adlandırılmış olabilir. Bu nedenle rtc.getTime() kısmını kütüphanenize göre uyarlayın.
Bluetooth Komut Formatı: Kullanıcı telefonundan gönderilecek komutlarda gün değeri (0=Pazar, 1=Pazartesi vs.) şeklinde kodlanmıştır. İsterseniz gün isimlerini de gönderecek şekilde ek dönüşüm yapabilirsiniz.
EEPROM Yönetimi: Kod, EEPROM’un ilk baytını program sayısı olarak kullanır. Yeni program eklerken her kayıt 5 bayt (yapınızın boyutu) olarak EEPROM’da sıralı şekilde saklanır. Gerçek uygulamada ek doğrulama ve bellek doluluk kontrolleri gerekebilir.
Röle Sürücü Mantığı: Rölenizin aktif durumu modüle bağlı olarak değişebilir. Bu örnekte, HIGH sinyali röleyi aktive ettiğini varsaydık; gerekirse tersleyin.
Geliştirme İmkanları: İleride, aynı gün için birden fazla program, program silme, güncelleme veya saat biçimleri gibi ek özellikler ekleyerek sistemi daha esnek hale getirebilirsiniz.
Bu örneğin, sulama sisteminiz için sağlam bir temel oluşturacağını umuyorum. Projenizi geliştirirken karşılaşabileceğiniz diğer detaylar ya da modül yapılandırmaları hakkında konuşabilir, 
örneğin DS1302’nin doğru zaman tutması, Bluetooth’ın eşleştirme ayarları veya röle devresi tasarımları üzerine daha derin bilgiler verebilirim. Ne tarz ek özelliklerin sizi çektiğini merak ediyorum; örneğin, 
uzaktan zamanlamada mobil uygulama entegrasyonu ya da manuel müdahale için alternatif kontrol yolları gibi konuları tartışabiliriz.
*/

#include <DS1302.h>        // DS1302 RTC kütüphanesi (kendi DS1302 kütüphanenizi kullanın)
#include <SoftwareSerial.h>
#include <EEPROM.h>

// Röle kontrolü için bağlantı pini (örneğin, röle modülünüz aktive olmak için HIGH alıyor)
#define RELAY_PIN 4

// EEPROM’da saklanacak programlar için ayarlar
#define MAX_SCHEDULES 10
#define EEPROM_COUNT_ADDRESS 0         // EEPROM’un ilk baytı program adedini tutar
#define SCHEDULE_START_ADDRESS 1       // Program kayıtları bu adresten itibaren saklanır

// Sulama programının yapısını tanımlıyoruz
struct Schedule {
  uint8_t day;      // 0 = Pazar, 1 = Pazartesi, …, 6 = Cumartesi
  uint8_t sHour;    // Başlangıç saati (0-23)
  uint8_t sMinute;  // Başlangıç dakikası (0-59)
  uint8_t eHour;    // Bitiş saati (0-23)
  uint8_t eMinute;  // Bitiş dakikası (0-59)
};

// HC-05 Bluetooth modülü için yazılım tabanlı seri port (RX, TX pinleri örneğin 10 ve 11)
SoftwareSerial BTSerial(10, 11);

// DS1302 RTC için pin atamaları (CE, IO, SCLK). Bağlantılarınızı buna göre yapın.
DS1302 rtc(7, 6, 5);  

// Bluetooth üzerinden gelen komutun birikmesi için global değişken
String btCommand = "";

////////////////////////////////////////////////////////////////////////////////
// setup(): Pinler, seri haberleşme ve RTC ile EEPROM başlatılıyor.
// EEPROM’dan kayıtlı programlar okunup Bluetooth’a gönderiliyor.
////////////////////////////////////////////////////////////////////////////////
void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  Serial.begin(9600);
  BTSerial.begin(9600);  // HC-05 modülün varsayılan hızı
  
  // DS1302’yi aktif hale getiriyoruz
  rtc.halt(false);
  rtc.writeProtect(false);
  
  // EEPROM’dan program sayısını oku; eğer mantıksız ise sıfırla.
  uint8_t schedCount = EEPROM.read(EEPROM_COUNT_ADDRESS);
  if(schedCount > MAX_SCHEDULES) {
    schedCount = 0;
    EEPROM.write(EEPROM_COUNT_ADDRESS, schedCount);
  }
  
  BTSerial.println("***** Sulama Sistemi Baslatildi *****");
  if(schedCount == 0) {
    BTSerial.println("EEPROM: Kayitli program bulunamadi.");
  } else {
    BTSerial.print("EEPROM'da kayitli program sayisi: ");
    BTSerial.println(schedCount);
    for (uint8_t i = 0; i < schedCount; i++) {
      Schedule sch;
      int addr = SCHEDULE_START_ADDRESS + i * sizeof(Schedule);
      EEPROM.get(addr, sch);
      BTSerial.print("Program ");
      BTSerial.print(i+1);
      BTSerial.print(": Gun=");
      BTSerial.print(sch.day);
      BTSerial.print(", Baslangic=");
      BTSerial.print(sch.sHour);
      BTSerial.print(":");
      if(sch.sMinute < 10)
        BTSerial.print("0");
      BTSerial.print(sch.sMinute);
      BTSerial.print(", Bitis=");
      BTSerial.print(sch.eHour);
      BTSerial.print(":");
      if(sch.eMinute < 10)
        BTSerial.print("0");
      BTSerial.println(sch.eMinute);
    }
  }
  
  BTSerial.println("Bluetooth baglantisi aktif. Komut bekleniyor...");
}

////////////////////////////////////////////////////////////////////////////////
// loop(): Bluetooth'tan gelen komutları izler, DS1302’den saati okur ve
// program kaydı kontrolüne göre röleyi çalıştırır.
////////////////////////////////////////////////////////////////////////////////
void loop() {
  // Bluetooth komutlarını oku
  while (BTSerial.available()) {
    char c = BTSerial.read();
    if (c == '\n' || c == '\r') {
      if (btCommand.length() > 0) {
        processCommand(btCommand);
        btCommand = "";
      }
    } else {
      btCommand += c;
    }
  }
  
  // DS1302’den gün ve saati oku.
  // Burada DS1302 kütüphanenize bağlı olarak; örneğin "rtc.getTime()"
  // aşağıdaki şekilde bir DateTime yapısı döndürmeli:
  //
  // struct DateTime {
  //   uint8_t sec;
  //   uint8_t min;
  //   uint8_t hour;
  //   uint8_t day;
  //   uint8_t month;
  //   int year;
  //   uint8_t dow;   // 0 = Pazar, 1 = Pazartesi, …, 6 = Cumartesi
  // };
  //
  // Kendi kütüphanenize göre uyarlayın.
  DateTime now = rtc.getTime();  // Kütüphanenize göre değişebilir.
  
  // Kayıtlı programlar arasında kontrol yap: gün eşleşiyorsa, mevcut saat dakika değeri
  // programın başlangıç ve bitiş aralığında mı?
  uint8_t schedCount = EEPROM.read(EEPROM_COUNT_ADDRESS);
  bool active = false;
  for (uint8_t i = 0; i < schedCount; i++) {
    Schedule sch;
    int addr = SCHEDULE_START_ADDRESS + i * sizeof(Schedule);
    EEPROM.get(addr, sch);
    
    if (sch.day == now.dow) {  // Gün kontrolü (örneğin 1 = Pazartesi vs.)
      int currentMins = now.hour * 60 + now.min;
      int startMins = sch.sHour * 60 + sch.sMinute;
      int endMins = sch.eHour * 60 + sch.eMinute;
      
      if (currentMins >= startMins && currentMins <= endMins) {
        active = true;
        break;
      }
    }
  }
  
  // Röle kontrolü: Eğer aktif aralıksa röleyi aç, değilse kapat.
  if(active) {
    digitalWrite(RELAY_PIN, HIGH);
  } else {
    digitalWrite(RELAY_PIN, LOW);
  }
  
  delay(1000); // Her saniye kontrol
}

////////////////////////////////////////////////////////////////////////////////
// processCommand(): Bluetooth’tan gelen komutu ayrıştırır ve
// - "SET,gun,sSaat,sDakika,eSaat,eDakika" komutu ile yeni program ekler,
// - "CLEAR" ile tüm programları siler,
// - "LIST" ile mevcut programları listeler.
////////////////////////////////////////////////////////////////////////////////
void processCommand(String cmd) {
  cmd.trim();
  if (cmd.startsWith("SET")) {
    // Beklenen format: SET,gun,sSaat,sDakika,eSaat,eDakika  
    // Örnek: SET,1,8,0,9,0  (Pazartesi, 08:00 - 09:00)
    int firstComma = cmd.indexOf(',');
    if(firstComma == -1) {
      BTSerial.println("Hatali format. Ornek: SET,1,8,0,9,0");
      return;
    }
    // "SET," kısmından sonraki parametreleri al
    String params = cmd.substring(firstComma+1);
    int tokens[5];
    int tokenIndex = 0;
    int start = 0;
    for (int i = 0; i < params.length() && tokenIndex < 5; i++) {
      if(params.charAt(i) == ',') {
        String token = params.substring(start, i);
        tokens[tokenIndex] = token.toInt();
        tokenIndex++;
        start = i+1;
      }
    }
    // Son parametreyi oku
    if(tokenIndex < 5) {
      String token = params.substring(start);
      tokens[tokenIndex] = token.toInt();
      tokenIndex++;
    }
    
    if (tokenIndex != 5) {
      BTSerial.println("Hatali parametre sayisi. Ornek: SET,1,8,0,9,0");
      return;
    }
    
    Schedule newSch;
    newSch.day     = tokens[0];
    newSch.sHour   = tokens[1];
    newSch.sMinute = tokens[2];
    newSch.eHour   = tokens[3];
    newSch.eMinute = tokens[4];
    
    // EEPROM’da ekleme yapmak için mevcut program sayısını oku
    uint8_t schedCount = EEPROM.read(EEPROM_COUNT_ADDRESS);
    if (schedCount >= MAX_SCHEDULES) {
      BTSerial.println("Program kapasitesi dolu.");
      return;
    }
    int addr = SCHEDULE_START_ADDRESS + schedCount * sizeof(Schedule);
    EEPROM.put(addr, newSch);
    schedCount++;
    EEPROM.write(EEPROM_COUNT_ADDRESS, schedCount);
    
    BTSerial.println("Yeni program kaydedildi:");
    BTSerial.print("Gun=");
    BTSerial.print(newSch.day);
    BTSerial.print(", Baslangic=");
    BTSerial.print(newSch.sHour);
    BTSerial.print(":");
    if(newSch.sMinute < 10)
      BTSerial.print("0");
    BTSerial.print(newSch.sMinute);
    BTSerial.print(", Bitis=");
    BTSerial.print(newSch.eHour);
    BTSerial.print(":");
    if(newSch.eMinute < 10)
      BTSerial.print("0");
    BTSerial.println(newSch.eMinute);
  
  } else if (cmd.startsWith("CLEAR")) {
    // Tüm programları sil: program sayısını 0 yapıyoruz.
    EEPROM.write(EEPROM_COUNT_ADDRESS, 0);
    BTSerial.println("Tum programlar silindi.");
  } else if (cmd.startsWith("LIST")) {
    // Mevcut program listesini gönder.
    uint8_t schedCount = EEPROM.read(EEPROM_COUNT_ADDRESS);
    BTSerial.print("Kayitli program sayisi: ");
    BTSerial.println(schedCount);
    for (uint8_t i = 0; i < schedCount; i++) {
      Schedule sch;
      int addr = SCHEDULE_START_ADDRESS + i * sizeof(Schedule);
      EEPROM.get(addr, sch);
      BTSerial.print("Program ");
      BTSerial.print(i+1);
      BTSerial.print(": Gun=");
      BTSerial.print(sch.day);
      BTSerial.print(", Baslangic=");
      BTSerial.print(sch.sHour);
      BTSerial.print(":");
      if(sch.sMinute < 10)
        BTSerial.print("0");
      BTSerial.print(sch.sMinute);
      BTSerial.print(", Bitis=");
      BTSerial.print(sch.eHour);
      BTSerial.print(":");
      if(sch.eMinute < 10)
        BTSerial.print("0");
      BTSerial.println(sch.eMinute);
    }
  } else {
    BTSerial.println("Bilinmeyen komut. Gecerli komutlar: SET, CLEAR, LIST");
  }
}
