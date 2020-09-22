#include <SD.h>
#include <SPI.h>
#include <DS3231.h>
#include <LiquidCrystal.h>
#include <MultitapKeypad.h>
#include<EEPROM.h>
#include <ModbusMaster.h>
#include <String.h>
#include <Arduino_FreeRTOS.h>
#include <SoftwareSerial.h>
#define MAX485_RE_NEG 6
#define MAX485_DE 7


ModbusMaster node;

char str1[32];

int index = 0;
void TaskLCDKEYPAD( void *pvParameters );
void clrBuffer();
void getString(byte startPos, byte endPos);
void setCursorPos( void );
void decCursorPos( void );
byte getSymbol(byte ctr);
void displayInputMode(void);
void incCursorPos(void);
byte getAlphabet(byte chr, byte ctr);
void printToLcd(char chr);
void otherProcess(void);
char getAKey();
void waitUntilUnpressed() ;

File sdcard_file;
DS3231  rtc(SDA, SCL);
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
int CS_pin = 53; // Pin 53 on Arduino MEga
Sd2Card card;
SdVolume volume;
SdFile root;
const int sensor_pin = 1;
float Temp;  
float output;
int modbus;



void preTransmission()
{
digitalWrite(MAX485_RE_NEG, 1);
digitalWrite(MAX485_DE, 1);
}

void postTransmission()
{
digitalWrite(MAX485_RE_NEG, 0);
digitalWrite(MAX485_DE, 0);
}


struct readmem 
{
  char readstring[50];
};

struct writemem
{
   char writestring[50];
}customVar1,customVar2,customVar3,customVar4,customVar5,customVar6;


const uint16_t BACKLIGHT_PERIODS = 10000;
const byte ROW0 = 26;
const byte ROW1 = 27;
const byte ROW2 = 28;
const byte ROW3 = 29;
const byte COL0 = 22;
const byte COL1 = 23;
const byte COL2 = 24;
const byte COL3 = 25;

const byte LCD_RS = 12;
const byte LCD_EN = 11;
const byte LCD_D4 = 5;
const byte LCD_D5 = 4;
const byte LCD_D6 = 3;
const byte LCD_D7 = 2;
const byte BACKLIGHT = 19;
const byte BEEP = 20;

const byte CHR_BOUND = 3;
const byte BACKSPACE = 8;
const byte CLEARSCREEN = 12;
const byte CARRIAGE_RETURN = 13;
const byte CAPSLOCK_ON = 17;
const byte CAPSLOCK_OFF = 18;
const byte NUMLOCK_ON = 19;
const byte NUMLOCK_OFF = 20;

const char SYMBOL[] PROGMEM = 
{
  ',', '.', ';', ':', '!',
  '?', '\'', '"','-', '+',
  '*', '/', '=', '%', '^',
  '(', ')', '#', '@', '$',
  '[', ']', '{', '}', CHR_BOUND
};
const char ALPHABET[] PROGMEM = 
{
  'A', 'B', 'C', CHR_BOUND, CHR_BOUND,
  'D', 'E', 'F', CHR_BOUND, CHR_BOUND,
  'G', 'H', 'I', CHR_BOUND, CHR_BOUND,
  'J', 'K', 'L', CHR_BOUND, CHR_BOUND,
  'M', 'N', 'O', CHR_BOUND, CHR_BOUND,
  'P', 'Q', 'R', 'S',       CHR_BOUND,
  'T', 'U', 'V', CHR_BOUND, CHR_BOUND,
  'W', 'X', 'Y', 'Z',       CHR_BOUND
};

boolean alphaMode = true;
boolean upperCaseMode = true;
boolean autoOffBacklight = false;
boolean isEndOfDisplay = false;
byte startCursorPos;
byte endCursorPos;
byte cursorPos;
byte chrCtr;
unsigned long backlightTimeout;

char strBuffer[32];

// creates lcd as LiquidCrystal object
//LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
// creates kpd as MultitapKeypad object
// for matrix 4 x 3 keypad
// MultitapKeypad kpd(ROW0, ROW1, ROW2, ROW3, COL0, COL1, COL2);
// for matrix 4 x 4 keypad
MultitapKeypad kpd(ROW0, ROW1, ROW2, ROW3, COL0, COL1, COL2, COL3);
// creates key as Key object
Key key;

