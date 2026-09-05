#include <LiquidCrystal.h>

// =====================================================
// MODULE 3 - INTERRUPT LATENCY / JITTER
// Arduino UNO
// Software-based test
// No oscilloscope required
// =====================================================

// LCD connections
// RS -> D8
// EN -> D9
// D4 -> D4
// D5 -> D5
// D6 -> D6
// D7 -> D7

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);


// =====================================================
// PIN DEFINITIONS
// =====================================================

const byte INTERRUPT_PIN = 2;
const byte TEST_PULSE_PIN = 3;
const byte ISR_OUTPUT_PIN = 13;


// =====================================================
// INTERRUPT VARIABLES
// =====================================================

volatile unsigned long interruptCount = 0;

volatile unsigned long lastInterruptTime = 0;
volatile unsigned long measuredLatency = 0;


// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  // D2 receives interrupt
  pinMode(INTERRUPT_PIN, INPUT);

  // D3 generates test pulse
  pinMode(TEST_PULSE_PIN, OUTPUT);

  // D13 shows ISR response
  pinMode(ISR_OUTPUT_PIN, OUTPUT);

  digitalWrite(TEST_PULSE_PIN, LOW);
  digitalWrite(ISR_OUTPUT_PIN, LOW);


  // ===================================================
  // LCD INITIALIZATION
  // ===================================================

  lcd.begin(16, 2);

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("MODULE 3");

  lcd.setCursor(0, 1);
  lcd.print("INT LATENCY");

  delay(1500);


  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("D3 -> D2");

  lcd.setCursor(0, 1);
  lcd.print("INT TEST");

  delay(1500);


  // ===================================================
  // EXTERNAL INTERRUPT
  // ===================================================

  attachInterrupt(
    digitalPinToInterrupt(INTERRUPT_PIN),
    interruptISR,
    RISING
  );


  Serial.println("================================");
  Serial.println("MODULE 3 INTERRUPT TEST");
  Serial.println("D3 -> D2");
  Serial.println("D2 = INTERRUPT INPUT");
  Serial.println("D13 = ISR RESPONSE");
  Serial.println("================================");
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop()
{
  static unsigned long lastDisplayCount = 0;

  // Generate a test pulse on D3

  digitalWrite(TEST_PULSE_PIN, HIGH);

  // Give the interrupt time to execute
  delayMicroseconds(10);

  digitalWrite(TEST_PULSE_PIN, LOW);


  // Read interrupt counter safely

  noInterrupts();

  unsigned long count = interruptCount;
  unsigned long latency = measuredLatency;

  interrupts();


  // Update LCD when interrupt occurs

  if (count != lastDisplayCount)
  {
    lastDisplayCount = count;

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("INT:");
    lcd.print(count);

    lcd.setCursor(0, 1);
    lcd.print("TIME:");
    lcd.print(latency);
    lcd.print(" us");


    // Serial output

    Serial.print("Interrupt = ");
    Serial.print(count);

    Serial.print("   Time = ");
    Serial.print(latency);

    Serial.println(" us");
  }


  delay(10);
}


// =====================================================
// INTERRUPT SERVICE ROUTINE
// =====================================================

void interruptISR()
{
  // Record the interrupt occurrence

  interruptCount++;


  // Toggle D13

  PORTB ^= (1 << PB5);


  // Simple ISR timing marker

  measuredLatency = micros();
}