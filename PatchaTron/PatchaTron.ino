#include <EEPROM.h>
#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_RA8875.h"
#include "Keypad.h"'
#include "MIDI.h";


//RA8875 settings

#define RA8875_INT 11
#define RA8875_CS 12
#define RA8875_RESET 9

Adafruit_RA8875 tft = Adafruit_RA8875(RA8875_CS, RA8875_RESET);


//touchscreen variables (including debounce)

uint16_t tx, ty;

uint16_t row;
uint16_t column;

uint16_t touchRow, touchCol;

int bounceCount;
long bounceTime = 0;
int bounceThreshold = 2; //anything more than 5 and its a stylus only device. less than 2 and its jittery.

//button size and spacing 

int x_spacing = 100;
int y_spacing = 66;
  
int btn_width = 94;
int btn_height = 62;

//array representing the state of switches visible on the screen (i.e. the active bus)
bool screenState[6][8];

//array representing the logical state of ALL 384 switches (not inluding mute)
bool cPA[48][8];

//quick memory
bool memCPA[48][8];
bool memMute[8];
uint16_t memBus;

//the active bus, most be between 0 and 7
uint16_t activeBus;

//array representing which of the buses is muted. 
bool busMute[8];


//static array. 
//the configuration below is specific to the Roland TR-626
// 47 bend points are connected to most of IC 15, a few points on IC5,
// 1 point each on IC18 and IC19, and each inst out L and H.


static const uint16_t buttonColors[6][8] = {
  { RA8875_RED, 0xC959, 0xC959, 0xCB2C, 0xE32C, 0xFB2C, 0xFCB2, 0xFD75 },
  { 0xC959, 0xC959, 0xC959, 0xCB2C, 0xE32C, 0xFB2C, 0xFCB2, 0xFD75 },
  { RA8875_RED, RA8875_BLACK, 0x94BF, 0xCCBF, 0xCCBF, 0x94BF, 0x94BF, 0x94BF },
  { 0xB659, 0xB659, 0x94BF, 0xCCBF, 0xCCBF, 0x94BF, 0x94BF, 0x94BF},
  { 0x94BF, 0x94BF, 0x94BF, 0xCCBF, 0xCCBF, 0x94BF, 0x94BF, 0x94BF },
  { 0x465F, 0x465F, 0x465F, 0x465F, 0x465F, 0x465F, 0x465F, 0x465F }
};


//keypad settings...
  
const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

//byte rowPins[ROWS] ={10, 9, 8, 7};  //connect to the row pinouts of the keypad
//byte colPins[COLS] = {6, 5, 4, 3}; //connect to the column pinouts of the keypad

byte rowPins[ROWS] ={3, 4, 5, 6};  //connect to the row pinouts of the keypad
byte colPins[COLS] = {7, 8, 9, 10}; //connect to the column pinouts of the keypad

Keypad keypad1 = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );


// eeprom size. (we are saving 64 user favorites of 49 bytes each here...)

const uint16_t eepromSize = 4096;

//keypad command buffer... (so we can parse multile keystrokes)

char commandBuffer[8];
byte commandLength;
bool commandMode;

//LED settings
#define PGM_LED 22
#define PGM2_LED 23

//crosspoint array settingss 

//defines the interface pins to the 3 Intersil CD22m30494 corss-point ICs

const byte xAddrPins[] = { 24, 25, 26, 27 };
const byte yAddrPins[] = { 28, 29, 30 };
const byte statePin = 31;
const byte strobePin = 32;
const byte csPins[] = { 33, 34, 35 };
const byte resetPin = 36;

const byte resetButtonPin = 37w;

//MIDI setup 
  MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, MIDI);
  
