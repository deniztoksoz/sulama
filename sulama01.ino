#include <Arduino.h>
//Bt gelen veri için değişkenler....
String gelen; 
char harf; 

int buttonPin = 2; // Buton bağlı olan pin
int relayPin = 3; // Röle bağlı olan pin
bool buttonState = false; // Buton durumu


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  pinMode(buttonPin, INPUT_PULLUP); // Buton pini giriş olarak ayarlanır
  pinMode(relayPin, OUTPUT); // Röle pini çıkış olarak ayarlanır


}

void loop() {
  // put your main code here, to run repeatedly:
    bt_veri_oku();
    Bt_acKapa();

}
void bt_veri_oku(){ //
        while(Serial.available()){
          //bt ile gelen veri okunarak stringe dönüşüyor
          harf = Serial.read();
          gelen.concat(harf);
              if(harf == '#'){ // if end of message received
                gelen = gelen.substring(0,gelen.indexOf('#'));
                islem_sec(gelen);
                Serial.print(gelen); 
                gelen = ""; 
                Serial.println();
              }
        }
}

void Bt_acKapa(){
  if (digitalRead(buttonPin) == LOW) { // Buton basıldığında
    delay(50); // Titreşim önleme için gecikme
    if (digitalRead(buttonPin) == LOW) { // Buton hala basılıysa
      buttonState = !buttonState; // Buton durumu değiştirilir
      digitalWrite(relayPin, buttonState); // Röle durumu değiştirilir
      while (digitalRead(buttonPin) == LOW); // Buton bırakılana kadar bekle
    }
  }

}

void islem_sec(String veri){


}


