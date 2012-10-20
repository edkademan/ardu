// simple data logger
//
// using Arduino Uno and an Adafruit Logger Shield
//
// uses Adafruit's RTClib

#include <Wire.h>
#include <OneWire.h>
#include <SD.h>
#include "RTClib.h"

// Pin setup
const int buttonPin = 2;
const int ledRedPin = 3;
const int ledGreenPin = 4;
const int oneWirePin = 7;
const int chipSelect = 10;    

// ----- GLOBALS -----
RTC_DS1307 RTC;
OneWire    ds(oneWirePin);
byte       ds_addr[8];
byte       ds_hires;

const byte ser_max = 20;
byte       ser_pos = 0;
byte       ser_buf[ser_max];

uint16_t   delta = 10; // measurement delta in seconds
uint32_t   next_ts;
DateTime   now;

byte       but_state = 1;
byte       but_active;
uint16_t   but_count;
const uint16_t but_max = 10;

static uint16_t year;
static uint8_t month, day, hour, minute, second;

File file;
byte log_is_open;

//              0         1         2         3         4
// offsets:     01234567890123456789012345678901234567890
byte  line[] = "yyyy.mm.dd HH:MM:SS +000.00 00000000 0000\n";
const int short_line_pos = 19;

// offsets:    0123456789012
byte name[] = "yymmdd_x.log";

// ----- FUNCTIONS -----
// fatal error - blink led and wait for reset
static void fail() {
  while(1) {
    digitalWrite(ledRedPin, HIGH);
    delay(200);
    digitalWrite(ledRedPin, LOW);
    delay(200);
  }
}

inline void led_red(int on)
{
  digitalWrite(ledRedPin, on ? HIGH : LOW);
}

inline void led_green(int on)
{
  digitalWrite(ledGreenPin, on ? HIGH : LOW);
}

static void rtc_init()
{
  // RTC
  Wire.begin();
  RTC.begin();

  // check RTC
  if (!RTC.isrunning()) {
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }
  
  // get value for first measurement
  now = RTC.now();
  next_ts = now.unixtime();
}  

static void sd_init()
{
  pinMode(10, OUTPUT);
  if (!SD.begin(chipSelect)) {
    Serial.println("FATAL: SD");
    fail();
  }
}

static void temp_init()
{
  // search adapter
  ds.reset_search();
  if(!ds.search(ds_addr)) {
    Serial.println("FATAL: ADDR?");
    fail();
  }
 
  // check CRC 
  if (OneWire::crc8(ds_addr, 7) != ds_addr[7]) {
    Serial.println("FATAL: ACRC?");
    fail();
  }

  // write addr
  switch (ds_addr[0]) {
    case 0x10:
      Serial.print("DS18S20: ");
      ds_hires = 1;
      break;
    case 0x28:
      Serial.print("DS18B20: ");
      ds_hires = 0;
      break;
    case 0x22:
      Serial.print("DS1822: ");
      ds_hires = 0;
      break;
    default:
      Serial.print("FATAL: DEV?");
      fail();
  } 
  for(int i=0;i<7;i++) {
    Serial.print(ds_addr[i],HEX);
    Serial.write(' ');
  }
  Serial.println();
}

static unsigned int temp_read(byte *ok)
{
  byte data[9];
  
  ds.reset();
  ds.select(ds_addr);
  ds.write(0x44,1); // start measurement
  
  delay(500);
  
  byte present = ds.reset();
  ds.select(ds_addr);
  ds.write(0xbe); // read scratchpad
  
  for(int i=0;i<9;i++) {
    data[i] = ds.read();
  }
  
  byte crc = OneWire::crc8(data, 8);
  if(crc != data[8]) {
    *ok = 0;
    return 0;
  }
  
  unsigned int raw = (data[1] << 8) | data[0];
  if (ds_hires) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // count remain gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    if (cfg == 0x00) raw = raw << 3;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw << 2; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw << 1; // 11 bit res, 375 ms
    // default is 12 bit resolution, 750 ms conversion time
  }
  *ok = 1;
  return raw;
}

static uint16_t parse_dec(byte *buf, byte num)
{
  uint16_t val = 0;
  for(byte i=0;i<num;i++) {
    val = val * 10;
    if((buf[i] >= '0')&&(buf[i] <= '9')) {
      byte v = buf[i] - '0';
      val += v;
    } else {
      return 0xffff;
    }
  }
  return val;
}

static void to_dec(uint16_t val, byte *buf, byte num)
{
  byte *ptr = buf + num - 1;
  for(byte i=0;i<num;i++) {
    byte r = val % 10;
    val = val / 10;
    *(ptr--) = '0' + r;
  }
}

static void to_hex(uint32_t val, byte *buf, byte num)
{
  byte *ptr = buf + num - 1;
  for(byte i=0;i<num;i++) {
    byte r = val & 0xf;
    val >>= 4;
    if(r < 10) {
      *(ptr--) = '0' + r;
    } else {
      *(ptr--) = 'A' + (r - 10);
    }
  }
}

static void store_time()
{
  to_dec(now.year(),line,4);
  to_dec(now.month(),line+5,2);
  to_dec(now.day(),line+8,2);
  to_dec(now.hour(),line+11,2);
  to_dec(now.minute(),line+14,2);
  to_dec(now.second(),line+17,2);
  
  uint32_t ts = now.unixtime();
  to_hex(ts,line+28,8);
}

static void store_temp(uint16_t t)
{
  to_hex(t,line+37,4);
  
  uint16_t cel = t >> 4;
  to_dec(cel,line+21,3);
  uint16_t mil = (cel & 0xf) * 100 / 16;
  to_dec(mil,line+25,2);
}

