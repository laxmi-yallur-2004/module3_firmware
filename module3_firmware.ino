/*
============================================================
MODULE 3 FIRMWARE
============================================================

TASK 1 : INTERRUPT RESPONSE / LATENCY DEMONSTRATION
TASK 2 : WATCHDOG RESET + RESET REASON DETECTION

Board:
Arduino Uno
ATmega328P
16 MHz

LCD:
RS = D8
EN = D9
D4 = D4
D5 = D5
D6 = D6
D7 = D7

Task 1:
Interrupt input = D2
ISR output      = D13

Task 2:
Watchdog Timer
MCUSR / WDRF reset detection
EEPROM fault storage

No delay()
No dynamic memory
Non-blocking main loop
============================================================
*/


/* =========================================================
   COMMON INCLUDES
   ========================================================= */

#include <Arduino.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <avr/io.h>
#include <avr/wdt.h>


/* =========================================================
   COMMON LCD
   ========================================================= */

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);


/* =========================================================
   TASK 1 : INTERRUPT / LATENCY
   ========================================================= */

const uint8_t INTERRUPT_PIN = 2;
const uint8_t ISR_OUTPUT_PIN = 13;

volatile unsigned long interruptCount = 0;
volatile unsigned long lastInterruptTime = 0;

const unsigned long DEBOUNCE_TIME = 50000UL;

unsigned long task1StartTime = 0;

bool task1DisplayDone = false;


/* =========================================================
   TASK 2 : WATCHDOG / RESET REASON
   ========================================================= */

#define FAULT_ADDR       0
#define SENSOR_FAULT     101

#define WATCHDOG_MARKER  0xA5


/*
   These variables are placed in .noinit.

   .noinit variables survive a watchdog reset.
   They are NOT initialized by normal C startup.
*/

uint8_t resetCause
    __attribute__((section(".noinit")));

uint8_t optibootResetFlag
    __attribute__((section(".noinit")));


/*
   EEPROM marker.

   This is only a fallback mechanism.

   It tells us:

   "Before the previous reset, this program deliberately
    enabled the watchdog."

   The actual hardware reset flag is still checked using WDRF.
*/

#define WD_MARKER_ADDR  1


uint8_t watchdogResetDetected = 0;

uint8_t effectiveResetCause = 0;

uint8_t lastFault = 0;

uint8_t task2Stage = 0;

unsigned long task2Timer = 0;


/* =========================================================
   TASK 2 : EARLY OPTIBOOT RESET FLAG CAPTURE
   ========================================================= */

/*
   Optiboot can pass the reset cause in CPU register R2.

   This function executes in .init0, before normal C startup.

   R2 is saved into optibootResetFlag.
*/

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


/* =========================================================
   TASK 2 : EARLY MCUSR CAPTURE
   ========================================================= */

/*
   This executes very early during startup.

   It:
   1. Reads MCUSR
   2. Saves it into resetCause
   3. Clears MCUSR
   4. Disables watchdog

   WDRF is bit 3 of MCUSR on ATmega328P.
*/

void captureMCUSR(void)
    __attribute__((naked))
    __attribute__((used))
    __attribute__((section(".init3")));


void captureMCUSR(void)
{
    __asm__ __volatile__(
        "in r24, %0"              "\n\t"
        "sts resetCause, r24"     "\n\t"

        "ldi r24, 0"              "\n\t"
        "out %0, r24"             "\n\t"

        :
        : "I" (_SFR_IO_ADDR(MCUSR))
        : "r24"
    );
}


/* =========================================================
   TASK 1 : INTERRUPT SERVICE ROUTINE
   ========================================================= */

void interruptHandler()
{
    unsigned long currentTime = micros();

    /*
       Simple software debounce.

       Only accept an interrupt if enough time has passed
       since the previous accepted interrupt.
    */

    if ((currentTime - lastInterruptTime) >= DEBOUNCE_TIME)
    {
        interruptCount++;

        /*
           D13 is controlled directly through PORTB.

           Arduino Uno:
           D13 = PB5
        */

        PORTB ^= (1 << PB5);

        lastInterruptTime = currentTime;
    }
}


/* =========================================================
   SETUP
   ========================================================= */

