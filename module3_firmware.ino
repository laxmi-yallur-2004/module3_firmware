
#include <Arduino.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>


// =====================================================
// COMMON HARDWARE
// Used by both tasks
// =====================================================

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);


// =====================================================
// TASK 1 — INTERRUPT / ISR
// =====================================================

// Interrupt input pin
const byte INTERRUPT_PIN = 2;

// ISR response output
const byte ISR_OUTPUT_PIN = 13;

// Interrupt counter
volatile unsigned long interruptCount = 0;

// Time of last accepted interrupt
volatile unsigned long lastInterruptTime = 0;

// 50 ms debounce time
const unsigned long DEBOUNCE_TIME = 50000UL;

// Startup timer
unsigned long startupTime = 0;

bool startupMessageDone = false;


// =====================================================
// TASK 2 — WATCHDOG / RESET REASON
// =====================================================

// EEPROM address
#define FAULT_ADDR 0

// Stored sensor fault code
#define SENSOR_FAULT 101


// -----------------------------------------------------
// Reset cause storage
//
// .noinit keeps these variables available during
// early startup so that MCUSR can be captured.
// -----------------------------------------------------

uint8_t resetCause
    __attribute__((section(".noinit")));

uint8_t optibootResetFlag
    __attribute__((section(".noinit")));


// -----------------------------------------------------
// Program stages
//
// 1 = first boot, save fault and start watchdog
// 2 = watchdog reset detected
// 3 = display final result
// 4 = watchdog running
// 5 = test complete
// -----------------------------------------------------

byte stage = 0;

unsigned long stageStartTime = 0;


// =====================================================
// TASK 2 — CAPTURE OPTIBOOT RESET FLAG
// =====================================================

void captureOptibootFlag(void)
    __attribute__((naked))
    __attribute__((used))
    __attribute__((section(".init0")));

void captureOptibootFlag(void)
{
    __asm__ __volatile__(
        "sts optibootResetFlag, r2\n"
    );
}


// =====================================================
// TASK 2 — CAPTURE MCUSR
//
// MCUSR = MCU Status Register
// WDRF  = Watchdog System Reset Flag
//
// This runs very early during startup.
// =====================================================

void captureMCUSR(void)
    __attribute__((naked))
    __attribute__((used))
    __attribute__((section(".init3")));

void captureMCUSR(void)
{
    __asm__ __volatile__(
        "in r24, %0\n"
        "sts resetCause, r24\n"
        "ldi r24, 0\n"
        "out %0, r24\n"
        :
        : "I" (_SFR_IO_ADDR(MCUSR))
        : "r24"
    );

    wdt_disable();
}


// =====================================================
// TASK 1 — INTERRUPT SERVICE ROUTINE
// =====================================================

void interruptISR()
{
    unsigned long currentTime = micros();

    // -----------------------------------------------
    // Debounce
    // -----------------------------------------------

    if (currentTime - lastInterruptTime >= DEBOUNCE_TIME)
    {
        // Count valid interrupt
        interruptCount++;

        // Toggle D13 directly
        // Arduino Uno D13 = PB5
        PORTB ^= (1 << PB5);

        // Save interrupt time
        lastInterruptTime = currentTime;
    }
}


// =====================================================
// ONE SETUP()
// Both tasks are initialized here.
// =====================================================

void setup()
{
    // =================================================
    // COMMON — LCD
    // =================================================

    lcd.begin(16, 2);


    // =================================================
    // COMMON — SERIAL
    // =================================================

    Serial.begin(9600);


    // =================================================
    // TASK 1 — INTERRUPT INITIALIZATION
    // =================================================

    // D2 uses internal pull-up
    pinMode(INTERRUPT_PIN, INPUT_PULLUP);

    // D13 is ISR response output
    pinMode(ISR_OUTPUT_PIN, OUTPUT);

    digitalWrite(ISR_OUTPUT_PIN, LOW);


    // Attach external interrupt
    // D2 HIGH -> LOW = FALLING edge
    attachInterrupt(
        digitalPinToInterrupt(INTERRUPT_PIN),
        interruptISR,
        FALLING
    );


    // =================================================
    // TASK 2 — CHECK RESET REASON
    // =================================================

    bool watchdogReset = false;


    // Check actual ATmega328P MCUSR WDRF
    if (resetCause & (1 << WDRF))
    {
        watchdogReset = true;
    }


    // Also check Optiboot reset flag
    if (optibootResetFlag & (1 << WDRF))
    {
        watchdogReset = true;
    }


    // =================================================
    // TASK 2 — WATCHDOG RESET DETECTED
    // =================================================

    if (watchdogReset)
    {
        int lastFault = 0;

        // Read previously stored fault
        EEPROM.get(FAULT_ADDR, lastFault);


        // ---------------------------------------------
        // SERIAL OUTPUT
        // ---------------------------------------------

        Serial.println();
        Serial.println("==============================");
        Serial.println("        MODULE 4");
        Serial.println("==============================");
        Serial.println("RESET STATUS");

        Serial.print("MCUSR = 0x");
        Serial.println(resetCause, HEX);

        Serial.print("WDRF = ");

        if (resetCause & (1 << WDRF))
        {
            Serial.println("1");
        }
        else
        {
            Serial.println("1 (Optiboot)");
        }

        Serial.println("Reset Reason: WATCHDOG");

        Serial.print("Last Fault Code: ");
        Serial.println(lastFault);


        // ---------------------------------------------
        // LCD OUTPUT
        // ---------------------------------------------

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("RESET: WATCHDOG");

        lcd.setCursor(0, 1);
        lcd.print("WDRF = 1");


        // Move to next stage
        stageStartTime = millis();

        stage = 2;
    }


    // =================================================
    // TASK 2 — NORMAL POWER-ON
    // =================================================

    else
    {
        // ---------------------------------------------
        // Save sensor fault code in EEPROM
        // ---------------------------------------------

        EEPROM.put(FAULT_ADDR, SENSOR_FAULT);


        // ---------------------------------------------
        // SERIAL OUTPUT
        // ---------------------------------------------

        Serial.println();
        Serial.println("==============================");
        Serial.println("        MODULE 4");
        Serial.println("==============================");
        Serial.println("RESET STATUS");

        Serial.print("MCUSR = 0x");
        Serial.println(resetCause, HEX);

        Serial.println("WDRF = 0");

        Serial.println("Reset Reason: POWER ON");

        Serial.println("Sensor Fault Detected");

        Serial.println("Saving Fault Code: 101");


        // ---------------------------------------------
        // LCD OUTPUT
        // ---------------------------------------------

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("FAULT SAVED");

        lcd.setCursor(0, 1);
        lcd.print("CODE: 101");


        // Start watchdog sequence
        stageStartTime = millis();

        stage = 1;
    }


    // =================================================
    // TASK 1 — STARTUP TIMER
    // =================================================

    startupTime = millis();
}