void setup() 
{
  row = 0;
  column = 0;
  
  Serial.begin(9600);
  Serial.println("RA8875 start");

  /* Initialise the display using 'RA8875_480x272' or 'RA8875_800x480' */
  if (!tft.begin(RA8875_800x480)) {
    Serial.println("RA8875 Not Found!");
    while (1);
  }

  Serial.println("Found RA8875");

  tft.displayOn(true);
  tft.GPIOX(true);      // Enable TFT - display enable tied to GPIOX
  tft.PWM1config(true, RA8875_PWM_CLK_DIV1024); // PWM output for backlight
  tft.PWM1out(255);

  bootSplash();


 //set the address pins to output mode
  uint16_t _pin = 22;
  while(_pin < 35)
  {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
    _pin++;
  }
  
  pinMode(resetButtonPin, INPUT_PULLUP);
  //turn off any switches in case this is a warm reboot...
  resetChips();
  
  //blink the LEDs to verify function
  pinMode(PGM_LED, OUTPUT);
  pinMode(PGM2_LED, OUTPUT);
   
  digitalWrite(PGM_LED, LOW);
  digitalWrite(PGM2_LED, HIGH);
  delay(200); 
  digitalWrite(PGM_LED, HIGH);
  digitalWrite(PGM2_LED, LOW); 
  delay(200);
  digitalWrite(PGM_LED, LOW);
  digitalWrite(PGM2_LED, HIGH);;
  delay(200);
  digitalWrite(PGM_LED, HIGH);
  digitalWrite(PGM2_LED, LOW); 
  delay(200);
  digitalWrite(PGM_LED, LOW);
  digitalWrite(PGM2_LED, LOW);   
  
  tft.fillScreen(RA8875_GREEN);
  delay(200);
  tft.fillScreen(RA8875_MAGENTA);
  delay(200);
  tft.fillScreen(RA8875_BLACK);
  
  drawScreen();
  

  pinMode(RA8875_INT, INPUT);
  digitalWrite(RA8875_INT, HIGH);
  
  tft.touchEnable(true);
    
  Serial.print("Status: "); Serial.println(tft.readStatus(), HEX);
  Serial.println("Waiting for touch events, keypad input or MIDI in...");
}



