/*****************************************************************************
//  File Name    : wiznetweb.c
//  Version      : 1.0
//  Description  : AVRJazz Mega328 and Wiznet W5100 Web Server
//  Author       : RWB
//  Target       : AVRJazz Mega328 Board
//  Compiler     : AVR-GCC 4.3.2; avr-libc 1.6.6 (WinAVR 20090313)
//  IDE          : Atmel AVR Studio 4.17
//  Programmer   : AVRJazz Mega328 STK500 v2.0 Bootloader
//               : AVR Visual Studio 4.17, STK500 programmer
//  Last Updated : 20 July 2010
*****************************************************************************/
#include <avr/io.h>
#include <string.h>
#include <stdio.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#define byte uint8_t

// AVRJazz Mega328 SPI I/O
#define SPI_PORT PORTB
#define SPI_DDR  DDRB
#define SPI_CS   PORTB2
// Wiznet W5100 Op Code
#define WIZNET_WRITE_OPCODE 0xF0
#define WIZNET_READ_OPCODE 0x0F
// Wiznet W5100 Register Addresses
#define MR         0x0000      // Mode Register
#define GAR        0x0001      // Gateway Address: 0x0001 to 0x0004
#define SUBR       0x0005      // Subnet mask Address: 0x0005 to 0x0008
#define SAR        0x0009      // Source Hardware Address (MAC): 0x0009 to 0x000E
#define SIPR       0x000F      // Source IP Address: 0x000F to 0x0012
#define RMSR       0x001A      // RX Memory Size Register
#define TMSR       0x001B      // TX Memory Size Register
#define S0_MR		   0x0400      // Socket 0: Mode Register Address
#define S0_CR		   0x0401      // Socket 0: Command Register Address
#define S0_IR		   0x0402      // Socket 0: Interrupt Register Address
#define S0_SR		   0x0403      // Socket 0: Status Register Address
#define S0_PORT    0x0404      // Socket 0: Source Port: 0x0404 to 0x0405
#define SO_TX_FSR  0x0420      // Socket 0: Tx Free Size Register: 0x0420 to 0x0421
#define S0_TX_RD   0x0422      // Socket 0: Tx Read Pointer Register: 0x0422 to 0x0423
#define S0_TX_WR   0x0424      // Socket 0: Tx Write Pointer Register: 0x0424 to 0x0425
#define S0_RX_RSR  0x0426      // Socket 0: Rx Received Size Pointer Register: 0x0425 to 0x0427
#define S0_RX_RD   0x0428      // Socket 0: Rx Read Pointer: 0x0428 to 0x0429
#define TXBUFADDR  0x4000      // W5100 Send Buffer Base Address
#define RXBUFADDR  0x6000      // W5100 Read Buffer Base Address
// S0_MR values
#define MR_CLOSE	  0x00    // Unused socket
#define MR_TCP		  0x01    // TCP
#define MR_UDP		  0x02    // UDP
#define MR_IPRAW	  0x03	  // IP LAYER RAW SOCK
#define MR_MACRAW	  0x04	  // MAC LAYER RAW SOCK
#define MR_PPPOE	  0x05	  // PPPoE
#define MR_ND			  0x20	  // No Delayed Ack(TCP) flag
#define MR_MULTI	  0x80	  // support multicating
// S0_CR values
#define CR_OPEN          0x01	  // Initialize or open socket
#define CR_LISTEN        0x02	  // Wait connection request in tcp mode(Server mode)
#define CR_CONNECT       0x04	  // Send connection request in tcp mode(Client mode)
#define CR_DISCON        0x08	  // Send closing reqeuset in tcp mode
#define CR_CLOSE         0x10	  // Close socket
#define CR_SEND          0x20	  // Update Tx memory pointer and send data
#define CR_SEND_MAC      0x21	  // Send data with MAC address, so without ARP process
#define CR_SEND_KEEP     0x22	  // Send keep alive message
#define CR_RECV          0x40	  // Update Rx memory buffer pointer and receive data
// S0_SR values
#define SOCK_CLOSED      0x00     // Closed
#define SOCK_INIT        0x13	  // Init state
#define SOCK_LISTEN      0x14	  // Listen state
#define SOCK_SYNSENT     0x15	  // Connection state
#define SOCK_SYNRECV     0x16	  // Connection state
#define SOCK_ESTABLISHED 0x17	  // Success to connect
#define SOCK_FIN_WAIT    0x18	  // Closing state
#define SOCK_CLOSING     0x1A	  // Closing state
#define SOCK_TIME_WAIT	 0x1B	  // Closing state
#define SOCK_CLOSE_WAIT  0x1C	  // Closing state
#define SOCK_LAST_ACK    0x1D	  // Closing state
#define SOCK_UDP         0x22	  // UDP socket
#define SOCK_IPRAW       0x32	  // IP raw mode socket
#define SOCK_MACRAW      0x42	  // MAC raw mode socket
#define SOCK_PPPOE       0x5F	  // PPPOE socket
#define TX_BUF_MASK      0x07FF   // Tx 2K Buffer Mask:
#define RX_BUF_MASK      0x07FF   // Rx 2K Buffer Mask:
#define NET_MEMALLOC     0x05     // Use 2K of Tx/Rx Buffer
#define TCP_PORT         80       // TCP/IP Port