void setup() {
  
  lcd.begin(16,2);
  pinMode(MAX485_RE_NEG, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);
  Serial.begin(9600);
  node.begin(1, Serial);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  modbus = 0;
  pinMode(sensor_pin,INPUT);
  pinMode(CS_pin, OUTPUT);
  Initialize_SDcard();
  Initialize_RTC();
  initGSM();                      // init GSM module
  Read_LM35();                     // Read Temp
  Write_SDcard();                  // Writing in SD card
  lcd.setCursor(0,0);
  lcd.print("Date: ");
  lcd.print(rtc.getDateStr());
  delay(10000);
  lcd.setCursor(0,1);
  lcd.print("Time: ");
  lcd.print(rtc.getTimeStr());
 // lcd.clear();
  lcd.print("                ");
  delay(10000);
  lcd.setCursor(0,1);
  lcd.print("Temp: ");
  lcd.print(Temp);
  delay(10000);
  lcd.clear();
   
  uint8_t result;                                  //modbus Rs-485
  result = node.readInputRegisters(1,2);
  if (result == node.ku8MBSuccess)
{
  modbus=node.getResponseBuffer(0);
  Serial.println(modbus);
  delay(1000);
}
else {
  Serial.print("\nError ");
  delay(10000);
}
  
  
xTaskCreate(
    TaskLCDKEYPAD,
    (const portCHAR *)
      "LCDKEYPAD"   // A name just for humans
    ,  300  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  0  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL );

  
 
}

 
 

