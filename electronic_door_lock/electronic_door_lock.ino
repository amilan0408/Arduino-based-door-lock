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
#define MC_FLAG_ADDR 0  // Master Card flag at address 0
#define PSW_FLAG_ADDR 1 // Password flag at address 1
#define MC_DATA_ADDR 2  // Master Card data at addresses 2 - 6
#define PSW_DATA_ADDR 6 // Password data at addresses 6 to 11
#define ID_SECTION 11   // from 11 to the end addresses are reserved for card's IDs (every ID occupy 4Bytes)
#define NUM_OF_CARDS 500 //value at address 500 represents number of stored card id's

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
  
uint8_t numOfStoredCards;
byte storedCards[10][4];    //2 dimensional array which holds IDs of registered cards (It can hold max 10 cards)
byte readCard[4];   // Stores scanned ID read from RFID Module
byte masterCard[4];   // Stores master card's ID read from EEPROM

void setup() { 
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(button, INPUT_PULLUP);
  pinMode(wipe_button, INPUT_PULLUP);
  pinMode(relay, OUTPUT);

  SPI.begin();           // MFRC522 Hardware uses SPI protocol
  rfid.PCD_Init();    // Initialize MFRC522 Hardware
  //If you set Antenna Gain to Max it will increase reading distance
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);
  
  CheckWipeButton();


  /*------------------------------- Checking and defining Master Card ---------------------------------*/
  // Check if master card defined, if not let user choose a master card
  // If master card is defined, EEPROM holds number 143 at address 0
  if (EEPROM.read(MC_FLAG_ADDR) != 143) 
  {
    bool successRead;    //Successful Read from Reader -> true
    do {
      successRead = getID();            // sets successRead to true when we get read from reader otherwise false
      digitalWrite(redLed, LED_ON);    // Visualize Master Card need to be defined
      digitalWrite(greenLed, LED_OFF);
      delay(200);
      digitalWrite(redLed, LED_OFF);
      digitalWrite(greenLed, LED_ON);
      delay(200);
    }
    while (!successRead);                  // Program will not go further while you not get a successful read
    for ( uint8_t j = 0; j < 4; j++ ) {        // Loop 4 times
      EEPROM.write( MC_DATA_ADDR + j, readCard[j] );  // Write scanned PICC's UID to EEPROM, start from address 2
    }
    EEPROM.write(MC_FLAG_ADDR, 143);                  // Write to EEPROM we defined Master Card.
  
    BlinkLedFast(greenLed); //blink greenLed to indicate Master Card is defined successfully
  }

  /*----------------------- Checking and defining Password in EEPROM ---------------------------------*/
  // Check if password is defined, if not let user choose it
  // If password is defined, EEPROM holds number 143 at address 1
  if (EEPROM.read(PSW_FLAG_ADDR) != 143) 
  { //if there is no defined password
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
    //--------- RFID check -----------
    if(getID())
    {     
      if(Compare(readCard,masterCard,4)) ProgrammingMode();
      else{
        bool card_found=false;
        for(uint8_t i=0;i<numOfStoredCards;i++){
          if(Compare(readCard,storedCards[i],4)){
            AccessGranted();
            card_found=true;
            break;
          }
        }
        if(!card_found) AccessDenied();
      }
      break;
    }
    
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

  for ( uint8_t i = 0; i < 4; i++ ) {                     // Read Master Card's UID from EEPROM
    masterCard[i] = EEPROM.read(MC_DATA_ADDR + i);        // Write it to masterCard variable
  }
  for ( uint8_t i = 0; i < 5; i++ ) {                     // Read Password from EEPROM
    storedPassword[i] = EEPROM.read(PSW_DATA_ADDR + i);  
  }

  numOfStoredCards=EEPROM.read(NUM_OF_CARDS);

  if(numOfStoredCards!=0)  //if there is data at ID_SECTION
  {
    uint16_t counter=0;
    for(uint8_t i=0;i<numOfStoredCards;i++)
    {  
      for(uint8_t j=0;j<4;j++)
      {
        storedCards[i][j]=EEPROM.read(ID_SECTION+counter);
        counter++;
      }     
    }
  }
}

/*-------------------------- Get PICC's UID ------------------------------------------------*/
bool getID() {
  // Getting ready for Reading PICCs
  if ( ! rfid.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return false;
  }
  if ( ! rfid.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return false;
  }
  
  for ( uint8_t i = 0; i < 4; i++) { 
    readCard[i] = rfid.uid.uidByte[i];
  }
  rfid.PICC_HaltA(); // Stop reading
  return true;
}