void loop() 
{
  float xScale = 1024.0F/tft.width();
  float yScale = 1024.0F/tft.height();
      
  if (digitalRead(resetButtonPin) == 0)
  {
      digitalWrite(PGM_LED, LOW);
      digitalWrite(PGM2_LED, HIGH); 
      resetAllSwitches();
      flushCommandBuffer();
      digitalWrite(PGM2_LED, LOW); 
  }
  /* Wait around for touch events, MIDI in or keypad input*/
  else if (!digitalRead(RA8875_INT)) //touch interrupt
  {

    if (tft.touched()) 
    {
      digitalWrite(PGM_LED, HIGH); 
      //touchpad driver on the RA8875 sucks.
      //we need to debounce
      if(millis() != bounceTime)
      {
        //reading = digitalRead(inPin);
        tft.touchRead(&tx, &ty);
        column = getColumn((uint16_t)(tx/xScale));
        row = getRow((uint16_t)(ty/yScale));
        
        if((touchRow != row || touchCol != column) && bounceCount > 0) //press is 
        {
          bounceCount--;
        }
        if(touchRow == row && touchCol == column)
        {
           bounceCount++; 
        }
        // If the Input has shown the same value for long enough let's switch it
        if(bounceCount >= bounceThreshold)
        {
          bounceCount = 0;
           //ok, we've got input! - Do Stuff!!!
          tft.touchEnable(false); //stop listening while we do this shit.
          bool _allowed = true;
          
          //Serial.print(tx); Serial.print(", "); Serial.println(ty);
          //Serial.print(row); Serial.print(", "); Serial.println(column);
          if(column == 0 && row == 0)
          {
            resetSwitches();
          //  screenToCrossPoint(activeBus);
          }
          else if((row == 0 || row == 1) && (column > 2)) //is an inst out toggle between low and high (row 0/row 1)
          { 
            uint16_t but_x;
            uint16_t but_y;
            bool _isOn = !screenState[row][column];  
            //check for conflict
            if(_isOn)
            {
              if(row == 0)
              {
                //turn off row 1
                setScreenSwitch(1, column, false);
              //  screenState[1][column] = false;
                but_x = 5 + (column * x_spacing); 
                but_y = 5 + (1 * y_spacing);     
                drawButton(but_x, but_y, screenState[1][column], true, buttonColors[1][column]);
              }            
              else if(row == 1)
              {
                //turn off row 0
                setScreenSwitch(0, column, false);
               // screenState[0][column] = false;
                but_x = 5 + (column * x_spacing); 
                but_y = 5;   
                drawButton(but_x, but_y, screenState[0][column], true, buttonColors[0][column]);
              }  
            }
            muteConflicting(activeBus, row, column);
        
            setScreenSwitch(row, column, _isOn);  
        
            but_x = 5 + (column * x_spacing); 
            but_y = 5 + (row * y_spacing);     
            drawButton(but_x, but_y, screenState[row][column], true, buttonColors[row][column]);      
          }
          else if(column < 2 && row == 2)
          {
            //is a power lead only allow 1 so we don't get a short1!!!
            uint16_t but_x;
            uint16_t but_y;
            bool _isOn = !screenState[row][column];  
            //check for conflict
            if(_isOn)
            {
              if(column == 0)
              {
                //turn off col 1
                setScreenSwitch(row, 1, false);

                but_x = 5 + (1 * x_spacing); 
                but_y = 5 + (row * y_spacing);     
                drawButton(but_x, but_y, screenState[row][1], true, buttonColors[row][1]);
 
                tft.textMode();
                tft.textSetCursor(but_x + 5, but_y - 2 );
                tft.textEnlarge(1);
                tft.textTransparent(RA8875_WHITE);
                tft.textWrite("-");
                tft.graphicsMode();
              }            
              else if(column == 1)
              {
                //turn off col 0
                setScreenSwitch(row, 0, false);

                but_x = 5; 
                but_y = 5 + (row * y_spacing);   
                drawButton(but_x, but_y, screenState[row][0], true, buttonColors[row][0]);
  
                tft.textMode();
                tft.textSetCursor(but_x + 5, but_y - 2);
                tft.textEnlarge(1);
                tft.textTransparent(RA8875_WHITE);
                tft.textWrite("+");
                tft.graphicsMode();
              } 
              muteConflicting(activeBus, row, column);  
            }
             
            setScreenSwitch(row, column, _isOn);
            but_x = 5 + (column * x_spacing); 
            but_y = 5 + (row * y_spacing);     
            drawButton(but_x, but_y, screenState[row][column], true, buttonColors[row][column]); 

            switch(column)
            {
              case 0:
                tft.textMode();
                tft.textSetCursor(but_x + 5, but_y - 2);
                tft.textEnlarge(1);
                tft.textTransparent(RA8875_WHITE);
                tft.textWrite("+");
                tft.graphicsMode();
                break;
              case 1:
                tft.textMode();
                tft.textSetCursor(but_x + 5, but_y - 2);
                tft.textEnlarge(1);
                tft.textTransparent(RA8875_WHITE);
                tft.textWrite("-");
                tft.graphicsMode();
                break;
            }
          }
          else if (row < 6 && column < 8)
          {

             bool _isOn = !screenState[row][column];
             if(_isOn)
               muteConflicting(activeBus, row, column);
         
             setScreenSwitch(row, column, _isOn);
         
             uint16_t but_x = 5 + (column * x_spacing); 
             uint16_t but_y = 5 + (row * y_spacing);
       
             drawButton(but_x, but_y, screenState[row][column], true, buttonColors[row][column]);
         
          }
          else if(row == 6)
          {
            //bus chooser button
            if(column != activeBus);
              setBus(column);
          }


        }
        touchRow = row;
        touchCol = column;
        
        bounceTime = millis();
      }
      tft.touchEnable(true);
      tft.touchRead(&tx, &ty); //clear out the input...
       
    } 
    digitalWrite(PGM_LED, LOW);
    digitalWrite(PGM2_LED, HIGH);  
    delay(40);
    digitalWrite(PGM2_LED, LOW); 
  }
  else if (MIDI.read())                // Is there a MIDI message incoming ?
  {
      uint16_t  prgmMIDI = 0;
      switch(MIDI.getType())      // Get the type of the message we caught
      {
          case midi::ProgramChange:       // If it is a Program Change,
              prgmMIDI = MIDI.getData1(); // TODO - set the active patch to the MIDI preset here...
              if(prgmMIDI < 64)
              {
                //only     64                           faves
                loadFavorite(prgmMIDI);
                drawScreen();
                activeBus = 0;
                drawBusButtons(activeBus);
              }
              break;
            // See the online reference for other message types
          default:
              break;
      }
  }
  else //check keypad
  {
    char key = keypad1.getKey();

     if (key != NO_KEY){
      Serial.println(key);
      switch(key)
      {
        case '*':
          commandMode = !commandMode;
          if(!commandMode)
          {
            digitalWrite(PGM_LED, LOW);
            digitalWrite(PGM2_LED, LOW); 
            Serial.print("Exiting Command Mode");
            flushCommandBuffer();
          }
          else
         {
           digitalWrite(PGM_LED, HIGH);
           Serial.print("Entering CommandMode");
           flushCommandBuffer();
         }
          break;
        case '#':
          {
            //end command
            digitalWrite(PGM_LED, LOW);
            digitalWrite(PGM2_LED, HIGH); 
            //parse buffer
            if(commandBuffer[0] == 'A')
            {
              //save command
              byte _mem;
              byte _len = commandLength - 1;
              switch(_len)
              {
                case 1:
                 _mem = commandBuffer[1] - '0';
                 saveFavorite(_mem);
                 Serial.print("Command: Save memory # "); 
                 Serial.print(_mem); 
                 break;
                case 2:
                 _mem = ((int)(commandBuffer[1] - '0') * 10) + (commandBuffer[2] - '0');
                 saveFavorite(_mem);
                 Serial.print("Command: Save memory # "); 
                 Serial.print(_mem); 
                 break;
              }
              flushCommandBuffer();
              commandMode = false;
              digitalWrite(PGM2_LED, LOW); 
            }
            else if (commandBuffer[0] == 'B')
            {
              //load command
              byte _len = commandLength - 1;
              byte _mem;
              switch(_len)
              {
                case 1:
                 {
                 _mem = commandBuffer[1] - '0';
                 loadFavorite(_mem);
                 Serial.print("Command: Load memory # "); 
                 Serial.print(_mem); 
                 }
                 break;
               case 2:
                 if(commandBuffer[1] == 'B' && commandBuffer[2] == 'B')
                 {
                   clearEeprom();
                   Serial.print("Command: Clear ALL patches from EEPROM! "); 
                 }
                 else
                 {
                   _mem = ((int)(commandBuffer[1] - '0') * 10) + (commandBuffer[2] - '0');
                   loadFavorite(_mem);
                   Serial.print("Command: Load memory # "); 
                   Serial.print(_mem);
                 } 
                break;
              }
            }       
          commandMode = false;
          flushCommandBuffer();
          digitalWrite(PGM2_LED, LOW); 
          }
         break;
       case '1':
       case '2':
       case '3':
       case '4':
       case '5':
       case '6':
       case '7':
       case '8':
       case '9':
       case '0':
       case 'A':
       case 'B':
          if(commandLength < 4)
          {
            commandBuffer[commandLength] = key;
            commandLength++;
            digitalWrite(PGM_LED, LOW);
            digitalWrite(PGM2_LED, HIGH); 
            delay(40);
            digitalWrite(PGM_LED, HIGH);
            digitalWrite(PGM2_LED, LOW); 
          }
          else
          {
            flushCommandBuffer();
            commandMode = false;
            digitalWrite(PGM_LED, LOW);
            digitalWrite(PGM2_LED, LOW); 
          }
          break;  
          
        case 'C':
        //M+ command
          digitalWrite(PGM_LED, LOW);
          digitalWrite(PGM2_LED, HIGH); 
          saveMemory();
          digitalWrite(PGM2_LED, LOW); 
          break;
        case 'D':
        //MR command
          digitalWrite(PGM_LED, LOW);
          digitalWrite(PGM2_LED, HIGH); 
          loadMemory();
          digitalWrite(PGM2_LED, LOW); 
          break;  
       }  

     }

   }
  
  

}