void TaskLCDKEYPAD(void *pvParameters)  // This is a task.
{
  (void) pvParameters;

// >

  Serial.println(F("Input Characters Demo..."));
  lcd.print(F("Input Characters"));
  lcd.setCursor(0, 1);
  lcd.print(F("Demo..."));
  vTaskDelay( 2000 / portTICK_PERIOD_MS );
  tone(BEEP, 4000, 50);

  lcd.clear();
  lcd.print("Previous Settings");
  vTaskDelay( 1000 / portTICK_PERIOD_MS );
  readmem var;
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("ID:");
  vTaskDelay( 1000 / portTICK_PERIOD_MS );
  lcd.clear();
  EEPROM.get(1,var);

  
  for(int i=0;i<=31;i++)
 {
   if (i<=15)
  {
        if(i<strlen( var.readstring ))  
        {
          lcd.setCursor(i,0);
          lcd.print(var.readstring[i]);
         }
       else
        {
          lcd.setCursor(i,0);
          lcd.print("\0");
        }
  
  }
  else
  {
        if(i<strlen( var.readstring ))
        {
           lcd.setCursor((i-16),1);
           lcd.print(var.readstring[i]);
        }
        else 
         {
           lcd.setCursor(i-16,0);
           lcd.print("\0");
         }
  }
}
  //lcd.print(var.readstring);
  vTaskDelay( 1000 / portTICK_PERIOD_MS );
  

  
  
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("PWD:");
  vTaskDelay( 1000 / portTICK_PERIOD_MS );
  lcd.clear();
  EEPROM.get(100,var);
    for(int i=0;i<=31;i++)
 {
   if (i<=15)
  {
        if(i<strlen( var.readstring ))
        {
          lcd.setCursor(i,0);
          lcd.print(var.readstring[i]);
         }
       else
        {
          lcd.setCursor(i,0);
          lcd.print("\0");
        }
  
  }
  else
  {
        if(i<strlen( var.readstring ))
        {
           lcd.setCursor((i-16),1);
           lcd.print(var.readstring[i]);
        }
        else 
         {
           lcd.setCursor(i-16,0);
           lcd.print("\0");
         }
  }
}
  //lcd.print(var.readstring);
  vTaskDelay( 2000 / portTICK_PERIOD_MS );


  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("URL");
  vTaskDelay( 1000 / portTICK_PERIOD_MS );
  lcd.clear();
  EEPROM.get(200,var);
  for(int i=0;i<=31;i++)
 {
   if (i<=15)
  {
        if(i<strlen( var.readstring ))
        {
          lcd.setCursor(i,0);
          lcd.print(var.readstring[i]);
         }
       else
        {
          lcd.setCursor(i,0);
          lcd.print("\0");
        }
  
  }
  else
  {
        if(i<strlen( var.readstring ))
        {
           lcd.setCursor((i-16),1);
           lcd.print(var.readstring[i]);
        }
        else 
         {
           lcd.setCursor(i-16,0);
           lcd.print("\0");
         }
  }
}
 // lcd.print(var.readstring);
  vTaskDelay( 2000 / portTICK_PERIOD_MS );


  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("DIR");
  vTaskDelay( 1000 / portTICK_PERIOD_MS );
  lcd.clear();
  EEPROM.get(300,var);
    for(int i=0;i<=31;i++)
 {
   if (i<=15)
  {
        if(i<strlen( var.readstring ))
        {
          lcd.setCursor(i,0);
          lcd.print(var.readstring[i]);
         }
       else
        {
          lcd.setCursor(i,0);
          lcd.print("\0");
        }
  
  }
  else
  {
        if(i<strlen( var.readstring ))
        {
           lcd.setCursor((i-16),1);
           lcd.print(var.readstring[i]);
        }
        else 
         {
           lcd.setCursor(i-16,0);
           lcd.print("\0");
         }
  }
}
  //lcd.print(var.readstring);
  vTaskDelay( 2000 / portTICK_PERIOD_MS );


  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("PORT");
  vTaskDelay( 1000 / portTICK_PERIOD_MS );
  lcd.clear();
  EEPROM.get(400,var);
   for(int i=0;i<=31;i++)
 {
   if (i<=15)
  {
        if(i<strlen( var.readstring ))
        {
          lcd.setCursor(i,0);
          lcd.print(var.readstring[i]);
         }
       else
        {
          lcd.setCursor(i,0);
          lcd.print("\0");
        }
  
  }
  else
  {
        if(i<strlen( var.readstring ))
        {
           lcd.setCursor((i-16),1);
           lcd.print(var.readstring[i]);
        }
        else 
         {
           lcd.setCursor(i-16,0);
           lcd.print("\0");
         }
  }
}
  //lcd.print(var.readstring);
  vTaskDelay( 2000 / portTICK_PERIOD_MS );
// <

  
  lcd.clear();
  lcd.print("Press 1 to enter");
  lcd.setCursor(0, 1);
  lcd.print("config mode");
  char chr;
  while (true) 
  {
    chr = getAKey();
    if (chr == '1') 
    {
      tone(BEEP, 5000, 20);
      break;
    }
    tone(BEEP, 4000, 100);
  }

  waitUntilUnpressed();
  Serial.print(F("Configuration Mode: "));
  Serial.println(chr);
  lcd.clear();
  lcd.cursor();
  lcd.blink();

 // writemem customVar1 = {strBuffer};
  
  lcd.setCursor(0, 1);
  lcd.print(F("ID:"));
  getString(19, 50);
  index=0;
  strcpy(customVar1.writestring,strBuffer);
  Serial.print(F("ID: "));
  Serial.println(strBuffer);
  //Serial.println(customVar1.writestring);
  EEPROM.put(1,customVar1.writestring);
  
 // writemem customVar2 = {strBuffer};
  
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(F("PWD:"));
  getString(20, 51);
  index=0;
  strcpy(customVar2.writestring,strBuffer);
  Serial.print(F("Password: "));
  Serial.println(strBuffer);
//  Serial.println(customVar2.writestring);
  EEPROM.put(100,customVar2.writestring);

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(F("URL:"));
  getString(20, 51);
  index=0;
  strcpy(customVar3.writestring,strBuffer);
  Serial.print(F("URL: "));
  Serial.println(strBuffer);
//  Serial.println(customVar3.writestring);
  EEPROM.put(200,customVar3.writestring);


  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(F("DIR:"));
  getString(20, 51);
  index=0;
  strcpy(customVar4.writestring,strBuffer);
  Serial.print(F("Directory "));
  Serial.println(strBuffer);
//  Serial.println(customVar4.writestring);
  EEPROM.put(300,customVar4.writestring);


  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(F("PORT:"));
  getString(21, 51);
  index=0;
  strcpy(customVar5.writestring,strBuffer);
  Serial.print(F("PORT: "));
  Serial.println(strBuffer);
//  Serial.println(customVar5.writestring);
  EEPROM.put(400,customVar5.writestring);
  
  
//  lcd.clear();
//  lcd.noCursor();
//  lcd.noBlink();
//  lcd.print(F("Please open the"));
//  lcd.setCursor(0, 1);
//  lcd.print(F("Serial Monitor"));

  Serial.print(F("DONE"));
  vTaskDelay( 2000 / portTICK_PERIOD_MS );

 /* for (;;) // A Task shall never return or exit.
  {
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    vTaskDelay( 1000 / portTICK_PERIOD_MS ); // wait for one second
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    vTaskDelay( 1000 / portTICK_PERIOD_MS ); // wait for one second
  }
  */
}


 
void loop()
{

}
    


