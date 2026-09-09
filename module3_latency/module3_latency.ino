#include <LiquidCrystal.h>

// ==================================================
// LCD
// RS, EN, D4, D5, D6, D7
// ==================================================
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// ==================================================
// INTERRUPT PINS
// ==================================================
const byte INTERRUPT_PIN = 2;   // D2 = interrupt input
const byte ISR_OUTPUT_PIN = 13; // D13 = ISR response

volatile unsigned long interruptCount = 0;

// ==================================================
// SETUP
// ==================================================
void setup()
{
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

  delay(1500);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("D2 = INPUT");

  lcd.setCursor(0, 1);
  lcd.print("D13 = ISR");

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
// MAIN LOOP
// ==================================================
void loop()
{
  static unsigned long oldCount = 0;

  unsigned long count;

  // Safely read variable modified by ISR
  noInterrupts();
  count = interruptCount;
  interrupts();

  if (count != oldCount)
  {
    oldCount = count;

    // LCD
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("INT COUNT:");
    lcd.print(count);

    lcd.setCursor(0, 1);
    lcd.print("ISR RESPONSE");

    // Serial Monitor
    Serial.print("Interrupt = ");
    Serial.println(count);
  }
}

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