// =====================================================
// ONE LOOP()
// Both tasks operate from the same loop.
// =====================================================

void loop()
{
    unsigned long currentTime = millis();


    // =================================================
    // TASK 1 — STARTUP DISPLAY
    // =================================================

    if (!startupMessageDone)
    {
        if (currentTime - startupTime >= 1500)
        {
            startupMessageDone = true;

            // Only show Task 1 display when
            // Task 2 is not controlling the LCD.
            if (stage == 0)
            {
                lcd.clear();

                lcd.setCursor(0, 0);
                lcd.print("D2 = INPUT");

                lcd.setCursor(0, 1);
                lcd.print("D13 = ISR");
            }
        }
    }


    // =================================================
    // TASK 1 — READ INTERRUPT COUNT
    // =================================================

    static unsigned long oldCount = 0;

    unsigned long count;


    // Safely read volatile 32-bit counter
    noInterrupts();

    count = interruptCount;

    interrupts();


    // =================================================
    // TASK 1 — DISPLAY INTERRUPT COUNT
    // =================================================

    // Do not overwrite Task 2 LCD information
    if (stage == 0)
    {
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


    // =================================================
    // TASK 2 — STAGE 1
    //
    // First power-on:
    // Save fault -> wait 2 sec -> start watchdog
    // =================================================

    if (stage == 1)
    {
        if (currentTime - stageStartTime >= 2000)
        {
            // -----------------------------------------
            // LCD
            // -----------------------------------------

            lcd.clear();

            lcd.setCursor(0, 0);
            lcd.print("WATCHDOG");

            lcd.setCursor(0, 1);
            lcd.print("STARTING...");


            // -----------------------------------------
            // SERIAL
            // -----------------------------------------

            Serial.println();
            Serial.println("Starting Watchdog");
            Serial.println("Timeout: 1 second");

            Serial.flush();


            // -----------------------------------------
            // START ATmega328P WATCHDOG
            // -----------------------------------------

            wdt_enable(WDTO_1S);


            /*
               IMPORTANT:

               We intentionally DO NOT call:

                   wdt_reset();

               Therefore the watchdog will expire and
               automatically reset the ATmega328P.
            */

            stage = 4;
        }
    }


    // =================================================
    // TASK 2 — STAGE 2
    //
    // This executes after watchdog reset.
    // =================================================

    else if (stage == 2)
    {
        if (currentTime - stageStartTime >= 2000)
        {
            int lastFault = 0;

            // Read fault saved before reset
            EEPROM.get(FAULT_ADDR, lastFault);


            // -----------------------------------------
            // LCD
            // -----------------------------------------

            lcd.clear();

            lcd.setCursor(0, 0);
            lcd.print("LAST FAULT:");

            lcd.setCursor(0, 1);
            lcd.print(lastFault);


            // -----------------------------------------
            // SERIAL
            // -----------------------------------------

            Serial.print("Last Fault Code: ");
            Serial.println(lastFault);


            stageStartTime = currentTime;

            stage = 3;
        }
    }


    // =================================================
    // TASK 2 — STAGE 3
    //
    // Test complete
    // =================================================

    else if (stage == 3)
    {
        if (currentTime - stageStartTime >= 2000)
        {
            // -----------------------------------------
            // LCD
            // -----------------------------------------

            lcd.clear();

            lcd.setCursor(0, 0);
            lcd.print("MODULE 4");

            lcd.setCursor(0, 1);
            lcd.print("TEST COMPLETE");


            // -----------------------------------------
            // SERIAL
            // -----------------------------------------

            Serial.println("TEST COMPLETE");

            stage = 5;
        }
    }


    // =================================================
    // TASK 2 — STAGE 4
    //
    // Watchdog is running.
    //
    // Do not call wdt_reset().
    // The hardware watchdog will reset the MCU.
    // =================================================

    else if (stage == 4)
    {
        // Intentionally empty.
    }


    // =================================================
    // TASK 2 — STAGE 5
    //
    // Test finished.
    // =================================================

    else if (stage == 5)
    {
        // Test complete.
    }
}

