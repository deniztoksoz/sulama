/*
Bağlantılar:
1. DS1302 RTC Saat Modülü Bağlantıları:
VCC → Arduino 5V

GND → Arduino GND

SCL → Arduino D4 (GPIO)

SDA → Arduino D5 (GPIO)

2. HC-05 Bluetooth Modülü Bağlantıları:
VCC → Arduino 5V

GND → Arduino GND

TX → Arduino RX (Pin 0)

RX → Arduino TX (Pin 1) (Burada dikkat etmeniz gereken nokta: Arduino'nun USB bağlantısını kullanarak yükleme yaparken bu pinleri geçici olarak bağlamamalısınız.)

3. Röle Bağlantıları:
VCC → Arduino 5V

GND → Arduino GND

IN → Arduino D2 (Röleyi kontrol etmek için)

*/

#include <Wire.h>
#include <RTClib.h>  // RTC modülünü kullanmak için gerekli kütüphane

RTC_DS1302 rtc;  // DS1302 RTC modülünü başlatıyoruz
int relayPin = 2;  // Röle çıkışı için pin
String command = "";  // Bluetooth'tan alınan komut

// Günü ve saati kontrol etmek için değişkenler
int setDay = -1;  // Ayarlanacak gün (-1: gün seçilmedi)
int setHour = -1; // Ayarlanacak saat (-1: saat seçilmedi)

void setup() {
  Serial.begin(9600);  // Seri iletişim başlatıyoruz
  rtc.begin();  // RTC modülünü başlatıyoruz
  
  pinMode(relayPin, OUTPUT);  // Röleyi çıkış olarak ayarlıyoruz

  // RTC'nin doğru çalışıp çalışmadığını kontrol ediyoruz
  if (!rtc.isrunning()) {
    Serial.println("RTC modülü çalışmıyor.");
    while (1);
  }
  
  Serial.println("Bluetooth ile bağlantı bekleniyor...");
}

void loop() {
  // Bluetooth'tan gelen veriyi kontrol ediyoruz
  if (Serial.available() > 0) {
    char receivedChar = Serial.read();  // Bluetooth'tan bir karakter alıyoruz
    command += receivedChar;  // Komutu birleştiriyoruz

    // Komutun sonuna geldiğinde işleme başlıyoruz
    if (receivedChar == '\n') {
      processCommand(command);  // Komutu işliyoruz
      command = "";  // Komutu sıfırlıyoruz
    }
  }

  // Gerçek zamanlı saat verilerini alıyoruz
  DateTime now = rtc.now();

  // Eğer kullanıcı bir gün ve saat ayarlamışsa, kontrol ediyoruz
  if (setDay != -1 && setHour != -1) {
    // Şu anki gün ve saati kontrol ediyoruz
    if (now.dayOfTheWeek() == setDay && now.hour() == setHour) {
      digitalWrite(relayPin, HIGH);  // Röleyi aç (sulama başlasın)
      Serial.println("Sulama başladı.");
    } else {
      digitalWrite(relayPin, LOW);  // Röleyi kapat (sulama durdu)
      Serial.println("Sulama durdu.");
    }
  }

  delay(1000);  // 1 saniye bekleyelim
}

void processCommand(String cmd) {
  // Komutu analiz edip, gün ve saati ayarlıyoruz
  cmd.trim();  // Gereksiz boşlukları temizliyoruz
  Serial.println("Alınan Komut: " + cmd);
  
  // Komut formatı: "SET [Gün] [Saat]"
  // Örnek: "SET 2 9" → Salı günü saat 9'da sulama başlasın
  if (cmd.startsWith("SET")) {
    int space1 = cmd.indexOf(' ');
    int space2 = cmd.indexOf(' ', space1 + 1);

    setDay = cmd.substring(space1 + 1, space2).toInt();  // Gün
    setHour = cmd.substring(space2 + 1).toInt();  // Saat

    // Gün ve saat bilgisi ayarlandığında, kullanıcının onayını veriyoruz
    Serial.print("Sulama şu gün ve saatte başlatılacak: Gün - ");
    Serial.print(setDay);
    Serial.print(", Saat - ");
    Serial.println(setHour);
  } else {
    Serial.println("Geçersiz komut!");
  }
}
/*
Cep Telefonu Uygulaması:
Cep telefonundan Bluetooth Terminal uygulaması ile bu komutları gönderebilirsiniz. Komut formatı şu şekilde olmalıdır:

SET [Gün] [Saat]
Örnek:
SET 2 9
Bu komut, Salı günü saat 09:00'da sulamanın başlamasını sağlar.

*/