static void store_name()
{
  to_dec(now.year(),name,2);
  to_dec(now.month(),name+2,2);
  to_dec(now.day(),name+4,2);
}

static byte log_find_name()
{
  store_name();
  for(int i=0;i<26;i++) {
    name[7] = 'a' + i;
    if(!SD.exists((char *)name)) {
      return 1;
    }
  }
  return 0;
}

static byte log_open()
{
  // log already open!
  if(file) {
    return 1;
  }
  // try to find name
  if(!log_find_name()) {
    return 2;
  }
  file = SD.open((char *)name, FILE_WRITE);
  if(!file) {
    return 3;
  }
  // ok, log is open
  led_red(1);
  log_is_open = 1;
  Serial.print("O: ");
  Serial.println((char *)name);
  return 0;
}

static byte log_close()
{
  // log is not open!
  if(!file) {
    return 1;
  }
  file.close();
  // ok, log is closed
  log_is_open = 0;
  led_red(0);
  Serial.print("C: ");
  Serial.println((char *)name);
  return 0;
}

static void store_byte_val(byte v, byte cmd)
{
  switch(cmd) {
    case 'm': month = v; break;
    case 'd': day = v; break;
    case 'H': hour = v; break;
    case 'M': minute = v; break;
    case 'S': second = v; break;
  }
}

static void store_word_val(uint16_t v, byte cmd)
{
  switch(cmd) {
    case 'y': year = v; break;
    case 'D': delta = v; break;
  }
}

static void adjust_clock()
{
  DateTime set(year,month,day,hour,minute,second);
  RTC.adjust(set);

  // check new time
  now = RTC.now();
  uint32_t ts = now.unixtime();
  
  store_time();
  Serial.print((char *)line);

  // update next time
  next_ts = ts + delta;
}

const byte ERR_CMD = 9;
const byte ERR_SYNTAX = 8;
const byte OK = 0;

static byte handle_command()
{
  byte n = ser_pos;
  uint16_t v;
  switch(ser_buf[0]) {
    case 'y': // set year
    case 'D': // set delta
      if(n==5) {
        v = parse_dec(ser_buf+1,4);
        if(v==0xffff) {
          return ERR_SYNTAX;
        }
        Serial.write(ser_buf[0]);
        Serial.println(v);
        store_word_val(v,ser_buf[0]);
      } else {
        return ERR_SYNTAX;
      }
      break;
    case 'm': // set month
    case 'd': // set day
    case 'H': // set HOUR
    case 'M': // set MINUTE
    case 'S': // set SECOND
      if(n==3) {
        v = parse_dec(ser_buf+1,2);
        if(v==0xffff) {
          return ERR_SYNTAX;
        }
        Serial.write(ser_buf[0]);
        Serial.println(v);
        store_byte_val((byte)v,ser_buf[0]);
      } else {
        return ERR_SYNTAX;
      }
      break;
    case 'a': // adjust clock
      adjust_clock();
      break;
    case 'o': // open log
      return log_open();
    case 'c': // close log
      return log_close();
    default:
      return ERR_CMD;
  }
  return OK;
}

static void serial_in()
{
  // read serial
  if(Serial.available()) {
    byte d = Serial.read();
    // end of line -> handle command
    if((d == '\r')||(d == '\n')) {
      if(ser_pos > 0) {
        ser_buf[ser_pos] = 0;
        
        // execute command
        byte err = handle_command();
        if(err == 0) {
          Serial.println("OK");
        } else {
          Serial.print("ERR");
          Serial.print(err);
          Serial.println();
        }
        
        ser_pos = 0;
      }
    } 
    // normal char -> add to buffer
    else {
      if(ser_pos < ser_max) {
        ser_buf[ser_pos++] = d;
      }
    }
  }    
}   

static void handle_button(uint32_t ts)
{
  if(file) {
    log_close();
  } else {
    log_open();
  }
}

static byte button_in(uint32_t ts)
{
  byte b = digitalRead(buttonPin);
  // other state?
  if(b != but_state) {
    but_count = 0;
    but_active = 1;
  } else if(but_active) {
    but_count ++;
    if(but_count == but_max) {
      but_state = 1 - but_state;
      if(!but_state) {
        handle_button(ts);
      }
      but_active = 0;
    }
  }
}


// ----- MAIN -----
void setup () {
  // setup serial
  Serial.begin(57600);
  
  // button & leds
  pinMode(buttonPin, INPUT);
  pinMode(ledRedPin, OUTPUT);
  pinMode(ledGreenPin, OUTPUT);
  
  rtc_init();
  sd_init();
  temp_init();
}

void loop () {
  uint32_t ts;
  
  // wait for next measurement
  led_green(1);
  while(1) {
    // check time
    now = RTC.now();
    ts = now.unixtime();
    if(ts >= next_ts) {
      next_ts += delta;
      break;
    }

    // input command?
    serial_in();
    button_in(ts);
        
    delay(10); // sampling rate of button
  }  
  led_green(0);
  
  // do measurement
  now = RTC.now();
  byte ok = 0;
  unsigned int t = temp_read(&ok);
  if(ok) {
    store_time();
    store_temp(t);
    
    // write to console
    if(log_is_open) {
      Serial.print("T: ");
    } else {
      Serial.print("t: ");
    }
    Serial.print((char *)line); // has a new line
    
    // write to log file
    if(log_is_open) {
      if(file) {
        led_red(0);
        size_t len = file.write(line, sizeof(line)-1);
        if(len != (sizeof(line)-1) {
          Serial.println("WERR!");
          log_close();
        } else {
          led_red(1);
        }
      }
    }
  }
}
