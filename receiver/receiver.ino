/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

//
// Hardware configuration
//

// Set up nRF24L01 radio on SPI bus plus pins 9 & 10

RF24 radio(9,10);

// sets the role of this unit in hardware.  Connect to GND to be the 'led' board receiver
// Leave open to be the 'remote' transmitter
const int role_pin = 6;
const int sensor_pin = 3;
const int led_pin = 4;

//
// Topology
//

// Single radio pipe address for the 2 nodes to communicate.
const uint64_t pipe = 0xE8E8F0F0E1LL;

//
// Role management
//
// Set up role.  This sketch uses the same software for all the nodes in this
// system.  Doing so greatly simplifies testing.  The hardware itself specifies
// which node it is.
//
// This is done through the role_pin
//

// The various roles supported by this sketch
typedef enum { role_remote = 1, role_led } role_e;

// The debug-friendly names of those roles
const char* role_friendly_name[] = { "invalid", "Remote", "LED Board"};

// The role of the current running sketch
role_e role;

//
// Payload
//

uint8_t led_state;
uint8_t sensor_state;

//
// Setup
//

void setup(void)
{
  //
  // Role
  //

  // set up the role pin
  pinMode(role_pin, INPUT);
  digitalWrite(role_pin,HIGH);
  delay(20); // Just to get a solid reading on the role pin

  // read the address pin, establish our role
  if ( digitalRead(role_pin) )
    role = role_remote;
  else
    role = role_led;

  //
  // Print preamble
  //

  Serial.begin(57600);
  printf_begin();
  printf("\n\rRF24/examples/led_remote/\n\r");
  printf("ROLE: %s\n\r",role_friendly_name[role]);

  //
  // Setup and configure rf radio
  //

  radio.begin();

  //
  // Open pipes to other nodes for communication
  //

  // This simple sketch opens a single pipes for these two nodes to communicate
  // back and forth.  One listens on it, the other talks to it.

  if ( role == role_remote )
  {
    radio.openWritingPipe(pipe);
  }
  else
  {
    radio.openReadingPipe(1,pipe);
  }

  //
  // Start listening
  //

  if ( role == role_led )
    radio.startListening();

  //
  // Dump the configuration of the rf unit for debugging
  //

  radio.printDetails();

  //
  // Set up buttons / LED's
  //

  // Set pull-up resistors for all buttons
  if ( role == role_remote )
  {
    pinMode(led_pin,OUTPUT);
    pinMode(sensor_pin,INPUT);
    digitalWrite(sensor_pin,HIGH);
  }

  // Turn LED's ON until we start getting keys
  if ( role == role_led )
  {
    pinMode(led_pin,OUTPUT);
    led_state = HIGH;
    digitalWrite(led_pin, led_state);
  }

}

//
// Loop
//

void loop(void)
{
  //
  // Remote role.  If the state of any button has changed, send the whole state of
  // all buttons.
  //

  if ( role == role_remote )
  {
    // Get the current state of buttons, and
    // Test if the current state is different from the last state we sent
    bool different = false;
    uint8_t state = ! digitalRead(sensor_pin);
    printf("Sensor state: %d\n", state);
    if ( state != sensor_state )
    {
      different = true;
      sensor_state = state;
    }

    // Send the state of the buttons to the LED board
    if ( different )
    {
      printf("Now sending...");
      radio.stopListening();
      bool ok = radio.write( &sensor_state, sizeof(uint8_t) );
      if (ok)
        printf("ok\n\r");
      else
        printf("failed\n\r");
      radio.startListening();
      led_state = sensor_state;
      digitalWrite(led_pin, led_state);
    }

    // Try again in a short while
    delay(20);
  }

  //
  // LED role.  Receive the state of all buttons, and reflect that in the LEDs
  //

  if ( role == role_led )
  {
    // if there is data ready
    if ( radio.available() )
    {
      // Dump the payloads until we've gotten everything
      bool done = false;
      while (!done)
      {
        // Fetch the payload, and see if this was the last one.
        done = radio.read( &sensor_state, sizeof(uint8_t) );

        // Spew it
        printf("Got buttons\n\r");

        // For each button, if the button now on, then toggle the LED
        led_state = sensor_state;
        digitalWrite(led_pin, led_state);
      }
    }
  }
}
// vim:ai:cin:sts=2 sw=2 ft=cpp