void setup()
{
    /*
       -----------------------------------------------------
       COMMON HARDWARE
       -----------------------------------------------------
    */

    lcd.begin(16, 2);

    Serial.begin(9600);


    /*
       -----------------------------------------------------
       TASK 1 INITIALIZATION
       -----------------------------------------------------
    */

    pinMode(INTERRUPT_PIN, INPUT_PULLUP);

    pinMode(ISR_OUTPUT_PIN, OUTPUT);

    digitalWrite(ISR_OUTPUT_PIN, LOW);

    attachInterrupt(
        digitalPinToInterrupt(INTERRUPT_PIN),
        interruptHandler,
        FALLING
    );


    /*
       Start Task 1 timer.
    */

    task1StartTime = millis();


    /*
       -----------------------------------------------------
       TASK 2 : DETERMINE RESET REASON
       -----------------------------------------------------
    */

    /*
       Copy the early captured MCUSR value.
    */

    effectiveResetCause = resetCause;


    /*
       If MCUSR capture is zero, try the Optiboot R2 value.
    */

    if (effectiveResetCause == 0 &&
        optibootResetFlag != 0)
    {
        effectiveResetCause = optibootResetFlag;
    }


    /*
       -----------------------------------------------------
       ACTUAL HARDWARE WDRF CHECK
       -----------------------------------------------------
    */

    if (effectiveResetCause & (1 << WDRF))
    {
        watchdogResetDetected = 1;
    }
    else
    {
        watchdogResetDetected = 0;
    }


    /*
       -----------------------------------------------------
       EEPROM WATCHDOG MARKER
       -----------------------------------------------------
    */

    uint8_t watchdogMarker = EEPROM.read(WD_MARKER_ADDR);


    /*
       If the previous program execution deliberately enabled
       the watchdog and the MCU restarted, use this as a
       fallback indication of a watchdog reset.

       IMPORTANT:
       WDRF is still printed separately.
    */

    if (watchdogMarker == WATCHDOG_MARKER)
    {
        watchdogResetDetected = 1;

        /*
           Clear marker immediately.

           This prevents the marker from being reused on the
           next normal power-on.
        */

        EEPROM.update(WD_MARKER_ADDR, 0);
    }


    /*
       -----------------------------------------------------
       SERIAL HEADER
       -----------------------------------------------------
    */

    Serial.println();
    Serial.println("==============================");
    Serial.println("        MODULE 3 FIRMWARE");
    Serial.println("==============================");


    /*
       -----------------------------------------------------
       WATCHDOG RESET
       -----------------------------------------------------
    */

    if (watchdogResetDetected)
    {
        /*
           Read previously saved fault code.
        */

        lastFault = EEPROM.read(FAULT_ADDR);


        Serial.println("RESET STATUS");
        Serial.println();

        Serial.print("MCUSR Captured = 0x");
        Serial.println(resetCause, HEX);

        Serial.print("Optiboot Flag  = 0x");
        Serial.println(optibootResetFlag, HEX);

        Serial.print("Effective Cause = 0x");
        Serial.println(effectiveResetCause, HEX);

        Serial.print("WDRF = ");
        Serial.println(
            (effectiveResetCause & (1 << WDRF)) ? 1 : 0
        );

        Serial.println("Reset Reason: WATCHDOG");

        Serial.print("Last Fault Code: ");
        Serial.println(lastFault);


        /*
           LCD
        */

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("RESET: WATCHDOG");

        lcd.setCursor(0, 1);

        if (effectiveResetCause & (1 << WDRF))
        {
            lcd.print("WDRF = 1");
        }
        else
        {
            lcd.print("WDRF = 0");
        }


        /*
           Continue to final Task 2 stage.
        */

        task2Stage = 2;
        task2Timer = millis();
    }


    /*
       -----------------------------------------------------
       POWER-ON / NORMAL START
       -----------------------------------------------------
    */

    else
    {
        /*
           Save sensor fault code.

           This simulates a fault that happened before the
           watchdog recovery.
        */

        lastFault = SENSOR_FAULT;

        EEPROM.update(FAULT_ADDR, lastFault);


        Serial.println("Reset Reason: POWER ON");

        Serial.println("Sensor Fault Detected");

        Serial.print("Saving Fault Code: ");
        Serial.println(lastFault);


        /*
           LCD
        */

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("FAULT SAVED");

        lcd.setCursor(0, 1);
        lcd.print("CODE: ");
        lcd.print(lastFault);


        /*
           Task 2 stage 1:

           Wait without delay().
        */

        task2Stage = 1;
        task2Timer = millis();
    }


    /*
       -----------------------------------------------------
       TASK 1 START MESSAGE
       -----------------------------------------------------
    */

    Serial.println();
    Serial.println("TASK 1: INTERRUPT READY");
    Serial.println("D2 = INTERRUPT INPUT");
    Serial.println("D13 = ISR RESPONSE");
}


