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

/*------------------------ EEPROM --------------------------*/
#define PSW_FLAG_ADDR 1 // Password flag at address 1
#define PSW_DATA_ADDR 6 // Password data at addresses 6 to 11

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

char storedPassword[5];
char password[5];   

void setup() { 
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(button, INPUT_PULLUP);
  pinMode(wipe_button, INPUT_PULLUP);
  pinMode(relay, OUTPUT);

  CheckWipeButton();

    /*----------------------- Checking and defining Password in EEPROM ---------------------------------*/
  // Check if password is defined, if not let user choose it
  // If password is defined, EEPROM holds number 143 at address 1
  if (EEPROM.read(PSW_FLAG_ADDR) != 143) { //if there is no defined password
    digitalWrite(redLed,LED_ON);  //turn on leds to indicate programming mode
    digitalWrite(greenLed,LED_ON);
    
    for(uint8_t j=0;j<5;j++){
      char c=input.waitForKey();
      digitalWrite(greenLed,LED_OFF);  //turn led off to indicate character has been entered
      delay(60);
      digitalWrite(greenLed,LED_ON);
      EEPROM.write(PSW_DATA_ADDR+j,c);
    }
    EEPROM.write(PSW_FLAG_ADDR, 143);                  // Set password flag
    BlinkLedFast(greenLed);
    digitalWrite(redLed,LED_OFF);  //turn off leds 
    digitalWrite(greenLed,LED_OFF);   
  }
  
  ScanEEPROM();   //read data from EEPROM and store it to the memory
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

    if(counter!=0 && (millis()-keypad_timer >= 2000))  //if keypad is waiting for next character and passed more than 2 sec, blink greenLed
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
  delay(3000); 
  NormalState();
}

/*-------------------------- Access Denied ------------------------------------------------*/
void AccessDenied(){
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

void CheckWipeButton(){
    //Wipe Code - If the Button pressed while setup run (powered on) it wipes EEPROM
    if (digitalRead(wipe_button) == LOW) {  // when button pressed pin should get low
      bool buttonState = monitorWipeButton(10000); // Give user 10 seconds to cancel operation
      if (buttonState == true && digitalRead(wipe_button) == LOW) {    // If button still be pressed, wipe EEPROM
        Serial.println(F("Starting Wiping EEPROM"));
        for (uint16_t x = 0; x < EEPROM.length(); x++) {    //Loop end of EEPROM address
          if (EEPROM.read(x) == 0) {              
            // do nothing, already clear, go to the next address in order to save time and reduce writes to EEPROM
          }
          else {
            EEPROM.write(x, 0);       //write 0 to clear, it takes 3.3mS
          }
        }
        //EEPROM successfully wiped
        digitalWrite(redLed,LED_OFF);
        digitalWrite(greenLed,LED_OFF);
        BlinkLedFast(greenLed);
      }
      else{
        //operation canceled
        digitalWrite(redLed,LED_OFF);
        digitalWrite(greenLed,LED_OFF);
        BlinkLedFast(redLed);
      }
    }
}

/*----------------------------- Monitor Wipe button ------------------------*/
bool monitorWipeButton(uint32_t interval) {
  digitalWrite(redLed,LED_OFF);
  digitalWrite(greenLed,LED_ON);
  uint32_t now = (uint32_t)millis();
  uint32_t previousMillis=(uint32_t)millis();
  while ((uint32_t)millis() - now < interval)  {
    // check on every half a second
    if (((uint32_t)millis() % 500) == 0) {
      if (digitalRead(wipe_button) != LOW)
        return false;
    }
    //blink effect 
    if((uint32_t)millis() -previousMillis>400) //change every 400ms
    {
      digitalWrite(redLed,!digitalRead(redLed));      // change state
      digitalWrite(greenLed,!digitalRead(greenLed));
      previousMillis=(uint32_t)millis();
    }   
  }
  return true;
}

/*------------------------- Blink LED fast ---------------------------------------------*/
void BlinkLedFast(uint8_t led){
  for(uint8_t i=0;i<10;i++) //blink 10 times
  {
    digitalWrite(led,LED_ON);
    delay(90);
    digitalWrite(led,LED_OFF);
    delay(90);
  }
}

/*---------------------------------------- Read data from EEPROM ----------------------------------------*/
void ScanEEPROM(){

  for ( uint8_t i = 0; i < 5; i++ ) {                     // Read Password from EEPROM
    storedPassword[i] = EEPROM.read(PSW_DATA_ADDR + i);  
    Serial.print(storedPassword[i]);
    
  }
}
