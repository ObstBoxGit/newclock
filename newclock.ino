/**********************************************************************
 * 
 * 
 * *******************************************************************/
#define SERIAL_DEBUG 

#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>
#include <MsTimer2.h>


// display com pins
#define HOUR_TENS_LED_PIN      5  // A0
#define HOUR_UNITS_LED_PIN     6  // A1
#define MINUTE_TENS_LED_PIN    9  // 2
#define MINUTE_UNITS_LED_PIN   10 // 3

#define LED_A_PIN              8
#define LED_B_PIN              11
#define LED_C_PIN              3  // 10
#define LED_D_PIN              2  // 9
#define LED_E_PIN              A1 // 6
#define LED_F_PIN              7
#define LED_G_PIN              12
#define ROUND_LED_PIN          A0 // 5

#define LDR_PIN                A2
#define MAIN_BUTTON_PIN             4

// thr1 = 2 thr2 = 8 at 100us
#define BUTTON_THRESHOLD1        15
//~ #define BUTTON_THRESHOLD2        50
#define BUTTON_THRESHOLD2        50

#define DISPLAY_PERIOD_FULL_US              2000 // us
#define LEDS_OFF_DURATION_DEFAULT_US     10
#define LEDS_ON_DURATION_DEFAULT_US  (DISPLAY_PERIOD_FULL_US - LEDS_OFF_DURATION_DEFAULT_US)

#define T2_INTERRUPT_MS      4

#define SYMBOL_SEGMENTS_AMOUNT    7
#define DISPLAY_DIGITS_AMOUNT      4
#define FINAL_TICK 4

#define ANALOG_READ_RESOLUTION   1024
#define ANALOG_WRITE_RESOLUTION   256

enum ButtonState {
    DEPRESSED = 0,
    SHORT_PRESS,
    LONG_PRESS
};

enum Symbol {
    SYMBOL_0 = 0,
    SYMBOL_1,
    SYMBOL_2,
    SYMBOL_3,
    SYMBOL_4,
    SYMBOL_5,
    SYMBOL_6,
    SYMBOL_7,
    SYMBOL_8,
    SYMBOL_9,
    SYMBOL_NONE,
    SYMBOL_MINUS,
    SYMBOL_r,
    SYMBOL_t,
    SYMBOL_c,
    SYMBOL_E,
    SYMBOL_AMOUNT           // used for assert
};

//~ static uint8_t        display_data[DISPLAY_DIGITS_AMOUNT];
static const uint8_t  display_digits_pins[DISPLAY_DIGITS_AMOUNT] = { HOUR_TENS_LED_PIN, 
  HOUR_UNITS_LED_PIN, MINUTE_TENS_LED_PIN, MINUTE_UNITS_LED_PIN };

static const uint8_t symbol_segments_pins[SYMBOL_SEGMENTS_AMOUNT] = { LED_A_PIN, LED_B_PIN, 
  LED_C_PIN, LED_D_PIN, LED_E_PIN, LED_F_PIN, LED_G_PIN };

static const uint8_t symbol_decode_table[SYMBOL_AMOUNT][SYMBOL_SEGMENTS_AMOUNT] = 
  {{ HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, LOW },       // SYMBOL_0 
  { HIGH, HIGH, LOW, LOW, LOW, LOW, LOW },            // SYMBOL_1
  { HIGH, LOW, HIGH, HIGH, LOW, HIGH, HIGH },         // SYMBOL_2
  { HIGH, HIGH, HIGH, LOW, LOW, HIGH, HIGH },         // SYMBOL_3
  { HIGH, HIGH, LOW, LOW, HIGH, LOW, HIGH },          // SYMBOL_4
  
  
  { LOW, HIGH, HIGH, LOW, HIGH, HIGH, HIGH },         // SYMBOL_5
  { LOW, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH },        // SYMBOL_6
  { HIGH, HIGH, LOW, LOW, LOW, HIGH, LOW },           // SYMBOL_7
  { HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH },       // SYMBOL_8
  { HIGH, HIGH, HIGH, LOW, HIGH, HIGH, HIGH },        // SYMBOL_9
  { LOW, LOW, LOW, LOW, LOW, LOW, LOW },              // SYMBOL_NONE
  { LOW, LOW, LOW, LOW, LOW, LOW, HIGH},              // SYMBOL_MINUS
  { LOW, LOW, LOW, HIGH, LOW, LOW, HIGH },            // SYMBOL_r
  { LOW, LOW, HIGH, HIGH, HIGH, LOW, HIGH },          // SYMBOL_t
  { LOW, LOW, HIGH, HIGH, LOW, LOW, HIGH },           // SYMBOL_c
  { LOW, LOW, HIGH, HIGH, HIGH, HIGH, HIGH }};        // SYMBOL_E
  

uint16_t leds_on_duration = LEDS_ON_DURATION_DEFAULT_US;
uint16_t leds_off_duration = LEDS_OFF_DURATION_DEFAULT_US;

uint16_t system_timer_counter = 0;

typedef struct {
  uint8_t counter;
  ButtonState state;
} Button;

typedef struct {
  uint16_t counter;
  boolean tick;
} SystemTimer;

struct Display {
  uint8_t pointer;
  uint8_t brightness;
  static const uint8_t digits_pins[DISPLAY_DIGITS_AMOUNT];
  static const uint8_t segments_pins[SYMBOL_SEGMENTS_AMOUNT];
};

