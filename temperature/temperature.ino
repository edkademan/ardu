// simple data logger
//
// using Arduino Uno and an Adafruit Logger Shield
//
// uses Adafruit's RTClib

#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SD.h>
#include <RTClib.h>
#include <stdio.h>
#include <string.h>
#include "temperature.h"

// Pin setup
#define BUTTON      2
#define GREENLED    3
#define REDLED      4
#define CHIPSELECT 10
#define NPINS       3  // number of thermistor pins

RTC_DS1307 RTC;
int        delta = 15*60;	/* measurement delta in seconds */
uint32_t   timeToMeasure;	/* in seconds */
int        buttonWasPressed = 0;
File       file;

OneWire p7(7);   DallasTemperature d7(&p7);
OneWire p8(8);   DallasTemperature d8(&p8);
OneWire p9(9);   DallasTemperature d9(&p9);

therm th[] = {{7, "pin 7", &d7},
	      {8, "pin 8", &d8},
	      {9, "pin 9", &d9}};

void ss(char* s) {Serial.println(s);}

int bufferFull(cbuf* b) {return b->n >= MAXBUFFER;}

void fail() {
  for(;;) {
    digitalWrite(REDLED, HIGH); delay(200);
    digitalWrite(REDLED, LOW);  delay(200);}}

// Return the current time in seconds since epoch.
uint32_t now() {return RTC.now().unixtime();}

void initializeRtc() {
  Wire.begin();
  RTC.begin();
  if (!RTC.isrunning()) {RTC.adjust(DateTime(__DATE__, __TIME__));}
  timeToMeasure = now();}  

void initializeSd() {
  pinMode(CHIPSELECT, OUTPUT);
  if(!SD.begin(CHIPSELECT)) {
    Serial.println("FATAL: SD");
    fail();}}

void initializeThermistors() {
  for(int i=0; i<NPINS; i++) th[i].ds->begin();}

/*
  daylight saving time active? (1 yes, 0 no)
  The code consults this to see if it's necessary to switch.
*/
int DST = 0;    /* daylight saving time active? (1 yes, 0 no) */

/* start/end of dst, month and sunday */
dstdef dstStart = {3, secondSunday};
dstdef dstEnd   = {11, firstSunday};

int monthOffset(int m) {
  switch(m) {
  case  1: return 0;
  case  2: return 3;
  case  3: return 3;
  case  4: return 6;
  case  5: return 1;
  case  6: return 4;
  case  7: return 6;
  case  8: return 2;
  case  9: return 5;
  case 10: return 0;
  case 11: return 3;
  case 12: return 5;}
  return -1;}

int leapYear(int y) {
  if(y % 400 == 0) return 1;
  if(y % 100 == 0) return 0;
  if(y % 4   == 0) return 1;
  return 0;}

/*
   Given year, month, and day of month return the day of
   week. (0=sunday, 6=saturday)
*/
int dayOfWeek(int y, int m, int dom) {
  int c,z;
  c = (int) y / 100;
  c -= 15;
  c %= 4;
  c -= 4;
  c *= -2;
  c %= 8;
  z = y % 100;
  return (((int) z)/4 + z + c + monthOffset(m) + dom -
	  (leapYear(y) && m<=2)) % 7;}

int daysInMonth(int y, int m) {
  switch(m) {
  case 1: case 3: case 5: case 7: case 8: case 10: case 12: return 31;
  case 4: case 6: case 9: case 11: return 30;}
  /* only February left */
  return leapYear(y) ? 29 : 28;}

/* 
   Find the days of month of all the sundays in month m for year
   y. The argument s is an array of length 5 that holds the first,
   second, third, fourth and last sundays.
*/
void sundaysFor(int y, int m, int* s) {
  int f, s0;
  f  = dayOfWeek(y, m, 1);	/* of 1st day of month */
  s0 = f ? 8-f : 1;		/* 1st sunday */
  for(int i=0; i<5; i++) s[i] = s0 + i*7;
  s[4] = (s[4] > daysInMonth(y, m)) ? s[3] : s[4];}

uint32_t dstdefToUnixtime(dstdef d) {
  int dom = -1, s[5], y = RTC.now().year();
  sundaysFor(y, d.mon, s);
  switch(d.sun) {
  case firstSunday:  dom = s[0]; break;
  case secondSunday: dom = s[1]; break;
  case thirdSunday:  dom = s[2]; break;
  case fourthSunday: dom = s[3]; break;
  case lastSunday:   dom = s[4]; break;}
  DateTime n(y, d.mon, dom, 2, 0, 0);
  return n.unixtime();}

int dstCurrently() {
  if(now() < dstdefToUnixtime(dstStart)) return 0;
  if(now() > dstdefToUnixtime(dstEnd))   return 0;
  return 1;}

void maybeAdjustForDst() {
  uint32_t n = RTC.now().unixtime();
  if(dstCurrently()) {
    if(DST) return;
    DateTime set1(n + 3600);
    RTC.adjust(set1);
    DST = 1;
    return;}
  /* currently standard time */
  if(DST) {
    DateTime set2(n - 3600);
    RTC.adjust(set2);
    DST = 0;}}

/* format the time into string s */
void formatCurrentTime(char* s) {
  DateTime n = RTC.now();
  sprintf(s, "%04d-%02d-%02d %02d:%02d:%02d %lu",
	  n.year(), n.month(), n.day(),
	  n.hour(), n.minute(), n.second(), n.unixtime());}

int sign(float x)       {return 2*(x>0.) - 1;}
int closestInt(float x) {return (int) (x + .5*sign(x));}