// Define W5100 Socket Register and Variables Used
uint8_t sockreg;
#define MAX_BUF 1024
uint8_t buf[MAX_BUF];
uint16_t CODE_BUFFER_SIZE;

void SPI_Write(uint16_t addr,uint8_t data)
{
  // Activate the CS pin
  SPI_PORT &= ~(1<<SPI_CS);
  // Start Wiznet W5100 Write OpCode transmission
  SPDR = WIZNET_WRITE_OPCODE;
  // Wait for transmission complete
  while(!(SPSR & (1<<SPIF)));
  // Start Wiznet W5100 Address High Bytes transmission
  SPDR = (addr & 0xFF00) >> 8;
  // Wait for transmission complete
  while(!(SPSR & (1<<SPIF)));
  // Start Wiznet W5100 Address Low Bytes transmission
  SPDR = addr & 0x00FF;
  // Wait for transmission complete
  while(!(SPSR & (1<<SPIF)));   

  // Start Data transmission
  SPDR = data;
  // Wait for transmission complete
  while(!(SPSR & (1<<SPIF)));
  // CS pin is not active
  SPI_PORT |= (1<<SPI_CS);
}

unsigned char SPI_Read(uint16_t addr)
{
  // Activate the CS pin
  SPI_PORT &= ~(1<<SPI_CS);
  // Start Wiznet W5100 Read OpCode transmission
  SPDR = WIZNET_READ_OPCODE;
  // Wait for transmission complete
  while(!(SPSR & (1<<SPIF)));
  // Start Wiznet W5100 Address High Bytes transmission
  SPDR = (addr & 0xFF00) >> 8;
  // Wait for transmission complete
  while(!(SPSR & (1<<SPIF)));
  // Start Wiznet W5100 Address Low Bytes transmission
  SPDR = addr & 0x00FF;
  // Wait for transmission complete
  while(!(SPSR & (1<<SPIF)));   

  // Send Dummy transmission for reading the data
  SPDR = 0x00;
  // Wait for transmission complete
  while(!(SPSR & (1<<SPIF)));  

  // CS pin is not active
  SPI_PORT |= (1<<SPI_CS);
  return(SPDR);
}

