#include <SimpleDHT.h>

#include <MegaTimers.h>
#include "font.h"

#define _259_D 12
#define _259_A0 11
#define _259_A1 10
#define _259_A2 9

#define _273_D5 12
#define _273_D3 11
#define _273_D4 10
#define _273_D6 9
#define _273_D2 8
#define _273_D0 7
#define _273_D1 6

#define _G2A 5
#define _C 4
#define _B 3
#define _A 2

#define _CLR 13

#define _273_6 0
#define _273_5 1
#define _273_4 2
#define _273_3 3
#define _273_2 4
#define _273_1 5
#define _259_1 6
#define _259_2 7

#define ScrollTime 100

#define THERMISTORPIN A1
#define THERMISTORNOMINAL 2200
#define TEMPERATURENOMINAL 25 
#define BCOEFFICIENT 3950
#define NUMSAMPLES 5
#define SERIESRESISTOR 1000    
int samples[NUMSAMPLES];

static const unsigned char text[] = {
  31,0x44,0x6F,0x6D,0x69,0x6E,0x69,0x6B,0x20,0x53,0x7A,0x79,0x6D,0x61,0x6E,0x73,0x6B,0x69,0x2E,0x20,0x53,0x6F,0x20,0x4D,0x75,0x63,0x68,0x20,0x4C,0x65,0x64,0x2E
};

void updateTemp();
void initTempText();
void clear138();
void clear259();
void clearState();
void commitDataToAddr(byte addr);
void set259High(byte chipAddr);
void uploadColumn(byte col);
void selectColumn(byte col);
void selectColumnRaw(byte chipAddr, byte addr);
void placeChar(byte code, byte col);
byte getGroup(byte column);
byte getGroupAddr(byte column);
byte getCharByte(int index);
void Scroll(byte fromCol);
void Refresh();

volatile byte cols[250];
volatile byte column;

volatile unsigned long lastTime;

volatile unsigned long lastTempTime;

void setup() {
  Serial.begin(115200);
  pinMode(13, OUTPUT);
  pinMode(_273_D0, OUTPUT);
  pinMode(_273_D1, OUTPUT);
  pinMode(_273_D2, OUTPUT);
  pinMode(_273_D3, OUTPUT);
  pinMode(_273_D4, OUTPUT);
  pinMode(_273_D5, OUTPUT);
  pinMode(_273_D6, OUTPUT);
  pinMode(_CLR, OUTPUT);
  pinMode(_G2A, OUTPUT);
  pinMode(_A, OUTPUT);
  pinMode(_B, OUTPUT);
  pinMode(_C, OUTPUT);
  analogReference(EXTERNAL);
  delay(1000);
  clear138();
  clear259();
  clearState();

  byte count = 41 < text[0] ? 41 : text[0];
  for(byte i = 0; i < count; i++)
  {
    byte ascii = text[i+1];
    placeChar(ascii, (i * 6) + 24);
  }

  Timer1.initialize(500);
  Timer1.attachInterrupt(Refresh);
  lastTime = millis();
  initTempText();
  updateTemp();
  lastTempTime = millis();
}

void initTempText()
{
  placeChar(45, 0);
  placeChar(45, 6);
  placeChar(167, 12);
  placeChar('C', 18);
}

void updateTemp()
{
  float average;
 
  // take N samples in a row, with a slight delay
  for (byte i=0; i< NUMSAMPLES; i++) {
   samples[i] = analogRead(THERMISTORPIN);
   delay(5);
  }
 
  // average all the samples out
  average = 0;
  for (byte i=0; i< NUMSAMPLES; i++) {
     average += samples[i];
  }
  average /= NUMSAMPLES;

  average = 1023 / average - 1;
  average = SERIESRESISTOR / average;

  float steinhart;
  steinhart = average / THERMISTORNOMINAL;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert to C

  if(steinhart > 99.5)
  {
    placeChar('H', 0);
    placeChar('I', 0);
    return;
  }

  if(steinhart < -9.5)
  {
    placeChar('L', 0);
    placeChar('O', 0);
    return;
  }

  if(steinhart < 0)
  {
    placeChar('-', 0);
    placeChar(0x30 + (int)steinhart, 6);
  }

  placeChar(0x30 + (byte)(steinhart / 10), 0);
  placeChar(0x30 + (byte)((int)steinhart % 10), 6);
}

void placeChar(unsigned char code, byte col)
{
  int base = (int)code * 5;
  cols[col] = getCharByte(base);
  cols[col+1] = getCharByte(base+1);
  cols[col+2] = getCharByte(base+2);
  cols[col+3] = getCharByte(base+3);
  cols[col+4] = getCharByte(base+4);
}

byte getGroup(byte column)
{
  return column / 15;
}