/*
   As of 2013-3 and Arduino 1.0.3 I can't seem to use the f format
   with sprintf.
*/
void formatTemp(byte ok, int pin, float t, char* s) {
  int cfahr;

  if(!ok) {
    sprintf(s, "%d NA", pin);
    return;}
  cfahr = closestInt(100.*t); /* fahrenheit × 100 */
  sprintf(s, "%d %3d.%02d", pin, cfahr/100, sign(cfahr)*cfahr%100);}
  
void formatFileName(char* s, int k) {
  DateTime n = RTC.now();
  sprintf(s, "%02d%02d%02d_%c.log",
	  n.year() % 100, n.month(), n.day(), 'a'+k);}

byte createUniqueFilename(char* s) {
  for(int k=0;; k++) {
    formatFileName(s,k);
    if(!SD.exists(s)) return 1;}
  return 0;}

byte openLogFile() {
  char name[20];

  if(file) return 1;		/* log already open */
  if(!createUniqueFilename(name)) return 2;
  file = SD.open(name, FILE_WRITE);
  if(!file) return 3;
  // ok, log is open
  digitalWrite(REDLED, HIGH);
  Serial.print("O: ");
  Serial.println(name);
  return 0;}

byte closeLogFile() {
  if(!file) return 1;		/* log is not open */
  file.close();
  digitalWrite(REDLED, LOW);
  Serial.print("C: ");
  Serial.println(file.name());
  return 0;}

int logOpen() {return file;}

void printDateTime() {
  char s[50];
  formatCurrentTime(s);
  Serial.print("c: ");
  Serial.println(s);}

void adjustClock(char* b) {
  int y,m,d,h,mm,s;
  DateTime n = RTC.now();
  y = n.year(); m  = n.month();  d = n.day();
  h = n.hour(); mm = n.minute(); s = n.second();
  if(dateSpec(b)) sscanf(b, "%d-%d-%d", &y, &m, &d);
  if(timeSpec(b)) {
    sscanf(b, "%d:%d", &h, &mm);
    if(strlen(b) <= 5) s = 0; else sscanf(b+5, ":%d", &s);}
  DateTime set(y, m, d, h, mm, s);
  RTC.adjust(set);
  DST = dstCurrently();
  printDateTime();
  timeToMeasure = now() + delta;}
    
void setDelta(char* b) {
  sscanf(b, "d%d", &delta);
  Serial.print("delta is ");
  Serial.println(delta);
  timeToMeasure = now() + delta;}

int readSerial(cbuf* b) {
  char c;
  while(!bufferFull(b) && Serial.available() > 0) {
    c = Serial.read();
    if(c == '\r' || c == '\n') {
      b->buf[b->n] = 0;
      return 1;}
    b->buf[b->n++] = c;}
  return 0;}

int dateSpec(char* s) {
  return (8 <= strlen(s) && strlen(s) <= 10 &&
	  s[4] == '-' && (s[6] == '-' || s[7] == '-'));}

int timeSpec(char* s) {
  return strlen(s) >= 5 && s[2] == ':';}

void runCommand(cbuf* b) {
  if(dateSpec(b->buf) || timeSpec(b->buf)) adjustClock(b->buf);
  if(b->buf[0] == 'd') setDelta(b->buf);
  if(b->buf[0] == 'o') openLogFile();
  if(b->buf[0] == 'c') closeLogFile();
  if(b->buf[0] == 'r') printDateTime();
  b->n      = 0;
  b->buf[0] = 0;}

void handleButton() {file ? closeLogFile() : openLogFile();}

/*
  Return true if we detect that the button has transitioned from
  unpressed to pressed.
*/
int buttonPress() {
  if(!digitalRead(BUTTON)) {
    buttonWasPressed = 0;
    return 0;}
  if(buttonWasPressed) return 0; /* already knew about press */
  for(int i=0; i<10; i++) {
    delay(10);
    buttonWasPressed = digitalRead(BUTTON);
    if(!buttonWasPressed) return 0;}
  return 1;}

float readTemperature(int n, byte *ok) {
  float x;

  th[n].ds->requestTemperatures();
  x = th[n].ds->getTempFByIndex(0);
  *ok = x > -150;
  return x;}
  
void createInfo(char* s) {
  byte ok;
  float t;
  char tempString[50], timeString[50];

  s[0] = 0;
  for(int i=0; i<NPINS; i++) {
    t = readTemperature(i, &ok);
    formatTemp(ok, th[i].pin, t, tempString);
    formatCurrentTime(timeString);
    strcat(s, timeString);
    strcat(s, " ");
    strcat(s, tempString);
    strcat(s, "\n");}}

void writeInfo() {
  char infoString[200];

  digitalWrite(GREENLED, LOW);
  createInfo(infoString);
  if(logOpen()) {
    file.write(infoString);
    file.flush();}
  Serial.print(infoString);
  digitalWrite(GREENLED, HIGH);}

void myloop(cbuf* b) {
  for(;;) {
    if(readSerial(b)) runCommand(b);
    if(buttonPress()) handleButton();
    if(now() >= timeToMeasure) {
      maybeAdjustForDst();
      timeToMeasure = now() + delta;
      writeInfo();}}}
  
// ----- MAIN -----
void setup() {
  cbuf inBuffer;

  Serial.begin(57600);
  pinMode(BUTTON,   INPUT);
  pinMode(REDLED,   OUTPUT);
  pinMode(GREENLED, OUTPUT);
  initializeRtc();
  initializeSd();
  initializeThermistors();
  inBuffer.n = 0;
  digitalWrite(GREENLED, HIGH);
  openLogFile();
  myloop(&inBuffer);}

void loop() {}