void W5100_Init(void)
{
  // Ethernet Setup
  unsigned char mac_addr[] = {0x00,0x16,0x36,0xDE,0x58,0xF6};
  unsigned char ip_addr[] = {192,168,2,10};
  unsigned char sub_mask[] = {255,255,255,0};
  unsigned char gtw_addr[] = {192,168,2,1};
  // Setting the Wiznet W5100 Mode Register: 0x0000
  SPI_Write(MR,0x80);            // MR = 0b10000000;
  // Setting the Wiznet W5100 Gateway Address (GAR): 0x0001 to 0x0004
  SPI_Write(GAR + 0,gtw_addr[0]);
  SPI_Write(GAR + 1,gtw_addr[1]);
  SPI_Write(GAR + 2,gtw_addr[2]);
  SPI_Write(GAR + 3,gtw_addr[3]);
  // Setting the Wiznet W5100 Source Address Register (SAR): 0x0009 to 0x000E
  SPI_Write(SAR + 0,mac_addr[0]);
  SPI_Write(SAR + 1,mac_addr[1]);
  SPI_Write(SAR + 2,mac_addr[2]);
  SPI_Write(SAR + 3,mac_addr[3]);
  SPI_Write(SAR + 4,mac_addr[4]);
  SPI_Write(SAR + 5,mac_addr[5]);
  // Setting the Wiznet W5100 Sub Mask Address (SUBR): 0x0005 to 0x0008
  SPI_Write(SUBR + 0,sub_mask[0]);
  SPI_Write(SUBR + 1,sub_mask[1]);
  SPI_Write(SUBR + 2,sub_mask[2]);
  SPI_Write(SUBR + 3,sub_mask[3]);
  // Setting the Wiznet W5100 IP Address (SIPR): 0x000F to 0x0012
  SPI_Write(SIPR + 0,ip_addr[0]);
  SPI_Write(SIPR + 1,ip_addr[1]);
  SPI_Write(SIPR + 2,ip_addr[2]);
  SPI_Write(SIPR + 3,ip_addr[3]);    

  // Setting the Wiznet W5100 RX and TX Memory Size (2KB),
  SPI_Write(RMSR,NET_MEMALLOC);
  SPI_Write(TMSR,NET_MEMALLOC);
}

void close(uint8_t sock)
{
   if (sock != 0) return;

   // Send Close Command
   SPI_Write(S0_CR,CR_CLOSE);
   // Waiting until the S0_CR is clear
   while(SPI_Read(S0_CR));
}

void disconnect(uint8_t sock)
{
   if (sock != 0) return;

   // Send Disconnect Command
   SPI_Write(S0_CR,CR_DISCON);
   // Wait for Disconecting Process
   while(SPI_Read(S0_CR));
}

uint8_t socket(uint8_t sock,uint8_t eth_protocol,uint16_t tcp_port)
{
    uint8_t retval=0;
    if (sock != 0) return retval;

    // Make sure we close the socket first
    if (SPI_Read(S0_SR) == SOCK_CLOSED) {
      close(sock);
    }
    // Assigned Socket 0 Mode Register
    SPI_Write(S0_MR,eth_protocol);

    // Now open the Socket 0
    SPI_Write(S0_PORT,((tcp_port & 0xFF00) >> 8 ));
    SPI_Write(S0_PORT + 1,(tcp_port & 0x00FF));
    SPI_Write(S0_CR,CR_OPEN);                   // Open Socket
    // Wait for Opening Process
    while(SPI_Read(S0_CR));
    // Check for Init Status
    if (SPI_Read(S0_SR) == SOCK_INIT)
      retval=1;
    else
      close(sock);

    return retval;
}

uint8_t listen(uint8_t sock)
{
   uint8_t retval = 0;
   if (sock != 0) return retval;
   if (SPI_Read(S0_SR) == SOCK_INIT) {
     // Send the LISTEN Command
     SPI_Write(S0_CR,CR_LISTEN);

     // Wait for Listening Process
     while(SPI_Read(S0_CR));
     // Check for Listen Status
     if (SPI_Read(S0_SR) == SOCK_LISTEN)
       retval=1;
     else
       close(sock);
    }
    return retval;
}

