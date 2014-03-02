#define MAXBUFFER 50

typedef struct {
  int n;			/* number of characters in buffer */
  char buf[MAXBUFFER];
} cbuf;

typedef struct {
  int pin;
  char description[MAXBUFFER];
  DallasTemperature* ds;
} therm;