//do not use directly! Use SetSwitch() or SetScreenSwitch instead...
void setCPSwitch(byte xAddr, byte yAddr, byte _chip, bool _state)
{
   //reset strobe
  //  digitalWrite(strobePin, false);
        
    // reset chip select just in case
 //   digitalWrite(csPins[0], 0);
 //   digitalWrite(csPins[1], 0);
//    digitalWrite(csPins[2], 0);

    // set chip select pins
    for (int c = 0; c < 3; c++)
    {if(_chip == c)
      {
        digitalWrite(csPins[c], HIGH); 
      }
      else digitalWrite(csPins[c], LOW); 
      
    }
    
    //set X addr
    for (byte i=0; i<4; i++) {
      byte _bit = bitRead(xAddr, i);
      digitalWrite(xAddrPins[i], _bit);
    }
    
    //set Y addr
    for (byte i=0; i<3; i++) {
      byte _bit = bitRead(yAddr, i);
      digitalWrite(yAddrPins[i], _bit);
    }
    
    //set state
    digitalWrite(statePin, _state);
    

        
    //set strobe (this is what actually flips the SWITCH in the Crosspoint IC.) 
    digitalWrite(strobePin, HIGH);
    delayMicroseconds(100); //make sure it stays on for long enough to take...
    //digitalWrite(strobePin, LOW);
   // delayMicroseconds(10);
   // digitalWrite(strobePin, HIGH);
   // delayMicroseconds(10); 
    //reset strobe.
    digitalWrite(strobePin, LOW);
        // reset chip select
    // reset chip select pin
    digitalWrite(csPins[_chip], LOW);
  
}

