// simple data logger
//
// using Arduino Uno and an Adafruit Logger Shield
//
// uses Adafruit's RTClib

#include <Wire.h>
#include <OneWire.h>
#include <SD.h>
#include <RTClib.h>
#include <stdio.h>
#include <string.h>
#include "temperature.h"

// Pin setup
#define BUTTON      2
#define GREENLED    3
#define REDLED      4
#define ONEWIREPIN  7
#define CHIPSELECT 10

static RTC_DS1307 RTC;
static OneWire    ds(ONEWIREPIN);
static int        delta = 10;	 /* measurement delta in seconds */
static uint32_t   timeToMeasure; /* in seconds */
static int        buttonWasPressed = 0;
static File       file;

static int bufferFull(cbuf* b) {return b->n >= MAXBUFFER;}

static void fail() {
  for(;;) {
    digitalWrite(REDLED, HIGH); delay(200);
    digitalWrite(REDLED, LOW);  delay(200);}}

// Return the current time in seconds since epoch.
static uint32_t now() {RTC.now().unixtime();}

static void initializeRtc() {
  Wire.begin();
  RTC.begin();
  if (!RTC.isrunning()) {RTC.adjust(DateTime(__DATE__, __TIME__));}
  timeToMeasure = now();}  

static void initializeSd() {
  pinMode(CHIPSELECT, OUTPUT);
  if(!SD.begin(CHIPSELECT)) {
    Serial.println("FATAL: SD");
    fail();}}

static int hiresDevice(byte* a) {return *a == 0x10;}

static void initializeThermistor(byte* dr) {
  ds.reset_search();
  if(!ds.search(dr)) {
    Serial.println("FATAL: ADDR?");
    fail();}
  if(OneWire::crc8(dr, 7) != dr[7]) {
    Serial.println("FATAL: ACRC?");
    fail();}
  switch (dr[0]) {
  case 0x10:
    Serial.print("DS18S20: ");
    break;
  case 0x28:
    Serial.print("DS18B20: ");
    break;
  case 0x22:
    Serial.print("DS1822: ");
    break;
  default:
    Serial.print("FATAL: DEV?");
    fail();} 
  for(int i=0;i<7;i++) {
    Serial.print(dr[i],HEX);
    Serial.write(' ');}
  Serial.println();}

#define MEASURE     0x44
#define READSCRATCH 0xbe

static void readScratchpad(byte* dr, byte* d) {
  ds.reset(); ds.select(dr);
  ds.write(MEASURE, 1);
  delay(1000);
  ds.reset(); ds.select(dr);
  ds.write(READSCRATCH);
  for(int i=0; i<9; i++) d[i] = ds.read();}

