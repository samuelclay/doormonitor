#include <Spi.h>
#include <mirf.h>

// converts a float into a char 
// and sends it via nRF24L01
void transmit( float v)
{
  byte c; 
  char buf[10];
  
  dtostrf(v,9,3,buf);

  for( int i=0 ; i<8 ; i++ )
  { 
    c = buf[i];
    Mirf.send(&c);
    while( Mirf.isSending() ) ;
  }
}

// sends a string via the nRF24L01
void transmit(const char *string)
{
  byte c; 
  
  for( int i=0 ; string[i]!=0x00 ; i++ )
  { 
    c = string[i];
    Mirf.send(&c);
    while( Mirf.isSending() ) ;
  }
}

// send a CR/LF sequence via the nRF24L01
void transmitlf(void)
{
  byte c;
  
  c = '\r';
  Mirf.send(&c);
    while( Mirf.isSending() ) ;
  
  c = '\n';
  Mirf.send(&c);
    while( Mirf.isSending() ) ;
}

void setup()
{
  
  // init the transceiver
  Mirf.init();
  
  // we transmit only a single byte each time
  Mirf.payload = 1;
  
  // selecting a channel which is not too noisy
  Mirf.channel = 90;
  Mirf.config();
 
  // Set 1MHz data rate
  Mirf.configRegister(RF_SETUP,0x06);
  
  // Set address - this one must match the 
  // address the receiver is using!
  Mirf.setTADDR((byte *)"TX_01");
}

void loop() 
{
  float v01,v02; 
  
  // read in some values
  v01 = analogRead(0);
  v02 = analogRead(1);
  
  // transmit a fixed token
  transmit(" : ");
  
  // transmit the first value
  transmit(v01);
  
  // transmit a separator
  transmit(" : ");
  
  // transmit a second token
  transmit(v02);
  
  // transmit a CR/LF for the PC
  // software to sync to
  transmitlf();
 
  // ... just take your time
  delay(100);
}