void setSwitchMem(uint16_t _bus, uint16_t _index, bool _on)
{
  //translate 48x8 (bus, index) to 8x16x3 (X, Y, Chip) for SetCPSwitch().
  float _tmp = _index / 16;
  byte _chip = _tmp;
  
  byte _x = _index - (_chip * 16);
  byte _y = _bus;
  setCPSwitch(_x, _y, _chip, (_on && !busMute[_bus])); //respect mute because we are restoring from memory.
  //set cPA regardless of mute
  cPA[_index][_bus] = _on;
}

void setScreenSwitch(uint16_t _row, uint16_t _col, bool _on)
{
  //translate 6x8 (row, col) to 8x16x3 (X, Y, Chip) for SetCPSwitch().
  uint16_t _index = _row * 8 + _col;
  
  
  float _tmp = _index / 16;
  byte _chip = _tmp;
  
  byte _x = _index - (_chip * 16);
  byte _y = activeBus;
  setCPSwitch(_x, _y, _chip, _on);
  //set cPA
  cPA[_index][activeBus] = _on;
  screenState[_row][_col] = _on;
}

void setBus(uint16_t _bus)
{
  if(busMute[_bus])
  {
      muteConflicting(_bus);
      setMuteBus(_bus, true);
  }
  //screenToCrossPoint(activeBus); //redundant save. safe to remove...
  crossPointToScreen(_bus);
  activeBus = _bus;
  drawScreen();
  //set switches...
}

//call this against the switch you are about to turn on 
void muteConflicting(uint16_t _bus, uint16_t _row, uint16_t _col)
{
  //never let 2 buses connect, MUTE any that conflict with the active change
  bool _found = false;
  uint16_t _b = 0;
  uint16_t _index = (_row * 8) + _col;
  while(_b < 8)
  {

      if(_b != _bus && cPA[_index][_b] && !busMute[_b])
      {
        //found
        setMuteBus(_b, false);
        _found = true;
      }
   
    _b++;
  }
  if(_found)
    drawBusButtons(activeBus);
}

//call this against the bus you are about to turn on 
void muteConflicting(uint16_t _bus)
{
  bool _found = false;
    //never let 2 buses connect, MUTE any that conflict with the active change
  uint16_t _i = 0;
  while (_i < 48)
  {
    if(cPA[_i][_bus])
    {
      //is on. Check against other BUSes
       uint16_t _b = 0;
       while(_b < 8)
       {
         if(_b != _bus && cPA[_i][_b] && !busMute[_b])
         {
             //found conflict! Mute this BUS
             setMuteBus(_b, false);
              _found = true; 
           
         }
         _b++;
       }
    }
    _i++;
  }
  if(_found) 
    drawBusButtons(activeBus);
}