void Write_SDcard()
{
    // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("data.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.print(rtc.getDateStr()); //Store date on SD card
    dataFile.print(","); //Move to next column using a ","

    dataFile.print(rtc.getTimeStr()); //Store time on SD card
    dataFile.print(","); //Move to next column using a ","

    dataFile.print(Temp); //Store data on SD card
    dataFile.print(","); //Move to next column using a ","

    dataFile.println(); //End of Row move to next row
    dataFile.close(); //Close the file
  }
  else
  Serial.println("OOPS!! SD card writing failed");
}

void Initialize_SDcard()
{
  
  
  
  // see if the card is present and can be initialized:
 /*if (SD.begin())
  {
    Serial.println("SD card is ready to use.");
  } else
  {
    Serial.println("SD card initialization failed");
    return;
  }*/
if (!card.init(SPI_HALF_SPEED, CS_pin)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card inserted?");
    Serial.println("* is your wiring correct?");
    return;
  } else {
    Serial.println("Wiring is correct and a card is present.");
  }

if (!volume.init(card)) {
    Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
    return;
  }


  // print the type and size of the first FAT-type volume
  uint32_t volumesize;
  Serial.print("\nVolume type is FAT");
  Serial.println(volume.fatType(), DEC);
  Serial.println();

  volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
  volumesize *= volume.clusterCount();       // we'll have a lot of clusters
  volumesize *= 512;                            // SD card blocks are always 512 bytes
  Serial.print("Volume size (bytes): ");
  Serial.println(volumesize);
  Serial.print("Volume size (Kbytes): ");
  volumesize /= 1024;
  Serial.println(volumesize);
  Serial.print("Volume size (Mbytes): ");
  volumesize /= 1024;
  Serial.println(volumesize);


  Serial.println("\nFiles found on the card (name, date and size in bytes): ");
  root.openRoot(volume);
  root.ls(LS_R | LS_DATE | LS_SIZE);
  
  
  sdcard_file = SD.open("data.txt", FILE_WRITE);
  if (sdcard_file) { 
    sdcard_file.print("Date");   
    sdcard_file.print(",");
    sdcard_file.print("Time");
    sdcard_file.print(",");
    sdcard_file.print("Temp");
    sdcard_file.println(",");
    sdcard_file.close(); // close the file
  }
  // if the file didn't open, print an error:
  else {
    Serial.println("error opening test.txt");
  }
}

void Initialize_RTC()
{
   // Initialize the rtc object
  rtc.begin();

//#### The following lines can be uncommented to set the date and time for the first time###  
/*
rtc.setDOW(FRIDAY);     // Set Day-of-Week to SUNDAY
rtc.setTime(18, 46, 45);     // Set the time to 12:00:00 (24hr format)
rtc.setDate(6, 30, 2017);   // Set the date to January 1st, 2014 
*/
}

void Read_LM35()
{
output = analogRead(sensor_pin);
Temp =(output*500)/1023;
}