static unsigned int readTemperature(byte* dr, byte *ok) {
  unsigned int raw = 0;
  byte data[9];
  
  readScratchpad(dr, data);
  *ok = 0;
  if(OneWire::crc8(data, 8) != data[8]) return raw;
  *ok = 1;
  raw = (data[1] << 8) | data[0];
  if(hiresDevice(dr)) {
    raw <<= 3;			// 9 bit resolution default
    if (data[7] == 0x10) {
      // count remain gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];}
    return raw;}
  return raw << 3 - ((data[4]>>5) & 0x3);}
    
/* format the time into string s */
static void formatCurrentTime(char* s) {
  DateTime n = RTC.now();
  sprintf(s, "%04d-%02d-%02d %02d:%02d:%02d %lu",
	  n.year(), n.month(), n.day(),
	  n.hour(), n.minute(), n.second(), n.unixtime());}

static int sign(float x)       {return 2*(x>0.) - 1;}
static int closestInt(float x) {return (int) (x + .5*sign(x));}

/*
   As of 2013-3 and Arduino 1.0.3 I can't seem to use the f format
   with sprintf.
*/
static void formatTemp(uint16_t t, char* s) {
  int cfahr;
  cfahr = closestInt(100.*(32. + 1.8*t/16.)); /* fahrenheit × 100 */
  sprintf(s, "%3d.%02d %x", cfahr/100, sign(cfahr)*cfahr%100, t);}

static void formatFileName(char* s, int k) {
  DateTime n = RTC.now();
  sprintf(s, "%02d%02d%02d_%c.log",
	  n.year() % 100, n.month(), n.day(), 'a'+k);}

static byte createUniqueFilename(char* s) {
  for(int k=0;; k++) {
    formatFileName(s,k);
    if(!SD.exists(s)) return 1;}
  return 0;}

static byte openLogFile() {
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

static byte closeLogFile() {
  if(!file) return 1;		/* log is not open */
  file.close();
  digitalWrite(REDLED, LOW);
  Serial.print("C: ");
  Serial.println(file.name());
  return 0;}

static int logOpen() {return file;}

static void printDateTime() {
  char s[50];
  formatCurrentTime(s);
  Serial.print("c: ");
  Serial.println(s);}

static void adjustClock(char* b) {
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
  printDateTime();
  timeToMeasure = now() + delta;}
    
static void setDelta(char* b) {
  sscanf(b, "d%d", &delta);
  Serial.print("delta is ");
  Serial.println(delta);}

static int readSerial(cbuf* b) {
  char c;
  while(!bufferFull(b) && Serial.available() > 0) {
    c = Serial.read();
    if(c == '\r' || c == '\n') {
      b->buf[b->n] = 0;
      return 1;}
    b->buf[b->n++] = c;}
  return 0;}

static int dateSpec(char* s) {
  return (8 <= strlen(s) && strlen(s) <= 10 &&
	  s[4] == '-' && (s[6] == '-' || s[7] == '-'));}

static int timeSpec(char* s) {
  return strlen(s) >= 5 && s[2] == ':';}

static void runCommand(cbuf* b) {
  if(dateSpec(b->buf) || timeSpec(b->buf)) adjustClock(b->buf);
  if(b->buf[0] == 'd') setDelta(b->buf);
  if(b->buf[0] == 'o') openLogFile();
  if(b->buf[0] == 'c') closeLogFile();
  if(b->buf[0] == 'r') printDateTime();
  b->n      = 0;
  b->buf[0] = 0;}

static void handleButton() {file ? closeLogFile() : openLogFile();}

/*
  Return true if we detect that the button has transitioned from
  unpressed to pressed.
*/
static int buttonPress() {
  if(!digitalRead(BUTTON)) {
    buttonWasPressed = 0;
    return 0;}
  if(buttonWasPressed) return 0; /* already knew about press */
  for(int i=0; i<10; i++) {
    delay(10);
    buttonWasPressed = digitalRead(BUTTON);
    if(!buttonWasPressed) return 0;}
  return 1;}

static void pause(cbuf* b) {
  digitalWrite(GREENLED, HIGH);
  for(;;) {
    if(now() >= timeToMeasure) {
      timeToMeasure += delta;
      break;}
    if(readSerial(b)) runCommand(b);
    if(buttonPress()) handleButton();}
  digitalWrite(GREENLED, LOW);}

static byte createInfo(char* s, byte* dr) {
  byte ok = 0;
  unsigned int t;
  char tempString[50];

  t = readTemperature(dr, &ok);
  if(ok) {
    formatCurrentTime(s);
    formatTemp(t, tempString);
    strcat(s, " ");
    strcat(s, tempString);
    strcat(s, "\n");}
  return ok;}
  
// ----- MAIN -----
void setup () {
  byte dsRegisters[8];
  cbuf inBuffer;
  Serial.begin(57600);
  pinMode(BUTTON,   INPUT);
  pinMode(REDLED,   OUTPUT);
  pinMode(GREENLED, OUTPUT);
  initializeRtc();
  initializeSd();
  initializeThermistor(dsRegisters);
  inBuffer.n = 0;
  for(;;) myloop(&inBuffer, dsRegisters);}

void myloop (cbuf* b, byte* dr) {
  char infoString[50];
  pause(b);
  if(!createInfo(infoString, dr)) return;
  Serial.print(logOpen() ? "T: " : "t: ");
  Serial.print(infoString);
  if(logOpen())
    file.write((uint8_t*) infoString, strlen(infoString));}

void loop() {}