void setMuteBus(byte _bus, bool _state)
{
  byte i = 0;
  while(i < 16)
  {

      setCPSwitch(i, _bus, 0, (_state && cPA[i][_bus])); 
      setCPSwitch(i, _bus, 1, (_state && cPA[i+16][_bus])); 
      setCPSwitch(i, _bus, 2, (_state && cPA[i+32][_bus]));
    
   i++; 
  }
  busMute[_bus] = !_state;
}
/*void screenToCrossPoint(uint16_t _bus)
{
   uint16_t i = 0;
   
   while(i < 6)
   {
     uint16_t j = 0;
     while(j < 8)
     {
       uint16_t cpI = (i * 8) + j;
       cPA[cpI][_bus] = screenState[i][j];
       j++;
     }  
     i++;
   }
   
   
   
}
*/
void crossPointToScreen(uint16_t _bus)
{
   uint16_t i = 0;
   
   while(i < 6)
   {
     uint16_t j = 0;
     while(j < 8)
     {
       uint16_t cpI = (i * 8) + j;
       screenState[i][j] = cPA[cpI][_bus];
       j++;
     }  
     i++;
   }
}
void drawScreen()
{
  uint16_t _c = 0;
  do
  {
    int b_x = 5 + (_c * x_spacing); 
    for(int _y = 5; _y >= 0; _y--)
    {
      int b_y = 5 + (_y * y_spacing);

      drawButton(b_x, b_y, screenState[_y][_c], true, buttonColors[_y][_c]);
      if(_y == 2)
      {
        if(_c == 0)
        {
          //draw '+'
          tft.textMode();
          tft.textSetCursor(b_x + 5, b_y - 2 );
          tft.textEnlarge(1);
          tft.textTransparent(RA8875_WHITE);
          tft.textWrite("+");
          tft.graphicsMode();
        }
        if(_c == 1)
        {
          tft.textMode();
          tft.textSetCursor(b_x + 5, b_y - 2);
          tft.textEnlarge(1);
          tft.textTransparent(RA8875_WHITE);
          tft.textWrite("-");
          tft.graphicsMode();
        }
      }
      
    }
    _c++;
  } while (_c < 8);
  
  _c = 0;
  
  drawCancelButton(5, 5, true, RA8875_RED);
  drawBusButtons(activeBus);
  
}

void drawButton(int16_t x, int16_t y, bool is_on, bool is_enabled, int16_t color)
{

  
  tft.drawRoundRect(x, y, btn_width, btn_height, 10, RA8875_WHITE);
  tft.fillRoundRect(x+1, y+1, btn_width - 4, btn_height - 4, 10, color);  
  if(is_on)
  {
    tft.drawCircle(x + (btn_width / 2), y + (btn_height / 2), 22, RA8875_WHITE);
    tft.fillCircle(x + (btn_width / 2), y + (btn_height / 2), 20, RA8875_GREEN);
  }
  else
  {
    tft.drawCircle(x + (btn_width / 2), y + (btn_height / 2), 10, RA8875_WHITE);
    tft.fillCircle(x + (btn_width / 2), y + (btn_height / 2), 9, RA8875_BLACK);
  }

}

void drawCancelButton(int16_t x, int16_t y, bool is_enabled, int16_t color)
{

  
  tft.drawRoundRect(x, y, btn_width, btn_height, 10, RA8875_WHITE);
  tft.fillRoundRect(x+1, y+1, btn_width - 4, btn_height - 4, 10, color);  
/*  if(is_on)
  {
    tft.drawCircle(x + (btn_width / 2), y + (btn_height / 2), 22, RA8875_WHITE);
    tft.fillCircle(x + (btn_width / 2), y + (btn_height / 2), 20, RA8875_GREEN);
  }
  else
  {
    tft.drawCircle(x + (btn_width / 2), y + (btn_height / 2), 10, RA8875_WHITE);
    tft.fillCircle(x + (btn_width / 2), y + (btn_height / 2), 9, RA8875_BLACK);
  }
*/
    char string[7] = "CLEAR";
    tft.textMode();
    tft.textSetCursor(x + 6, y + 12);
    tft.textEnlarge(1);
    tft.textTransparent(RA8875_WHITE);
    tft.textWrite(string);
    tft.graphicsMode();
}