void initGSM()
{
  //Begin serial communication with Arduino and SIM900
  Serial1.begin(9600);

  Serial.println("Initializing..."); 
  delay(1000);

  Serial1.println("AT"); //Handshaking with SIM900
  updateSerial();

  Serial1.println("AT+CMGF=1"); // Configuring TEXT mode
  updateSerial();
  Serial1.println("AT+CMGS=\"+919560799101\"");//change ZZ with country code and xxxxxxxxxxx with phone number to sms
  updateSerial();
  Serial1.print("M2MLogger.com"); //text content
  updateSerial();
  Serial1.write(26);
}


void updateSerial()
{
  delay(500);
  while (Serial.available()) 
  {
    Serial1.write(Serial.read());//Forward what Serial received to Software Serial Port
  }
  while(Serial1.available()) 
  {
    Serial.write(Serial1.read());//Forward what Software Serial received to Serial Port
  }
}





char getAKey() 
{
  while(true) 
  {
    key = kpd.getKey();
    if (key.state == KEY_DOWN || key.state == MULTI_KEY_DOWN)
      return key.character;
  }
}

void waitUntilUnpressed() 
{
  while(true) 
  {
    key = kpd.getKey();
    if (key.state == KEY_UP)
      return;
  }
}

void getString(byte startPos, byte endPos) 
{ index=0;
  char chr;
  byte strSize = endPos - startPos + 1;
  startCursorPos = startPos;
  endCursorPos = endPos;
  cursorPos = startPos;
  setCursorPos();
  chrCtr = 0;
  displayInputMode();
  boolean isCompleted = false;
  while (true) 
  { 
    key = kpd.getKey();
    switch (key.state) 
    {
      case KEY_DOWN:
      case MULTI_TAP:
        tone(BEEP, 5000, 20);
        digitalWrite(LED_BUILTIN, HIGH);
        switch (key.code) 
        {
          case KEY_1:
            chr = alphaMode ? getSymbol(key.tapCounter) : '1';
            break;
          case KEY_ASTERISK:
            chr = alphaMode ? 0 : key.character;
            break;
          case KEY_0:
            chr = alphaMode ? ' ' : '0';
            break;
          case KEY_NUMBER_SIGN:
            if (alphaMode) 
            {
              upperCaseMode = !upperCaseMode;
              chr = upperCaseMode ? CAPSLOCK_ON : CAPSLOCK_OFF;
            }
            else
              chr = key.character;
            break;
          case KEY_A:
            chr = BACKSPACE;
            break;
          case KEY_B:
          case KEY_C:
            chr = 0;
            break;
          case KEY_D:
            strBuffer[chrCtr] = 0;
             
            str1[chrCtr]=0;
            index=00000;
            
            isCompleted = true;
            break;
          default:
            chr = alphaMode ? getAlphabet(key.character, key.tapCounter) : key.character;
        }
        if (isCompleted)
          break;
        if (!upperCaseMode && chr >= 'A')
          chr += 32; // makes lower case
        if (key.state == MULTI_TAP && alphaMode && key.character >= '1' && key.character <= '9') 
        {
          printToLcd(BACKSPACE);
          chrCtr--;
        }
        if (chr > 0) 
        {
          if (chr == BACKSPACE)
            chrCtr--;
          if (chr >= ' ') 
          {
            strBuffer[chrCtr] = chr;
            if (chrCtr < strSize)
              chrCtr++;
          }
          printToLcd(chr);
        }
        autoOffBacklight = false;
        digitalWrite(BACKLIGHT, LOW);
        break;
      case LONG_TAP:
        switch (key.code) 
        {
          case KEY_A:
            chr = CLEARSCREEN;
            break;
          case KEY_NUMBER_SIGN:
            if (alphaMode)
              upperCaseMode = !upperCaseMode;
              alphaMode = !alphaMode;
              chr = alphaMode ? NUMLOCK_OFF : NUMLOCK_ON;
              break;
          default:
              chr = key.character;
        }
        if (chr < ' ' || alphaMode && chr >= '0' && chr <= '9') 
        {
          tone(BEEP, 5000, 20);
          if (chr >= '0' || chr == NUMLOCK_OFF) 
          {
            printToLcd(BACKSPACE);
            chrCtr--;
          }
          if (chr == CLEARSCREEN)
            chrCtr = 0;
          if (chr >= ' ') 
          {
            strBuffer[chrCtr] = chr;
            if (chrCtr < strSize)
              chrCtr++;
          }
          printToLcd(chr);
        }
        break;
      case MULTI_KEY_DOWN:
        tone(BEEP, 4000, 100);
        digitalWrite(LED_BUILTIN, LOW);
        break;
      case KEY_UP:
        digitalWrite(LED_BUILTIN, LOW);
        backlightTimeout = millis() + BACKLIGHT_PERIODS;
        autoOffBacklight = true;
        if (isCompleted)
          return;
    }
  }
}

