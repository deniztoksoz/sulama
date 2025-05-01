#include <Wire.h>
#include <RTClib.h>  // RTC modülünü kullanmak için gerekli kütüphane

RTC_DS3231 rtc;  // D1302 RTC modülünü başlatıyoruz
int relayPin = 2;  // Röleyi Arduino'nun dijital 2 numaralı pinine bağlıyoruz

void setup() {
  Serial.begin(9600);
  Wire.begin();
  
  if (!rtc.begin()) {
    Serial.println("RTC modülü bulunamadı");
    while (1);
  }
  
  pinMode(relayPin, OUTPUT);  // Röleyi çıkış olarak ayarlıyoruz
  
  // RTC'nin doğru zaman ayarına sahip olup olmadığını kontrol ediyoruz
  if (rtc.isrunning()) {
    Serial.println("RTC modülü çalışıyor");
  } else {
    Serial.println("RTC modülü duraklamış, doğru saat ayarlamanız gerekebilir.");
  }
}

void loop() {
  DateTime now = rtc.now();  // Şu anki tarih ve saat bilgisini alıyoruz

  // Günü, saati alıyoruz
  int currentDay = now.dayOfTheWeek();  // 0: Pazar, 1: Pazartesi, ..., 6: Cumartesi
  int currentHour = now.hour();  // Saat bilgisini alıyoruz

  // Haftada 2 gün çalışacak (örneğin Salı ve Cuma), 09:00 - 14:00 arasında sulama çalışacak
  if ((currentDay == 2 || currentDay == 5) && currentHour >= 9 && currentHour < 14) {
    digitalWrite(relayPin, HIGH);  // Röleyi aktif et (sulama başlasın)
    Serial.println("Sulama sistemi aktif");
  } else {
    digitalWrite(relayPin, LOW);  // Röleyi pasif et (sulama dursun)
    Serial.println("Sulama sistemi durduruldu");
  }

  delay(60000);  // Her dakika bir kontrol yapıyoruz
}
