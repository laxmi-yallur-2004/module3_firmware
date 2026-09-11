# Module 3 Firmware

This project contains two embedded firmware experiments using Arduino UNO.

## Hardware

* Arduino UNO
* 16x2 LCD
* Jumper wire

## LCD Pins

* RS → 8
* EN → 9
* D4 → 4
* D5 → 5
* D6 → 6
* D7 → 7

## Module 3 - Interrupt

File: `module3_latency/module3_latency.ino`

This program demonstrates an external interrupt using **D2**.

* D2 → Interrupt input
* D13 → ISR response
* D2 is connected to GND using a jumper.
* Interrupt count is displayed on the LCD.
* 50 ms debounce is used.
* `millis()` is used for non-blocking timing.

### Output

```text
MODULE 3
INT LATENCY
```

Then:

```text
D2 = INPUT
D13 = ISR
```

When D2 is triggered:

```text
INT COUNT:1
ISR RESPONSE
```

## Module 4 - Watchdog and Fault Recovery

File: `Reset reson`

This program demonstrates **watchdog reset and fault recovery**.

* Fault code `101` is saved in EEPROM.
* Watchdog timer is started.
* Arduino automatically resets.
* After reset, the saved fault code is retrieved.
* LCD and Serial Monitor show the result.

### Output

```text
Power On
Saving Fault: 101
Starting watchdog...
Reset Reason: WATCHDOG
Last Fault Code: 101
TEST COMPLETE
```

## Result

**Module 3:** External interrupt handling.

**Module 4:** Watchdog reset and EEPROM fault recovery.