void otherProcess(void) 
{
  if (autoOffBacklight && millis() >= backlightTimeout) 
  {
    digitalWrite(BACKLIGHT, HIGH);
    autoOffBacklight = false;
  }
}

void printToLcd(char chr) 
{

  switch (chr) 
  {
    case BACKSPACE:
      if (cursorPos > startCursorPos) 
      {
        if (!isEndOfDisplay)
          decCursorPos();
          setCursorPos();
          lcd.print(F(" "));
          setCursorPos();
        
      }
        setCursorPos();
        isEndOfDisplay = false;
      
      break;
    case CLEARSCREEN:
      while (cursorPos > startCursorPos) 
      {

        decCursorPos();
        setCursorPos();
        lcd.print(F(" "));
        setCursorPos();

      }

      break;
    case CAPSLOCK_ON:
    case CAPSLOCK_OFF:
    case NUMLOCK_ON:
    case NUMLOCK_OFF:
           displayInputMode();
           break;
    default:
      if (cursorPos == endCursorPos)
        isEndOfDisplay = true;
        lcd.print(chr);
        incCursorPos();
        setCursorPos();

  }
}

void displayInputMode(void) 
{
  lcd.setCursor(13, 0);
  if (alphaMode) 
  {
    if (upperCaseMode)
    {  lcd.print(F("ABC"));
     // lcd.setCursor(0,0);
     // lcd.print(strlen(strBuffer));
    } 
    else
     { lcd.print(F("abc"));
      //lcd.setCursor(0,0);
      //lcd.print(strlen(strBuffer));
     }
  }
  else
    {lcd.print(F("123"));
        //  lcd.setCursor(0,0);
      //lcd.print(strlen(strBuffer));
      }
  setCursorPos();
}

void incCursorPos(void) 
{
  if (cursorPos < endCursorPos) cursorPos++;
}

void decCursorPos( void ) 
{
  if (cursorPos > startCursorPos) cursorPos--;
}

void setCursorPos( void ) 
{
        lcd.setCursor(0,0);
        lcd.print(" ");
        lcd.setCursor(1,0);
        lcd.print(" ");
        if((32-index)>=10)
        {
        strcpy(str1,strBuffer);
        index = strlen(str1);
        lcd.setCursor(0,0);
        lcd.print(32-index);
        }
        else 
        {
          strcpy(str1,strBuffer);
          index = strlen(str1);
          lcd.setCursor(0,1);
          lcd.print(" ");
          lcd.setCursor(0,0);
          lcd.print(32-index);
        }
        
        lcd.setCursor(cursorPos % 16, cursorPos / 16);
        
        
}

byte getSymbol(byte ctr) 
{
  byte sym = pgm_read_byte_near(SYMBOL + ctr);
  if (sym == CHR_BOUND) 
  {
    sym = pgm_read_byte_near(SYMBOL);
    kpd.resetTapCounter();
  }
  return sym;
}

byte getAlphabet(byte chr, byte ctr) 
{
  chr = (chr - '2') * 5;
  byte alpha = pgm_read_byte_near(ALPHABET + chr + ctr);
  if (alpha == CHR_BOUND) 
  {
    alpha = pgm_read_byte_near(ALPHABET + chr);
    kpd.resetTapCounter();
  }
  return alpha;
}
void clrBuffer()
{int i = 0 ;
  for (i=0;i<=1;i++)
  strBuffer[i]=0;
  }
