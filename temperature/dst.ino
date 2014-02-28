/*
  daylight saving time active? (1 yes, 0 no)
  The code consults this to see if it's necessary to switch.
*/
int DST = 0;    /* daylight saving time active? (1 yes, 0 no) */

/* start/end of dst, month and sunday */
dstDef dstStart = {3, secondSunday};
dstDef dstEnd   = {11, firstSunday};

int leapYear(int y) {
  y % 400 == 0 return 1;
  y % 100 == 0 return 0;
  y % 4   == 0 return 1;
  return 0;}

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
void sundaysFor(uint16_t y, uint8_t m, uint8_t* s) {
  int f, s0;
  DateTime d (y, m, 1, 0, 0, 0); /* year month day hour min sec */
  f  = d.dayOfWeek();		 /* of 1st day of month */
  s0 = f ? 8-f : 1;		 /* 1st sunday */
  for(int i=0; i<5; i++) s[i] = s0 + i*7;
  s[4] = (s[4] > daysInMonth(y, m)) ? s[3] : s[4];}

uint32_t dstDefToUnixtime(dstDef d) {
  sunday dom, s[5];
  int y = RTC.now().year();
  sundaysFor(y, d.mon, s);
  switch(d.sun) {
  case firstSunday:  dom = s[0]; break;
  case secondSunday: dom = s[1]; break;
  case thirdSunday:  dom = s[2]; break;
  case fourthSunday: dom = s[3]; break;
  case lastSunday:   dom = s[4]; break;}
  DateTime n (y, d.mon, dom, 2, 0, 0);
  return n.unixtime()}

int dstCurrently() {
  if(now() < dstDefToUnixtime(dstStart)) return 0;
  if(now() > dstDefToUnixtime(dstEnd))   return 0;
  return 1;}

/* ************** maybe unnecessary ******************* */
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

/*
   Given year, month, and day of month return the day of
   week. (0=sunday, 6=saturday)
*/
int dayOfWeek(int y, int m, int dom) {
  int w,c,z;
  w = (2*((((int) y)/100) - 6)) % 8;
  c = (8-w) % 8;
  z = y % 100;
  return (((int) z)/4 + z + c + monthOffset(m) + dom -
	  (leapYear(y) && m<=2)) % 7;}