/*------------------------- Programming Mode -----------------------------------------------*/
void ProgrammingMode(){

  digitalWrite(redLed,LED_OFF);
  digitalWrite(greenLed,LED_ON);
  bool successRead=false;
  bool keypad_pressed=false;
  char c;                                 //stores first character of new password
  unsigned long previousMillis=millis();
  while(!successRead && !keypad_pressed)  //if card is detected or keypad is pressed, exit a loop
  {
    //---- waiting for any input -------
    successRead = getID();  
    c=input.getKey();
    if(c!= NO_KEY) keypad_pressed=true;
      
    //---- blinking animation -------
    if(millis()-previousMillis>=400)
    {
      previousMillis=millis();
      digitalWrite(redLed,!digitalRead(redLed));      // change state
      digitalWrite(greenLed,!digitalRead(greenLed));
    }
    delay(10);
  }
  
  digitalWrite(redLed,LED_ON);
  digitalWrite(greenLed,LED_ON);
  //---------------------- RFID CARD ---------------------------
  if(successRead && !Compare(readCard,masterCard,4))  // if new card is presented (not master card again)
  {
    if(CheckID(readCard)) DeleteID(readCard);   //if the card is already in EEPROM, erase it
    else AddID(readCard);             //else add it to the EEPROM             
  }

  //---------------------- NEW PASSWORD ------------------------
  if(keypad_pressed)  //if keypad is pressed - let user enter new password
  {
    EEPROM.write(PSW_DATA_ADDR,c);  //write first password character to EEPROM
    for(uint8_t i=1;i<5;i++)        //enter remained characters and write it to the EEPROM
    {
      c=input.waitForKey();
      digitalWrite(greenLed,LED_OFF);  //turn led of to indicate character has been entered
      delay(60);
      digitalWrite(greenLed,LED_ON);
      EEPROM.write(PSW_DATA_ADDR+i,c);
    }
    digitalWrite(redLed,LED_OFF);
    BlinkLedFast(greenLed);
    ScanEEPROM(); //load new data to memory
  }

}

/*--------------------------------------- Check if ID exits in EEPROM -----------------------------------*/
bool CheckID(byte id[]){
  if(numOfStoredCards==0) return false;
  else
  {
    for(uint8_t i=0;i<numOfStoredCards;i++)
    {
      if(Compare(id,storedCards[i],4)) return true;
      else return false;
    }
  }
}

/*---------------------------------------- Add ID to the EEPROM -----------------------------------------*/
void AddID(byte id[]){
  
  uint16_t startAddr=ID_SECTION+numOfStoredCards*4;             //find empty slot
 
  for(uint8_t i=0;i<4;i++)  EEPROM.write(startAddr+i, id[i]);    //write  it to EEPROM
  numOfStoredCards++;   //update number of stored cards
  EEPROM.write(NUM_OF_CARDS,numOfStoredCards);   
  
  ScanEEPROM(); //load new data to memory

  BlinkLedFast(greenLed);
}

/*---------------------------------------- Delete ID from the EEPROM ------------------------------------*/
void DeleteID(byte id[]){
  uint16_t id_addr;
  uint8_t i;
  for(i=0;i<numOfStoredCards;i++)         //Find the address of the requested id
  {
    if(Compare(id,storedCards[i],4))      //when you find id, convert position to EEPROM address
    {
      id_addr=ID_SECTION+i*4;       //conversion 
      break;                        //break the loop
    }
  }

  if(i==(numOfStoredCards-1)) //if requested id was the last one, just delete it
  {
    for(uint8_t i=0;i<4;i++) EEPROM.write(id_addr+i,0);
  }
  else                        //otherwise delete and fill the empty slot
  {
    uint16_t last_id_addr=(ID_SECTION+numOfStoredCards*4)-4;
    byte temp_array[4];
    for(uint8_t j=0;j<4;j++)   //copy last ID in temporary array and delete it from EEPROM
    {
      temp_array[j]=EEPROM.read(last_id_addr +j); //copy data to temporary array
      EEPROM.write(last_id_addr +j,0);            //delete data
    }
    for(uint8_t k=0;k<4;k++)  EEPROM.write(id_addr+k,temp_array[k]);      //rewrite it with last ID
  }
  
  numOfStoredCards--;
  EEPROM.write(NUM_OF_CARDS,numOfStoredCards);

  ScanEEPROM(); //reload data to memory

  BlinkLedFast(redLed);
}