void drawBusButtons(uint16_t selBus)
{

  int _b = 0;
  while(_b < 8)
  {
    uint16_t _r = 6;
    uint16_t but_x = 5 + (_b * x_spacing); 
    uint16_t but_y = 5 + (_r * y_spacing); 
    tft.drawRoundRect(but_x, but_y, btn_width, btn_height, 10, RA8875_WHITE);
    if(selBus == _b)
    {
      tft.fillRoundRect(but_x+1, but_y+1, btn_width - 4, btn_height - 4, 10, 0x8410); 
    }
    else  tft.fillRoundRect(but_x+1, but_y+1, btn_width - 4, btn_height - 4, 10, RA8875_BLACK); 
    
    if(busMute[_b])
    {
      tft.drawCircle(but_x + 72, but_y + 14, 12, RA8875_WHITE);
      tft.fillCircle(but_x + 72, but_y + 14, 10, RA8875_ORANGE);
    }
    else
    {
      tft.drawCircle(but_x + 72, but_y + 14, 12, RA8875_WHITE);
      tft.fillCircle(but_x + 72, but_y + 14, 10, RA8875_GREEN);
    }
    
    String _lbl = String(_b + 1);
    char lbl[2];
    _lbl.toCharArray(lbl, 2);
    


    tft.textMode();
    tft.textSetCursor(but_x + 18, but_y + 12);
    tft.textEnlarge(1);
    tft.textTransparent(RA8875_WHITE);
    tft.textWrite(lbl);
    tft.graphicsMode();
    _b++;
  }
}





int getRow(uint16_t _y)
{
  //return 0 based row from screen coord.
  
  float _raw = _y / y_spacing;
  int _row = (int)_raw; //straight cast truncates...
  return _row;
}

int getColumn(uint16_t _x)
{
    //return 0 based column from screen coord.
  
  float _raw = _x / x_spacing;
  int _column = (int)_raw; //straight cast truncates...
  return _column;
}

void resetSwitches()
{
  int _c = 0;
  while(_c < 8)
  {
    for(int r = 0; r < 6; r++)
    {
      if(screenState[r][_c] == true)
      {
       // screenState[r][column] = false;
        setScreenSwitch(r, _c, false);
        uint16_t but_x = 5 + (_c * x_spacing); 
        uint16_t but_y = 5 + (r * y_spacing);
        drawButton(but_x, but_y, screenState[r][_c], true, buttonColors[r][_c]);
      }

    }
    _c++;
  }

}

void loadFavorite(byte _index)
{
  if(_index < 64)
  {
    //64 user favorites of 48 bytes each
    int _baseAddress = (_index * 49);
       
    //bit 49 is mute settings       
    //LOAD this first as 'setSwitchMem() uses it to restore mute settings!!!
    byte _data = EEPROM.read(_baseAddress + 48);
    for( int i = 0; i < 8; i++) //bit for each bus
    {
       byte _mute = bitRead(_data, i);
       busMute[i] = _mute;
    }  
   
    int j = 0;
    while (j < 48) //byte for each patch
    {
      _data = EEPROM.read(_baseAddress + j);
      for( int i = 0; i < 8; i++) //bit for each bus
      {
        byte _state = bitRead(_data, i);
        setSwitchMem(i, j, _state);
      }
      j++;
    }
    
    
    setBus(0);


  Serial.print("Success! loaded user favorite from slot # ");
  Serial.print(_index);
  }
  else  Serial.print("Error! User favorite must be < 64.");
}

void saveFavorite(byte _index)
{
  if(_index < 64)
  {
    //64 user favorites of 48 bytes each
    int _baseAddress = (_index * 49);
    byte _data;
  
    int j = 0;
    while (j < 48) //byte for each patch
    {
      byte _out = 0;
      for( int i = 0; i < 8; i++) //bit for each bus
      {     
        byte _on = cPA[j][i];
        _out += (_on << i);
      }
      EEPROM.write((_baseAddress + j), _out);
      j++;
  
    }
    //bit 49 is mute settings 
    byte _mBus;
    for( int i = 0; i < 8; i++) //bit for each bus
    {     
      byte _mute = busMute[i];
      _mBus += (_mute << i);
    }
    EEPROM.write((_baseAddress + 48), _mBus);
 
  Serial.print("Success! saved user favorite to slot # ");
  Serial.print(_index);
  }
  else  Serial.print("Error! User favorite must be < 64.");
}

void loadMemory()
{
  //mute settings first so setswitchmem can see conflicts
    for( int i = 0; i < 8; i++) //bit for each bus
    {
       busMute[i] = memMute[i];
    }  
   
    int j = 0;
    while (j < 48) //byte for each patch
    {
      for( int i = 0; i < 8; i++) //bit for each bus
      {
        setSwitchMem(i, j, memCPA[j][i]);
      }
      j++;
    }
    setBus(memBus);


  Serial.print("Success! loaded fast memory! ");
}