uint16_t send_pack(uint8_t sock,const uint8_t *buf,uint16_t buflen)
{
   uint16_t ptr,offaddr,realaddr,txsize,timeout;   

   if (buflen <= 0 || sock != 0) return 0;

   // Make sure the TX Free Size Register is available
   txsize=SPI_Read(SO_TX_FSR);
   txsize=(((txsize & 0x00FF) << 8 ) + SPI_Read(SO_TX_FSR + 1));

   timeout=0;
   while (txsize < buflen) {
      _delay_ms(1);
     txsize=SPI_Read(SO_TX_FSR);
     txsize=(((txsize & 0x00FF) << 8 ) + SPI_Read(SO_TX_FSR + 1));

     // Timeout for approx 5000 ms
     if (timeout++ > 5000) {
       // Disconnect the connection
       disconnect(sock);
       return 0;
     }
   }	

   // Read the Tx Write Pointer
   ptr = SPI_Read(S0_TX_WR);
   offaddr = (((ptr & 0x00FF) << 8 ) + SPI_Read(S0_TX_WR + 1));

   while(buflen) {
     buflen--;
     // Calculate the real W5100 physical Tx Buffer Address
     realaddr = TXBUFADDR + (offaddr & TX_BUF_MASK);
     // Copy the application data to the W5100 Tx Buffer
     SPI_Write(realaddr,*buf);
     offaddr++;
     buf++;
   }

   // Increase the S0_TX_WR value, so it point to the next transmit
   SPI_Write(S0_TX_WR,(offaddr & 0xFF00) >> 8 );
   SPI_Write(S0_TX_WR + 1,(offaddr & 0x00FF));	

   // Now Send the SEND command
   SPI_Write(S0_CR,CR_SEND);

   // Wait for Sending Process
   while(SPI_Read(S0_CR));	

   return 1;
}

uint16_t recv(uint8_t sock,uint8_t *buf,uint16_t buflen)
{
    uint16_t ptr,offaddr,realaddr;   	

    if (buflen <= 0 || sock != 0) return 1;   

    // If the request size > MAX_BUF,just truncate it
    if (buflen > MAX_BUF - 1)
      buflen=MAX_BUF - 2;
    // Read the Rx Read Pointer
    ptr = SPI_Read(S0_RX_RD);
    offaddr = (((ptr & 0x00FF) << 8 ) + SPI_Read(S0_RX_RD + 1));

    while(buflen) {
      buflen--;
      realaddr=RXBUFADDR + (offaddr & RX_BUF_MASK);
      *buf = SPI_Read(realaddr);
      offaddr++;
      buf++;
    }
    *buf='\0';        // String terminated character

    // Increase the S0_RX_RD value, so it point to the next receive
    SPI_Write(S0_RX_RD,(offaddr & 0xFF00) >> 8 );
    SPI_Write(S0_RX_RD + 1,(offaddr & 0x00FF));	

    // Now Send the RECV command
    SPI_Write(S0_CR,CR_RECV);
    _delay_us(5);    // Wait for Receive Process

    return 1;
}

uint16_t recv_size(void)
{
  return ((SPI_Read(S0_RX_RSR) & 0x00FF) << 8 ) + SPI_Read(S0_RX_RSR + 1);
}

int strindex(char *s,char *t)
{
  uint16_t i,n;

  n=strlen(t);
  for(i=0;*(s+i); i++) {
    if (strncmp(s+i,t,n) == 0)
      return i;
  }
  return -1;
}

char* post_value(char* field, char* header)
{
	char* ch_ptr;
	char* old;
	
	old = NULL;

	ch_ptr = strtok(header, "\n");
	while(ch_ptr != NULL)
	{
		old = ch_ptr;
		ch_ptr = strtok(NULL, "\n");
	}

	old += strlen(field) + 1;
	return old;
}

uint8_t hex2bin(uint8_t b) {
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
  byte* end = buf + MAX_BUF;
  byte* ptr = buf;
  byte high = 1;
  
  while(*code != 0 && ptr != end) {
    uint8_t b = hex2bin(*code);
    if(0 <= b && b <= 15) {
      if(high) {
        *ptr = b;
        *ptr <<= 4;
        high = !high;
      } else {
        *ptr += b;
        ptr++;
        high = !high;
      }
    }
    code++;
  }

  CODE_BUFFER_SIZE = ptr - buf;
}

