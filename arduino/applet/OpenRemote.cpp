#include "WProgram.h"
byte hex2bin(byte b);
void convert_code(char* code);
inline void asm_delay(uint16_t x_constant);
uint16_t calc_delay(uint16_t n);
uint16_t roll8(uint16_t a);
void setup();
void loop();
int ledPin = 2;
int delay_count = 2;

// Apple Remote play/pause command
//char* CODE_GLOBAL = "0000006e00240000015600ac00160016001600400016004000160040001600160016004100160041001600410016004100160041001600410016001600160016001600160016001600160041001600400016001600160040001600160016001600160016001600160016001600160016001600400016004000160016001600400016004000160040001600160016058d01560055001600ac";
//char* CODE_GLOBAL = "0000006E00220002015700AC0016001600160041001600410016004100160016001600410016004100160041001600410016004100160041001600160016";
//"0016001600160016001600160041001600160016001600160041001600160016001600160016001600160016001600160041001600160016001600160041"
//"00160041001600160016001600160041001605E70157005500160E48";

char* CODE_GLOBAL = "0000006c00500000000a0046000a001e000a0046000a0046000a001e000a001e000a0046000a0046000a001e000a0046000a001e000a0046000a001e000a0046000a001e000a0660000a0046000a001e000a0046000a0046000a001e000a0046000a001e000a001e000a0046000a001e000a0046000a001e000a0046000a001e000a0046000a0660000a0046000a001e000a0046000a0046000a001e000a001e000a0046000a0046000a001e000a0046000a001e000a0046000a001e000a0046000a001e000a0660000a0046000a001e000a0046000a0046000a001e000a0046000a001e000a001e000a0046000a001e000a0046000a001e000a0046000a001e000a0046000a0660000a0046000a001e000a0046000a0046000a001e000a001e000a0046000a0046000a001e000a0046000a001e000a0046000a001e000a0046000a001e000a000a";

#define CODE_BUFFER_MAX 400
byte CODE_BUFFER[CODE_BUFFER_MAX];
uint16_t *WORD_BUFFER = (uint16_t*)CODE_BUFFER;
uint16_t CODE_BUFFER_SIZE;

byte hex2bin(byte b) {
  b -= '0';
  if(b > 9) {
    b += '0'-'A'+10;
  }
  if(b > 15) {
    b += 'A'-'a';
  }
  return b;
}

void convert_code(char* code) {
  //byte* end = CODE_BUFFER + CODE_BUFFER_SIZE;
  byte* end = CODE_BUFFER + CODE_BUFFER_MAX;
  byte* ptr = CODE_BUFFER;
  boolean high = true;
  
  /*char buffer[30];
  sprintf(buffer, "END: %p", end);
  Serial.println(buffer);
  
  char buffer2[30];
  sprintf(buffer2, "PTR: %p", ptr);
  Serial.println(buffer2);*/

  while(*code != NULL && ptr != end) {
    if(high) {
      *ptr = hex2bin(*code);
      *ptr <<= 4;
      high = !high;
    } else {
      *ptr += hex2bin(*code);
      ptr++;
      high = !high;
    }
    code++;
  }
  CODE_BUFFER_SIZE = ptr - CODE_BUFFER;
}

inline void asm_delay(uint16_t x_constant)
{
/*  asm volatile(
    "clr R1"         "\n\t"
    "1:"            "\n\t"
    "inc R1"         "\n\t"
    "nop"            "\n\t"
    "brsh 1b"        "\n\t"
  );*/
  
  volatile register int x=0;
  while( x < x_constant ) {
    ++x;
  }
}

uint16_t calc_delay(uint16_t n)
{
  uint16_t ret_val = round(3.86 * n);

  return ret_val;
}

uint16_t roll8(uint16_t a) {
//  asm ("leal (%0,%0,4), %0"
//    : "=r" (a)
//    : "0" (a) 
//  );
  a = a << 8 | a >> 8;
  
  return a;
}

float CARRIER_PERIOD;

void setup()
{  
  PORTD &= ~_BV(2);
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT);
  Serial.println(calc_delay(10));
  convert_code(CODE_GLOBAL);
  uint16_t *word_buff = (uint16_t*)CODE_BUFFER;
  uint16_t cycles = calc_delay(word_buff[1]);
  Serial.println(roll8(WORD_BUFFER[4]), HEX);
  Serial.println(CODE_BUFFER[2],HEX);
  Serial.println(CODE_BUFFER[3],HEX);
  Serial.println(cycles);
  uint16_t N = 0x006E;
  CARRIER_PERIOD = N * .241246 / 1.1321;

  uint16_t BURST_SEQ_ONE_SIZE = roll8(WORD_BUFFER[2]);
  uint16_t BURST_SEQ_TWO_SIZE = roll8(WORD_BUFFER[3]);

  for(uint16_t i=0; i<100; ++i) {
  uint16_t x = 4;
  uint16_t x2 = 5;
  for(; x<BURST_SEQ_ONE_SIZE*2+4;) {
    PORTD |= _BV(2);
    uint16_t dtime = roll8(WORD_BUFFER[x]);
    asm_delay(dtime * CARRIER_PERIOD - 12);

    PORTD &= ~_BV(2);
    dtime = roll8(WORD_BUFFER[x2]);  
    asm_delay(dtime * CARRIER_PERIOD - 12);
    x2 += 2;
    x += 2;
  }
  }
    PORTD &= ~_BV(2);
}

void loop()
{
  //digitalWrite(ledPin, HIGH);
  //uint16_t dtime = 0x0156;
  //PORTD |= _BV(2);
  //uint16_t dtime = roll8(WORD_BUFFER[4]);
  //asm_delay(dtime * CARRIER_PERIOD - 12);
  //PORTD &= ~_BV(2);
  //dtime = roll8(WORD_BUFFER[5]);
  //asm_delay(dtime * CARRIER_PERIOD - 12);
}

int main(void)
{
	init();

	setup();
    
	for (;;)
		loop();
        
	return 0;
}