void saveMemory()
{

  
    int j = 0;
    while (j < 48) //byte for each patch
    {
      for( int i = 0; i < 8; i++) //bit for each bus
      {     
        memCPA[j][i] = cPA[j][i];
      }
      j++;
    }
    //mute settings 

    for( int i = 0; i < 8; i++) //bit for each bus
    {     
      memMute[i] = busMute[i];
    }
    
    memBus = activeBus;

 
  Serial.print("Success! saved fast memory! ");

}


void clearEeprom()
{
  char _x = 0;
  for (int i = 0; i < 4096; i++)
  {
    EEPROM.write(i, _x);
  }
}
void flushCommandBuffer()
{
  for(int i = 0; i < 8; i++)
  {
    commandBuffer[i] = 0;
  }
  commandLength = 0;
}

void resetAllSwitches()
{
  //turn off all switches via RESET pin
  resetChips();
  for (int x = 0; x < 48; x++)
  {
    for (int y = 0; y < 8; y++)
    {
      cPA[x][y] = 0;
      if(x == 47)
      {
        busMute[y] = 0;
      }
    }
  }
  setBus(0);
  
}

void resetChips()
{
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(csPins[i], HIGH);
  }
  digitalWrite(resetPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(resetPin, LOW);
  for (int i = 0; i < 3; i++)
  {
    digitalWrite(csPins[i], LOW);
  }
}

void bootSplash()
{
    tft.fillScreen(RA8875_BLACK);
    tft.textMode();
    tft.textSetCursor(80, 100);
    tft.textEnlarge(3);
    tft.textTransparent(RA8875_WHITE);
    tft.textWrite("Patch-A-Tron-O-Matic");
   
    tft.textSetCursor(100, 240);
    tft.textEnlarge(1); 
    tft.textTransparent(RA8875_CYAN);
    tft.textWrite("Circuit Bender's Solid State Patch Bay");
    
    tft.textSetCursor(140, 400);
    tft.textEnlarge(1);
    tft.textTransparent(RA8875_BLUE);
    tft.textWrite("Copyright 2015 by Alpha Charlie");
    delay(1500);
    tft.textSetCursor(10, 10);
    for (int i = 0; i < 60 ;i++)
    {
      
      tft.textEnlarge(1);
      tft.textTransparent(RA8875_RED);
      tft.textWrite("ALL WORK AND NO PLAY MAKES CHUCK A DULL BOY... ");
      delay(80);
      i++;
    }
    tft.graphicsMode();
    
}


  //drawing methods ( for my own copy/pastage)...

 // tft.drawCircle(100, 100, 50, RA8875_BLACK);
//  tft.fillCircle(100, 100, 49, RA8875_GREEN);
  
 // tft.fillRect(11, 11, 398, 198, RA8875_BLUE);
 // tft.drawRect(10, 10, 400, 200, RA8875_GREEN);
 // tft.fillRoundRect(200, 10, 200, 100, 10, RA8875_RED);
//  tft.drawPixel(10,10,RA8875_BLACK);
//  tft.drawPixel(11,11,RA8875_BLACK);
//  tft.drawLine(10, 10, 200, 100, RA8875_RED);
 // tft.drawTriangle(200, 15, 250, 100, 150, 125, RA8875_BLACK);
//  tft.fillTriangle(200, 16, 249, 99, 151, 124, RA8875_YELLOW);
//  tft.drawEllipse(300, 100, 100, 40, RA8875_BLACK);
//  tft.fillEllipse(300, 100, 98, 38, RA8875_GREEN);
  // Argument 5 (curvePart) is a 2-bit value to control each corner (select 0, 1, 2, or 3)
//  tft.drawCurve(50, 100, 80, 40, 2, RA8875_BLACK);  
//  tft.fillCurve(50, 100, 78, 38, 2, RA8875_WHITE);

//writing text 
/*
    tft.textMode();
    tft.textSetCursor(but_x + 18, but_y + 12);
    tft.textEnlarge(1);
    tft.textTransparent(RA8875_WHITE);
    tft.textWrite(lbl);
    tft.graphicsMode();
    */
