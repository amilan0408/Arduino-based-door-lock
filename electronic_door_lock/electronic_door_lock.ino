#include <EEPROM.h>
#include <Keypad.h>
#include <SPI.h>        // RC522 Module uses SPI protocol
#include <MFRC522.h>  // Library for Mifare RC522 Devices

#define SS_PIN 10 // Set data and reset pin for rfid reader
#define RST_PIN 9

#define LED_ON HIGH
#define LED_OFF LOW
#define greenLed A3    // Led indicator pins
#define redLed A4

#define button 2 //indoor button
#define wipe_button 3 //to erase all data in EEPROM -> must be pressed during system boot
#define relay 4 //

#define KEYPAD_TIMEOUT 8000 //maximum delay between two characters

char keys[4][3] = {   
  {'1','2','3'},  
  {'4','5','6'},
  {'7','8','9'},  
  {'*','0','#'}  
};

byte rows[4] = {8,7,6,5}; 
byte columns[3] = {A0,A1,A2};  

Keypad input = Keypad( makeKeymap(keys), rows, columns,4, 3 );
MFRC522 rfid(SS_PIN, RST_PIN); 

char storedPassword[5]={'1','2','3','4','5'};
char password[5];   

void setup() { 
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(button, INPUT_PULLUP);
  pinMode(wipe_button, INPUT_PULLUP);
  pinMode(relay, OUTPUT);
}

void loop() {
  uint8_t counter=0;  //count number of entered characters
  unsigned long keypad_timer; //used for keypad tiping timeout
  unsigned long blinking_timer; //used for blink effect

  NormalState();
  
  while(true)
  {
    //--------- Keypad check ---------
    char c=input.getKey();
    if(c!= NO_KEY)
    {
      if(counter==0)  blinking_timer=millis(); //first character-> blink greenLed
      keypad_timer=millis(); //start timer
      digitalWrite(greenLed,LED_ON);
      delay(15);
      digitalWrite(greenLed,LED_OFF);
      password[counter]=c;
      counter++;
      
      if(counter>=5)  //password entered
      {
        if(Compare(password,storedPassword,5)) AccessGranted();
        else AccessDenied();
        break;
      }
     }

    if(counter!=0)  //if keypad is waiting for next character, blink greenLed
    {  
       if(millis()-blinking_timer >= 250){
        blinking_timer=millis();
        digitalWrite(greenLed,!digitalRead(greenLed)); //change the state of green led
       }
       if(millis()-keypad_timer >= KEYPAD_TIMEOUT) break;    
    }
    
    //-------- Button check ---------
    if(digitalRead(button)==LOW) 
    {
      AccessGranted();
      break;
    }
     delay(10);
  }
  delay(150);
}


/*--------------------------- Compare arrays ----------------------------------------------------*/
template <class T> T Compare (T a[], T b[], int k) {
  for ( uint8_t i = 0; i < k; i++ ) {   // Loop k times
    if ( a[i] != b[i] ) {     // IF a != b then false, because: one fails, all fail
       return false;
    }
  }
  return true;  
}

/*-------------------------- Access Granted ------------------------------------------------*/
void AccessGranted(){
  
  digitalWrite(greenLed,LED_ON);
  digitalWrite(redLed,LED_OFF);
  digitalWrite(relay,HIGH);
  Serial.println("\nAccess Granted");
  delay(3000); 
  NormalState();
}

/*-------------------------- Access Denied ------------------------------------------------*/
void AccessDenied(){
  Serial.println("\nAccess Denied");
  digitalWrite(greenLed,LED_OFF);
  digitalWrite(relay,LOW);
  for(uint8_t i=0; i<5;i++)
  {
      digitalWrite(redLed,LED_ON);
      delay(100);
      digitalWrite(redLed,LED_OFF);
      delay(100);
  }
  NormalState();
}
/*------------------------- Normal state ---------------------------------------------------*/
void NormalState(){
  digitalWrite(greenLed,LED_OFF);
  digitalWrite(redLed,LED_ON);
  digitalWrite(relay,LOW);
}
