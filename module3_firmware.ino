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
State-machine based control
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
#define WD_MARKER_ADDR   1


/*
   These variables are placed in .noinit.

   They are captured before normal C startup.
*/

uint8_t resetCause
    __attribute__((section(".noinit")));

uint8_t optibootResetFlag
    __attribute__((section(".noinit")));


/* =========================================================
   WATCHDOG INFORMATION
   ========================================================= */

uint8_t watchdogResetDetected = 0;

uint8_t effectiveResetCause = 0;

uint8_t lastFault = 0;


/* =========================================================
   TASK 2 : STATE MACHINE
   ========================================================= */

enum Task2State
{
    TASK2_POWER_ON,
    TASK2_WATCHDOG_WAIT,
    TASK2_RECOVERY,
    TASK2_COMPLETE,
    TASK2_FINISHED
};

Task2State task2State = TASK2_POWER_ON;


/* =========================================================
   TASK 2 : TIMER
   ========================================================= */

unsigned long task2Timer = 0;


/* =========================================================
   TASK 2 : EARLY OPTIBOOT RESET FLAG CAPTURE
   ========================================================= */

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
   Executes very early during startup.

   1. Read MCUSR
   2. Save MCUSR into resetCause
   3. Clear MCUSR

   WDRF = bit 3 of MCUSR.
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
       Software debounce.
    */

    if ((currentTime - lastInterruptTime) >= DEBOUNCE_TIME)
    {
        interruptCount++;

        /*
           Arduino Uno D13 = PB5
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
    /* -----------------------------------------------------
       COMMON HARDWARE
       ----------------------------------------------------- */

    lcd.begin(16, 2);

    Serial.begin(9600);


    /* -----------------------------------------------------
       TASK 1 INITIALIZATION
       ----------------------------------------------------- */

    pinMode(INTERRUPT_PIN, INPUT_PULLUP);

    pinMode(ISR_OUTPUT_PIN, OUTPUT);

    digitalWrite(ISR_OUTPUT_PIN, LOW);


    attachInterrupt(
        digitalPinToInterrupt(INTERRUPT_PIN),
        interruptHandler,
        FALLING
    );


    task1StartTime = millis();


    /* =====================================================
       TASK 2 : DETERMINE RESET REASON
       ===================================================== */

    effectiveResetCause = resetCause;


    /*
       If MCUSR is zero, check Optiboot flag.
    */

    if (effectiveResetCause == 0 &&
        optibootResetFlag != 0)
    {
        effectiveResetCause = optibootResetFlag;
    }


    /* -----------------------------------------------------
       ACTUAL HARDWARE WDRF CHECK
       ----------------------------------------------------- */

    if (effectiveResetCause & (1 << WDRF))
    {
        watchdogResetDetected = 1;
    }
    else
    {
        watchdogResetDetected = 0;
    }


    /* -----------------------------------------------------
       EEPROM WATCHDOG MARKER
       ----------------------------------------------------- */

    uint8_t watchdogMarker =
        EEPROM.read(WD_MARKER_ADDR);


    if (watchdogMarker == WATCHDOG_MARKER)
    {
        watchdogResetDetected = 1;

        /*
           Clear marker immediately.
        */

        EEPROM.update(
            WD_MARKER_ADDR,
            0
        );
    }


    /* =====================================================
       SERIAL HEADER
       ===================================================== */

    Serial.println();

    Serial.println("==============================");
    Serial.println("        MODULE 3 FIRMWARE");
    Serial.println("==============================");


    /* =====================================================
       WATCHDOG RESET PATH
       ===================================================== */

    if (watchdogResetDetected)
    {
        /*
           Read saved fault.
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
            (effectiveResetCause & (1 << WDRF))
            ? 1
            : 0
        );


        Serial.println("Reset Reason: WATCHDOG");


        Serial.print("Last Fault Code: ");
        Serial.println(lastFault);


        /* -------------------------------------------------
           LCD
           ------------------------------------------------- */

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
           Move to recovery state.

           No delay.
        */

        task2State = TASK2_RECOVERY;

        task2Timer = millis();
    }


    /* =====================================================
       POWER-ON PATH
       ===================================================== */

    else
    {
        /*
           Simulated sensor fault.
        */

        lastFault = SENSOR_FAULT;


        /*
           Save fault code.
        */

        EEPROM.update(
            FAULT_ADDR,
            lastFault
        );


        Serial.println("Reset Reason: POWER ON");

        Serial.println("Sensor Fault Detected");

        Serial.print("Saving Fault Code: ");
        Serial.println(lastFault);


        /* -------------------------------------------------
           LCD
           ------------------------------------------------- */

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("FAULT SAVED");

        lcd.setCursor(0, 1);
        lcd.print("CODE: ");
        lcd.print(lastFault);


        /*
           Start state machine.

           No 2-second artificial wait.

           Watchdog starts immediately.
        */

        task2State = TASK2_POWER_ON;
    }


    /* -----------------------------------------------------
       TASK 1 START MESSAGE
       ----------------------------------------------------- */

    Serial.println();

    Serial.println("TASK 1: INTERRUPT READY");

    Serial.println("D2 = INTERRUPT INPUT");

    Serial.println("D13 = ISR RESPONSE");
}


