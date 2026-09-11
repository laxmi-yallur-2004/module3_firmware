#include <LiquidCrystal.h>

// ==================================================
// LCD
// RS, EN, D4, D5, D6, D7
// ==================================================

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// ==================================================
// INTERRUPT PINS
// ==================================================

const byte INTERRUPT_PIN = 2;
const byte ISR_OUTPUT_PIN = 13;

// ==================================================
// INTERRUPT COUNT
// ==================================================

volatile unsigned long interruptCount = 0;

// ==================================================
// LCD STARTUP TIMER
// NON-BLOCKING
// ==================================================

unsigned long startupTime = 0;
bool startupMessageDone = false;

// ==================================================
// INTERRUPT SERVICE ROUTINE
// ==================================================

void interruptISR()
{
  // Count interrupt
  interruptCount++;

  // Toggle D13 immediately
  PORTB ^= (1 << PB5);
}

// ==================================================
// SETUP
// ==================================================

void setup()
{
  // Serial
  Serial.begin(115200);

  // Interrupt input
  pinMode(INTERRUPT_PIN, INPUT);

  // ISR response output
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

  // Attach interrupt
  attachInterrupt(
    digitalPinToInterrupt(INTERRUPT_PIN),
    interruptISR,
    RISING
  );

  Serial.println("==============================");
  Serial.println("MODULE 3 - INTERRUPT TEST");
  Serial.println("==============================");
  Serial.println("CH1 -> D2");
  Serial.println("CH2 -> D13");
  Serial.println("D2 = Interrupt Input");
  Serial.println("D13 = ISR Response");
  Serial.println("==============================");
}

// ==================================================
// LOOP
// ==================================================

void loop()
{
  static unsigned long oldCount = 0;

  unsigned long count;

  // ==================================================
  // NON-BLOCKING STARTUP DISPLAY
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
  // SAFELY READ ISR VARIABLE
  // ==================================================

  noInterrupts();

  count = interruptCount;

  interrupts();

  // ==================================================
  // DISPLAY NEW INTERRUPT COUNT
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

    Serial.print("Interrupt = ");
    Serial.println(count);
  }
}