const uint8_t Display::digits_pins[DISPLAY_DIGITS_AMOUNT] = 
  {HOUR_TENS_LED_PIN, HOUR_UNITS_LED_PIN, MINUTE_TENS_LED_PIN, MINUTE_UNITS_LED_PIN};
const uint8_t Display::segments_pins[SYMBOL_SEGMENTS_AMOUNT] =   
  { LED_A_PIN, LED_B_PIN, LED_C_PIN, LED_D_PIN, LED_E_PIN, LED_F_PIN, LED_G_PIN };
  
Button mainButton;
SystemTimer systemTimer;
Display display;



uint8_t raw_data[DISPLAY_DIGITS_AMOUNT] = {2,6,6,6};

// --------------------------------------------------------------------------------------------------------------
void setup () {
  pinMode(MAIN_BUTTON_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);

  pinMode(LED_A_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);
  pinMode(LED_C_PIN, OUTPUT);
  pinMode(LED_D_PIN, OUTPUT);
  pinMode(LED_E_PIN, OUTPUT);
  pinMode(LED_F_PIN, OUTPUT);
  pinMode(LED_G_PIN, OUTPUT);
  pinMode(ROUND_LED_PIN, OUTPUT);
  
  pinMode(HOUR_TENS_LED_PIN, OUTPUT);
  pinMode(HOUR_UNITS_LED_PIN, OUTPUT);
  pinMode(MINUTE_TENS_LED_PIN, OUTPUT);
  pinMode(MINUTE_UNITS_LED_PIN, OUTPUT);
  
  digitalWrite(HOUR_TENS_LED_PIN,    LOW);
  digitalWrite(HOUR_UNITS_LED_PIN,   LOW);
  digitalWrite(MINUTE_TENS_LED_PIN,  LOW);
  digitalWrite(MINUTE_UNITS_LED_PIN, LOW); 
  
  digitalWrite(MAIN_BUTTON_PIN, HIGH);
  digitalWrite(LDR_PIN, HIGH);

  digitalWrite(LED_A_PIN, LOW);
  digitalWrite(LED_B_PIN, LOW);
  digitalWrite(LED_C_PIN, LOW);
  digitalWrite(LED_D_PIN, LOW);
  digitalWrite(LED_E_PIN, LOW);
  digitalWrite(LED_F_PIN, LOW);
  digitalWrite(LED_G_PIN, LOW);
  digitalWrite(ROUND_LED_PIN, LOW);

  systemTimer.counter = 0;
  systemTimer.tick = false;
  display.pointer = 0;
  display.brightness = ANALOG_WRITE_RESOLUTION - 1;  

  MsTimer2::set(T2_INTERRUPT_MS, systemTick);
  MsTimer2::start();
  
  Serial.begin(9600);
}

// --------------------------------------------------------------------------------------------------------------
void loop() {   
  
  //~ delayMicroseconds(250);
   
  if ( systemTimer.tick == true) {
    systemTimer.tick = false;
    
    // systemTimer.counter == 1
    displayNextDigit();
    
    
    if (systemTimer.counter % 4 == 0) {        
      readButton();
    }
    
    if (systemTimer.counter % 20 == 0) {          
      if (mainButton.state == SHORT_PRESS) {
        raw_data[0] = 6;
      }      
      if (mainButton.state == LONG_PRESS) {
        raw_data[0] = 9;
      }    
    }
    
  }
}


// --------------------------------------------------------------------------------------------------------------
void displayNextDigit() {
  displayDigitOff(display.digits_pins[display.pointer]);
  if ( ++display.pointer >= DISPLAY_DIGITS_AMOUNT ) {
    display.pointer = 0;
  }
  setDigitalSegments(raw_data[display.pointer]);
  analogWrite(display.digits_pins[display.pointer], display.brightness);  
}

void displayDigitOff(uint8_t pin) {
  digitalWrite(pin, LOW);  
}

void setDigitalSegments(uint8_t data) {
  if (data > SYMBOL_AMOUNT) {
    return;
  }
  for (uint8_t i = 0; i < SYMBOL_SEGMENTS_AMOUNT; i++ ) {
    digitalWrite(symbol_segments_pins[i], symbol_decode_table[data][i]);
  }
}

uint16_t readLightDrivenResistor() {
  uint16_t value = analogRead(LDR_PIN);
  return value;
}

void readButton() {
  if (digitalRead(MAIN_BUTTON_PIN) == LOW) {    // is pressed
    if (mainButton.counter <= BUTTON_THRESHOLD2) mainButton.counter++;
  }
  else {    // is released
    if ((mainButton.counter >= BUTTON_THRESHOLD1) && (mainButton.counter < BUTTON_THRESHOLD2)) {
      mainButton.counter = 0;
      mainButton.state = SHORT_PRESS;
    }
    else if (mainButton.counter >= BUTTON_THRESHOLD2) {
      mainButton.counter = 0;
      mainButton.state = LONG_PRESS;
    }
    else {  // mainButton.counter < BUTTON_THRESHOLD1 < BUTTON_THRESHOLD2
      mainButton.counter = 0;
      mainButton.state = DEPRESSED;
    }
  }
}

void systemTick() {
  systemTimer.tick = true;
  if (++systemTimer.counter == FINAL_TICK) {
    systemTimer.counter = 0;
  }  
}