/* =========================================================
   MAIN LOOP
   ========================================================= */

void loop()
{
    unsigned long currentMillis = millis();


    /* =====================================================
       TASK 1 : INTERRUPT DISPLAY
       ===================================================== */

    /*
       Display result after 1.5 seconds.

       This is non-blocking.
    */

    if (!task1DisplayDone &&
        (currentMillis - task1StartTime >= 1500UL))
    {
        unsigned long countCopy;


        /*
           Safely copy volatile counter.
        */

        noInterrupts();

        countCopy = interruptCount;

        interrupts();


        /* -------------------------------------------------
           SERIAL OUTPUT
           ------------------------------------------------- */

        Serial.println();

        Serial.println("------------------------------");
        Serial.println("TASK 1 RESULT");
        Serial.println("------------------------------");

        Serial.println("D2 = INTERRUPT INPUT");

        Serial.println("D13 = ISR RESPONSE");

        Serial.print("Interrupt Count = ");
        Serial.println(countCopy);


        /* -------------------------------------------------
           LCD OUTPUT
           ------------------------------------------------- */

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("INT COUNT:");
        lcd.print(countCopy);

        lcd.setCursor(0, 1);
        lcd.print("ISR RESPONSE");


        task1DisplayDone = true;
    }


    /* =====================================================
       TASK 2 : STATE MACHINE
       ===================================================== */

    switch (task2State)
    {

        /* =================================================
           STATE 1 : POWER ON
           ================================================= */

        case TASK2_POWER_ON:

            /*
               Save marker immediately before enabling
               the watchdog.
            */

            EEPROM.update(
                WD_MARKER_ADDR,
                WATCHDOG_MARKER
            );


            Serial.println();

            Serial.println("------------------------------");
            Serial.println("STARTING WATCHDOG");
            Serial.println("------------------------------");

            Serial.println("Timeout: 1 second");


            /*
               Enable hardware watchdog.

               No wdt_reset() will be called.

               Therefore the watchdog will expire.
            */

            wdt_enable(WDTO_1S);


            Serial.println("WATCHDOG ENABLED");

            Serial.flush();


            /*
               Move to waiting state.
            */

            task2State = TASK2_WATCHDOG_WAIT;

            break;


        /* =================================================
           STATE 2 : WAIT FOR WATCHDOG RESET
           ================================================= */

        case TASK2_WATCHDOG_WAIT:

            /*
               Intentionally empty.

               Do NOT call wdt_reset().

               The hardware watchdog will expire and
               reset the ATmega328P.

               After reset, setup() runs again and
               MCUSR/WDRF is checked.
            */

            break;


        /* =================================================
           STATE 3 : WATCHDOG RECOVERY
           ================================================= */

        case TASK2_RECOVERY:

            /*
               Recovery is shown immediately after the
               watchdog reset.

               No artificial 2-second wait.
            */

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


            /*
               Move to complete state.
            */

            task2State = TASK2_COMPLETE;

            break;


        /* =================================================
           STATE 4 : COMPLETE
           ================================================= */

        case TASK2_COMPLETE:

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
               Final state.
            */

            task2State = TASK2_FINISHED;

            break;


        /* =================================================
           STATE 5 : FINISHED
           ================================================= */

        case TASK2_FINISHED:

            /*
               Nothing more to do.
            */

            break;
    }
}