byte getGroupAddr(byte column)
{
  byte grp = column / 15;
  switch(grp)
  {
    case 0:
      return _273_1;
    case 1:
      return _273_2;
    case 2:
      return _273_3;
    case 3:
      return _273_4;
    case 4:
      return _273_5;
    case 5:
      return _273_6;
  }
}

void commitDataToAddr(byte addr)
{
  digitalWrite(_A, addr & 0x01);
  digitalWrite(_B, addr & 0x02);
  digitalWrite(_C, addr & 0x04);
  digitalWrite(_G2A, LOW);
  digitalWrite(_G2A, HIGH);
}

void clear138()
{
  digitalWrite(_G2A, HIGH);
}

void clear259()
{
  set259High(_259_1);
  set259High(_259_2);
}

void clearState()
{
  digitalWrite(_CLR, LOW);
  digitalWrite(_CLR, HIGH);
}

void uploadColumn(byte col)
{
  byte val = cols[col];
  digitalWrite(_273_D1, val & 0x01);
  digitalWrite(_273_D0, val & 0x02);
  digitalWrite(_273_D2, val & 0x04);
  digitalWrite(_273_D6, val & 0x08);
  digitalWrite(_273_D4, val & 0x10);
  digitalWrite(_273_D3, val & 0x20);
  digitalWrite(_273_D5, val & 0x40);
  commitDataToAddr(getGroupAddr(col));  
}

void closeColumn(byte col)
{
  byte addr = translateColumn(col) % 15;  
  byte chipAddr = (addr + 1) & 0x08 ? _259_1 : _259_2;
  digitalWrite(_259_D, HIGH);
  digitalWrite(_259_A0, addr & 0x01);
  digitalWrite(_259_A1, addr & 0x02);
  digitalWrite(_259_A2, addr & 0x04);
  commitDataToAddr(chipAddr);
}

void selectColumn(byte col)
{
  byte addr = translateColumn(col) % 15;  
  byte chipAddr = (addr + 1) & 0x08 ? _259_1 : _259_2;
  digitalWrite(_259_D, LOW);
  digitalWrite(_259_A0, addr & 0x01);
  digitalWrite(_259_A1, addr & 0x02);
  digitalWrite(_259_A2, addr & 0x04);
  commitDataToAddr(chipAddr);
  //selectColumnRaw(chipAddr, addr & 0x07);
}

void selectColumnRaw(byte chipAddr, byte addr)
{
  for(byte i = 0; i < 8; i++)
  {
      digitalWrite(_259_D, i == (addr & 0x07) && chipAddr == _259_1 ? LOW : HIGH);
      digitalWrite(_259_A0, i & 0x01);
      digitalWrite(_259_A1, i & 0x02);
      digitalWrite(_259_A2, i & 0x04);
      commitDataToAddr(_259_1);
  }
  for(byte i = 0; i < 8; i++)
  {
      digitalWrite(_259_D,  i == (addr & 0x07) && chipAddr == _259_2 ? LOW : HIGH);
      digitalWrite(_259_A0, i & 0x01);
      digitalWrite(_259_A1, i & 0x02);
      digitalWrite(_259_A2, i & 0x04);
      commitDataToAddr(_259_2);
  }
}

void set259High(byte chipAddr)
{
  for(byte i = 0; i < 8; i++)
  {
      digitalWrite(_259_D,  HIGH);
      digitalWrite(_259_A0, i & 0x01);
      digitalWrite(_259_A1, i & 0x02);
      digitalWrite(_259_A2, i & 0x04);
      commitDataToAddr(chipAddr);
  }
}

byte translateColumn(byte col)
{
  byte base = (col / 15) * 15;
  byte rest = col % 15;
  if(rest == 7) return col;
  if(rest < 7) return base + (6 - rest & 0x7);
  return base + (14 - (rest & 0x7));
}

void loop() {
  unsigned long now = millis();
  if((now - lastTime) > ScrollTime || now < lastTime)
  {
    Scroll(24);
    lastTime = now;
  }
  now = millis();
  if((now - lastTempTime) > 1000 || now < lastTempTime)
  {
    updateTemp();
    lastTempTime = now;
  }
}

void Scroll(byte fromCol = 0)
{
  byte lastCol = cols[fromCol];
  for(byte i = fromCol; i < 249; i++)
  {
    cols[i] = cols[i + 1];
  }

  cols[249] = lastCol;
}

void Refresh()
{
  closeColumn(column == 0 ? 14 : column - 1);
  for(byte i = 0; i < 6; i++)
  {
    uploadColumn(column + (i * 15));
  }
  selectColumn(column);
  column++;
  if(column >= 15) column = 0;
}

byte getCharByte(int index)
{
  return pgm_read_word_near(font5x7 + index);
}

