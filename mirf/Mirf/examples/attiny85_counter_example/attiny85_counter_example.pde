/**
 * A Mirf example test sending and receiving data between
 * Ardunio (running as server) and ATTiny85 (running as client).
 *
 * This uses the SPI85 class from: https://github.com/stanleyseow/attiny-nRF24L01/tree/master/libraries/SPI85
 *
 * Pins:
 * Hardware SPI:
 * MISO -> PB0
 * MOSI -> PB1
 * SCK -> PB2
 *
 * (Configurable):
 * CE -> PB4
 * CSN -> PB3
 */

#include <SPI85.h>
#include <Mirf.h>
#include <nRF24L01.h>
#include <MirfHardwareSpi85Driver.h>

void setup() {
  /*
   * Setup pins / SPI.
   */

  Mirf.cePin = PB4;
  Mirf.csnPin = PB3;

  Mirf.spi = &MirfHardwareSpi85;
  Mirf.init();
  
  /*
   * Configure reciving address.
   */
   
  Mirf.setRADDR((byte *)"clie1");
  
  /*
   * Set the payload length to sizeof(unsigned long) the
   * return type of millis().
   *
   * NB: payload on client and server must be the same.
   */
   
  Mirf.payload = sizeof(unsigned long);
  
  /*
   * Write channel and payload config then power up reciver.
   */
   
  /*
   * To change channel:
   * 
   * Mirf.channel = 10;
   *
   * NB: Make sure channel is legal in your area.
   */
   
  Mirf.config();
}

void loop() {
  static unsigned long counter = 0;
  unsigned long time = millis();
  
  Mirf.setTADDR((byte *)"serv1");
  
  Mirf.send((byte *)&counter);

  while (Mirf.isSending()) {
    if ((millis() - time) > 1000) {
      delay(1000);
      return;
    }
  }

  delay(10);

  while (!Mirf.dataReady()){
    if ((millis() - time) > 1000) {
      delay(1000);
      return;
    }
  }

  unsigned long recv;
  Mirf.getData((byte *) &recv);

  counter = recv + 1;

  delay(500);
} 
  
  
  
