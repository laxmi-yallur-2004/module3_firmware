#include <LiquidCrystal.h>

// ==================================================
// LCD
// RS, EN, D4, D5, D6, D7
// ==================================================

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// ==================================================
// INTERRUPT PIN
// ==================================================

const byte INTERRUPT_PIN = 2;
const byte ISR_OUTPUT_PIN = 13;

// ==================================================
// INTERRUPT VARIABLES
// ==================================================

volatile unsigned long interruptCount = 0;
volatile unsigned long lastInterruptTime = 0;

// Ignore interrupts occurring within 50 ms
const unsigned long DEBOUNCE_TIME = 50000UL;

// ==================================================
// INTERRUPT SERVICE ROUTINE
// ==================================================

void interruptISR()
{
  unsigned long currentTime = micros();

  // Debounce
  if (currentTime - lastInterruptTime >= DEBOUNCE_TIME)
  {
    interruptCount++;

    // Toggle D13
    PORTB ^= (1 << PB5);

    lastInterruptTime = currentTime;
  }
}

// ==================================================
// STARTUP TIMER
// ==================================================

unsigned long startupTime = 0;
bool startupMessageDone = false;

// ==================================================
// SETUP
// ==================================================

void setup()
{
  // Internal pull-up
  pinMode(INTERRUPT_PIN, INPUT_PULLUP);

  // D13 = ISR response
  pinMode(ISR_OUTPUT_PIN, OUTPUT);
  digitalWrite(ISR_OUTPUT_PIN, LOW);

  // LCD
  lcd.begin(16, 2);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("MODULE 3");

  lcd.setCursor(0, 1);
  lcd.print("INT LATENCY");

  startupTime = millis();

  // D2 HIGH -> LOW triggers interrupt
  attachInterrupt(
    digitalPinToInterrupt(INTERRUPT_PIN),
    interruptISR,
    FALLING
  );
}

// ==================================================
// LOOP
// ==================================================

void loop()
{
  static unsigned long oldCount = 0;

  unsigned long count;

  // ==================================================
  // NON-BLOCKING STARTUP
  // ==================================================

  if (!startupMessageDone)
  {
    if (millis() - startupTime >= 1500)
    {
      startupMessageDone = true;

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("D2 = INPUT");

      lcd.setCursor(0, 1);
      lcd.print("D13 = ISR");
    }
  }

  // ==================================================
  // READ INTERRUPT COUNT SAFELY
  // ==================================================

  noInterrupts();

  count = interruptCount;

  interrupts();

  // ==================================================
  // DISPLAY COUNT
  // ==================================================

  if (count != oldCount)
  {
    oldCount = count;

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("INT COUNT:");
    lcd.print(count);

    lcd.setCursor(0, 1);
    lcd.print("ISR RESPONSE");
  }
}
