#include <avr/sleep.h> 
#include <avr/interrupt.h> 
#include <avr/wdt.h>

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <pinchange.h>

void sleepNow(void);
void wakeup();
void setupWatchdog(uint8_t prescalar);

#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny85__)
    RF24 radio(3,7);
    const int role_pin = 10;
    const int sensor_pin = 2;
    const int led_pin = 1;
    // const int sensor_pin = 8;
    // const int led_pin = 9;
#else
    RF24 radio(9, 10);
    const int role_pin = 6;
    const int sensor_pin = 2;
    const int led_pin = 4;
#endif 

const uint64_t pipe = 0xA8E8F0F0F1LL;

typedef enum { wdt_16ms = 0, wdt_32ms, wdt_64ms, wdt_128ms, wdt_250ms, 
               wdt_500ms, wdt_1s, wdt_2s, wdt_4s, wdt_8s } wdt_prescalar_e;

typedef enum { 
    role_remote = 1, 
    role_led = 2
} role_e;
role_e role;
const char* role_friendly_name[] = { "invalid", "Remote", "LED Board"};

uint8_t led_state = 0;
uint8_t sensor_state = 0;
volatile int awakems = 0;
int send_tries = 0;
bool send_ok = false;

void setup(void) {
    // set up the role pin
    pinMode(role_pin, INPUT);
    digitalWrite(role_pin, HIGH);
    delay(20); // Just to get a solid reading on the role pin

    // read the address pin, establish our role
    role = digitalRead(role_pin) ? role_remote : role_led;
    digitalWrite(role_pin, LOW);
    
    Serial.begin(9600);

    radio.begin();
    radio.setChannel(38);
    radio.setDataRate(RF24_250KBPS);
    radio.setAutoAck(pipe, true);
    radio.setRetries(15, 15);

    if (role == role_remote) {
        radio.openWritingPipe(pipe);
        radio.stopListening();
    } else {
        radio.openReadingPipe(1,pipe);
        radio.startListening();
    }

//    radio.printDetails();

    if (role == role_remote) {
        pinMode(sensor_pin,INPUT);
        digitalWrite(sensor_pin,HIGH);
    }

    if (role == role_led) {
        setupWatchdog(wdt_2s);
    }

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
    bool different = false;
    
    if (role == role_remote) {
        // Get the current state of buttons, and
        // Test if the current state is different from the last state we sent
        uint8_t state = !digitalRead(sensor_pin);
        Serial.write("Sensor state: ");
        Serial.write(state ? "ON" : "off");
        Serial.write('\n');
        if (state != sensor_state) {
            different = true;
            send_tries = 1000;
            sensor_state = state;
            led_state = sensor_state;
        }

        // Send the state of the buttons to the LED board
        if (send_tries && (different || !send_ok)) {
            Serial.write("Now sending...");
            digitalWrite(led_pin, led_state);
            radio.powerUp();
            delay(10);
            send_ok = radio.write( &sensor_state, sizeof(uint8_t) );
            if (send_ok) {
                Serial.write("ok\n\r");
            } else {
                char tries_left_char[1];
                send_tries--;
                itoa(send_tries, tries_left_char, 10);
                Serial.write("failed (");
                Serial.write((char *)tries_left_char);
                Serial.write(" tries left)\n\r");
                digitalWrite(led_pin, LOW);
                delay(25);        
                digitalWrite(led_pin, led_state);            
            }
            radio.powerDown();
            awakems = 0;
        }

        awakems += 1;
        if (awakems > 10) {
            sleepNow();
            awakems = 0;
        }
    }

    if (role == role_led) {
         // digitalWrite(led_pin, HIGH);

        if (radio.available()) {
            // Dump the payloads until we've gotten everything
            bool done = false;
            awakems = 0;
            while (!done) {
                done = radio.read( &sensor_state, sizeof(uint8_t) );
            }
            Serial.write("Got buttons: ");
            Serial.write(sensor_state ? "ON\n\r" : "off\n\r");
            led_state = sensor_state;
            digitalWrite(led_pin, led_state);
            awakems = -10000;
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
            
            awakems = 0;
        }
        
        awakems += 1;
        if (awakems > 4000) {
            // digitalWrite(led_pin, LOW);
            sleepNow();
            awakems = 0;
        }
    }
}
#define BODS 7                   //BOD Sleep bit in MCUCR
#define BODSE 2                  //BOD Sleep enable bit in MCUCR
uint8_t mcucr1, mcucr2;

void setupWatchdog(uint8_t prescalar) {
  prescalar = min(9,prescalar);
  uint8_t wdtcsr = prescalar & 7;
  if ( prescalar & 8 )
    wdtcsr |= _BV(WDP3);

  MCUSR &= ~_BV(WDRF);
  WDTCSR = _BV(WDCE) | _BV(WDE);
  WDTCSR = _BV(WDCE) | wdtcsr | _BV(WDIE);
}

void sleepNow(void) {
    Serial.write("Sleeping...\n\r");
    if (role == role_led) {
        radio.stopListening();
    }
    radio.powerDown();

    if (role == role_remote) {
        attachPcInterrupt(sensor_pin, wakeup, CHANGE);
    }
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
    if (role == role_remote) {
        detachPcInterrupt(sensor_pin);
    }
    sleep_disable();               
    sei();                         //enable interrupts again (but INT0 is disabled from above)
    Serial.print("Wakeup...\n\r");
    if (role == role_led) {
      radio.startListening();
    }
    delay(5);
}

void wakeup() {
    awakems = 0;
}

ISR(WDT_vect) {
    awakems = 0;
}