int main(void){
  uint8_t sockstat;
  uint16_t rsize;
  char* code;
  int getidx,postidx;
  int is_favicon;

  // Reset Port D
  DDRD = 0xFF;       // Set PORTD as Output
  PORTD = 0x00;	     

  // Initial ATMega386 ADC Peripheral
  ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1);
  // Free running ADC Mode
  ADCSRB = 0x00;
  // Initial the AVR ATMega328 SPI Peripheral
  // Set MOSI (PORTB3),SCK (PORTB5) and PORTB2 (SS) as output, others as input
  SPI_DDR = (1<<PORTB3)|(1<<PORTB5)|(1<<PORTB2);
  // CS pin is not active
  SPI_PORT |= (1<<SPI_CS);
  // Enable SPI, Master Mode 0, set the clock rate fck/2
  SPCR = (1<<SPE)|(1<<MSTR);
  SPSR |= (1<<SPI2X);  

  // Initial ATMega368 Timer/Counter0 Peripheral
  TCCR0A=0x00;                  // Normal Timer0 Operation
  TCCR0B=(1<<CS02)|(1<<CS00);   // Use maximum prescaller: Clk/1024
  TCNT0=0x94;                   // Start counter from 0x94, overflow at 10 mSec
  TIMSK0=(1<<TOIE0);            // Enable Counter Overflow Interrupt
  sei();                        // Enable Interrupt

  // Initial the W5100 Ethernet
  W5100_Init();
  // Initial variable used
  sockreg=0;

  // Loop forever
  for(;;){
    sockstat=SPI_Read(S0_SR);
    switch(sockstat) {
     case SOCK_CLOSED:
        if (socket(sockreg,MR_TCP,TCP_PORT) > 0) {
          // Listen to Socket 0
          if (listen(sockreg) <= 0)
            _delay_ms(1);
        }
        break;
     case SOCK_ESTABLISHED:
        // Get the client request size
        rsize=recv_size();
        if (rsize > 0)
        {
          // Now read the client Request
          if (recv(sockreg,buf,rsize) <= 0) break;
          // Check the Request Header
          getidx=strindex((char *)buf,"GET /");
          postidx=strindex((char *)buf,"POST /");
          
          is_favicon = strindex((char *)buf, "favicon");
          
          if ((getidx >= 0 || postidx >= 0) && is_favicon < 0)
          {            
            char field[] = "code";
            code = post_value(field, (char *)buf);
            
            convert_code(code);
            
            
            // Create the HTTP Response Header
            strcpy_P((char *)buf, PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n"));
            strcat_P((char *)buf, PSTR("<html><body><span style=\"color:#0000A0\">\r\n"));
            strcat_P((char *)buf, PSTR("<h1>OpenRemote</h1>\r\n"));
            strcat_P((char *)buf, PSTR("<h3>Please Enter Pronto Code Below:</h3>\r\n"));
            strcat_P((char *)buf, PSTR("<p><form method=\"POST\">\r\n"));
            // Now Send the HTTP Response
            if (send_pack(sockreg, buf, strlen((char *)buf)) <= 0) break;
            
            strcpy_P((char *)buf, PSTR("Code: <textarea name=\"code\" rows=\"5\" cols=\"30\"></textarea><br />\r\n"));
            strcat_P((char *)buf, PSTR("<input type=\"submit\">\r\n</form>"));
            sprintf((char *)buf+strlen((char *)buf), "%u", CODE_BUFFER_SIZE);
            strcat_P((char *)buf, PSTR("</p></span></body></html>\r\n"));
            // Now Send the HTTP Remaining Response
            if (send_pack(sockreg, buf, strlen((char *)buf)) <= 0) break;
          }
          // Disconnect the socket
          disconnect(sockreg);
        } else {
          _delay_us(1000);    // Wait for request
        }
        break;
      case SOCK_FIN_WAIT:
      case SOCK_CLOSING:
      case SOCK_TIME_WAIT:
      case SOCK_CLOSE_WAIT:
      case SOCK_LAST_ACK:
        // Force to close the socket
        close(sockreg);
        break;
    }
  }
}