/* =========================================================
   LOOP
   ========================================================= */

void loop()
{
    unsigned long currentMillis = millis();


    /* =====================================================
       TASK 1 : INTERRUPT DISPLAY
       ===================================================== */

    /*
       Wait 1.5 seconds without delay().
    */

    if (!task1DisplayDone &&
        (currentMillis - task1StartTime >= 1500UL))
    {
        unsigned long countCopy;


        /*
           Safely copy volatile interrupt counter.
        */

        noInterrupts();

        countCopy = interruptCount;

        interrupts();


        /*
           Serial output
        */

        Serial.println();
        Serial.println("------------------------------");
        Serial.println("TASK 1 RESULT");
        Serial.println("------------------------------");

        Serial.println("D2 = INTERRUPT INPUT");
        Serial.println("D13 = ISR RESPONSE");

        Serial.print("Interrupt Count = ");
        Serial.println(countCopy);


        /*
           LCD output
        */

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("INT COUNT:");

        lcd.print(countCopy);

        lcd.setCursor(0, 1);
        lcd.print("ISR RESPONSE");


        task1DisplayDone = true;
    }


    /* =====================================================
       TASK 2 : POWER-ON PATH
       ===================================================== */

    if (task2Stage == 1)
    {
        /*
           Wait 2 seconds without delay().
        */

        if (currentMillis - task2Timer >= 2000UL)
        {
            Serial.println();
            Serial.println("------------------------------");
            Serial.println("STARTING WATCHDOG");
            Serial.println("------------------------------");

            Serial.println("Timeout: 1 second");


            /*
               IMPORTANT:

               Mark that this program is intentionally about
               to wait for a watchdog reset.

               EEPROM.update() only writes when the value
               changes, reducing unnecessary EEPROM wear.
            */

            EEPROM.update(
                WD_MARKER_ADDR,
                WATCHDOG_MARKER
            );


            /*
               Enable watchdog reset.

               No wdt_reset() is called after this.

               Therefore the watchdog expires and resets
               the ATmega328P.
            */

            wdt_enable(WDTO_1S);

            Serial.println("WATCHDOG ENABLED");

            Serial.flush();


            /*
               Stage 4 means:

               WAIT FOR WATCHDOG RESET

               No delay().
               No blocking loop.
               The CPU simply continues running until the
               watchdog resets it.
            */

            task2Stage = 4;
        }
    }


    /* =====================================================
       TASK 2 : WATCHDOG RESET RESULT
       ===================================================== */

    else if (task2Stage == 2)
    {
        if (currentMillis - task2Timer >= 2000UL)
        {
            lcd.clear();

            lcd.setCursor(0, 0);
            lcd.print("LAST FAULT:");

            lcd.print(lastFault);

            lcd.setCursor(0, 1);
            lcd.print("RECOVERY OK");


            Serial.println();
            Serial.println("------------------------------");
            Serial.println("WATCHDOG RECOVERY");
            Serial.println("------------------------------");

            Serial.print("Last Fault Code: ");
            Serial.println(lastFault);

            Serial.println("Recovery successful");


            task2Stage = 3;
            task2Timer = currentMillis;
        }
    }


    /* =====================================================
       TASK 2 : TEST COMPLETE
       ===================================================== */

    else if (task2Stage == 3)
    {
        if (currentMillis - task2Timer >= 2000UL)
        {
            Serial.println();
            Serial.println("==============================");
            Serial.println("TASK 2 : COMPLETE");
            Serial.println("==============================");

            lcd.clear();

            lcd.setCursor(0, 0);
            lcd.print("TASK 2 COMPLETE");

            lcd.setCursor(0, 1);
            lcd.print("TEST COMPLETE");


            /*
               Stop this stage.

               The watchdog is already disabled by the early
               startup code after reset.
            */

            task2Stage = 5;
        }
    }


    /* =====================================================
       TASK 2 : WAITING FOR WATCHDOG
       ===================================================== */

    else if (task2Stage == 4)
    {
        /*
           INTENTIONALLY EMPTY.

           Do NOT call:

               wdt_reset();

           The watchdog must expire.

           The ATmega328P will reset automatically.
        */
    }


    /* =====================================================
       TASK 2 : FINISHED
       ===================================================== */

    else if (task2Stage == 5)
    {
        /*
           Nothing to do.

           Main application remains running.
        */
    }
}
