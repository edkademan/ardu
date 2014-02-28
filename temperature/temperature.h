#define MAXBUFFER 50

enum sunday
  {firstSunday, secondSunday, thirdSunday, fourthSunday, lastSunday};

typedef struct {
  int n;			/* number of characters in buffer */
  char buf[MAXBUFFER];
} cbuf;

typedef struct {
  int pin;
  char description[MAXBUFFER];
  DallasTemperature* ds;
} therm;

typedef struct {
  int mon;
  sunday sun;
} dstdef;
