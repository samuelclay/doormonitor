#include <avr/sleep.h> 
#include <avr/interrupt.h> 

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <printf.h>

RF24 radio(3,7);

const int role_pin = 10;
const int sensor_pin = 2;
const int led_pin = 1;
const int sleep_pin = 8;
const uint64_t pipe = 0xE8E8F0F0F1LL;

typedef enum { 
    role_remote = 1, 
    role_led = 2
} role_e;
role_e role;
const char* role_friendly_name[] = { "invalid", "Remote", "LED Board"};

uint8_t led_state;
uint8_t sensor_state;
volatile int awakems = 0;

void setup(void) {
    // set up the role pin
    pinMode(role_pin, INPUT);
    digitalWrite(role_pin, HIGH);
    delay(20); // Just to get a solid reading on the role pin

    // read the address pin, establish our role
    role = digitalRead(role_pin) ? role_remote : role_led;

    Serial.begin(9600);
    printf_begin();
    Serial.print("\n\rRF24/examples/led_remote/\n\r");
    printf("ROLE: %s\n\r",role_friendly_name[role]);

    radio.begin();
    radio.setChannel(37);
    radio.setDataRate(RF24_250KBPS);
    radio.setAutoAck(pipe, true);

    if (role == role_remote) {
        radio.openWritingPipe(pipe);
    } else {
        radio.openReadingPipe(1,pipe);
    }

    if (role == role_led) {
        radio.startListening();
    }

    radio.printDetails();

    if (role == role_remote) {
        pinMode(sensor_pin,INPUT);
        digitalWrite(sensor_pin,HIGH);
    }

    // Turn LED's ON until we start getting keys
    pinMode(sleep_pin,OUTPUT);
    digitalWrite(sleep_pin, HIGH);
    
    pinMode(led_pin,OUTPUT);
    led_state = LOW;
    digitalWrite(led_pin, led_state);
    int i = role == role_led ? 4 : 2;
    int pause = role == role_led ? 100 : 300;
    while (i--) {
        delay(pause);
        digitalWrite(led_pin, HIGH);
        delay(pause);
        digitalWrite(led_pin, LOW);
    }
}

void loop(void) {
    if (role == role_remote) {
        // Get the current state of buttons, and
        // Test if the current state is different from the last state we sent
        bool different = false;
        uint8_t state = ! digitalRead(sensor_pin);
        printf("Sensor state: %d\n", state);
        if (state != sensor_state) {
            different = true;
            sensor_state = state;
            awakems = 0;
        }

        // Send the state of the buttons to the LED board
        if (different) {
            printf("Now sending...");
            radio.stopListening();
            bool ok = radio.write( &sensor_state, sizeof(uint8_t) );
            if (ok) {
                printf("ok\n\r");
            } else {
                printf("failed\n\r");
            }
            radio.startListening();
            led_state = sensor_state;
            digitalWrite(led_pin, led_state);
        }

        delay(20);
        awakems += 20;
        if (awakems > 500) {
          sleepNow();
          awakems = 0;
        }
    }

    if (role == role_led) {
        if (radio.available()) {
            // Dump the payloads until we've gotten everything
            bool done = false;
            while (!done) {
                done = radio.read( &sensor_state, sizeof(uint8_t) );
                printf("Got buttons\n\r");
                led_state = sensor_state;
                digitalWrite(led_pin, led_state);
            }
        }
        
        uint8_t incomingByte;
        // send data only when you receive data:
	if (Serial.available() > 0) {
            // read the incoming byte:
            incomingByte = Serial.read();
            
            // say what you got with both the ASCII and decimal representations
            Serial.print("I received: ");
            Serial.write(incomingByte);
            Serial.print(" : ");
            Serial.println(incomingByte, DEC);
	}
    }
}
#define BODS 7                   //BOD Sleep bit in MCUCR
#define BODSE 2                  //BOD Sleep enable bit in MCUCR
uint8_t mcucr1, mcucr2;

void sleepNow(void)
{
  
    radio.powerDown();
    digitalWrite(sleep_pin, LOW);
    GIMSK |= _BV(INT0);                       //enable INT0
    MCUCR &= ~(_BV(ISC01) | _BV(ISC00));      //INT0 on low level
    ACSR |= _BV(ACD);                         //disable the analog comparator
    ADCSRA &= ~_BV(ADEN);                     //disable ADC
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    
    //turn off the brown-out detector.
    //must have an ATtiny45 or ATtiny85 rev C or later for software to be able to disable the BOD.
    //current while sleeping will be <0.5uA if BOD is disabled, <25uA if not.
    cli();
    mcucr1 = MCUCR | _BV(BODS) | _BV(BODSE);  //turn off the brown-out detector
    mcucr2 = mcucr1 & ~_BV(BODSE);
    MCUCR = mcucr1;
    MCUCR = mcucr2;
    sei();                         //ensure interrupts enabled so we can wake up again
    sleep_cpu();                   //go to sleep
    cli();                         //wake up here, disable interrupts
    GIMSK = 0x00;                  //disable INT0
    sleep_disable();               
    sei();                         //enable interrupts again (but INT0 is disabled from above)
    radio.powerUp();
    digitalWrite(sleep_pin, HIGH);
}

ISR(INT0_vect) {}  

