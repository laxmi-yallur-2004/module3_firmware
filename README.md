# Module 3 Firmware

## What is this project?

This project demonstrates **two embedded concepts** using Arduino UNO:

1. **Interrupt and ISR**
2. **Watchdog Reset and Reset Reason**

---

# Hardware

* Arduino UNO
* 16x2 LCD
* Jumper wires

### LCD Connection

| LCD | Arduino |
| --- | ------- |
| RS  | D8      |
| EN  | D9      |
| D4  | D4      |
| D5  | D5      |
| D6  | D6      |
| D7  | D7      |

---

# Task 1 – Interrupt

## What is an Interrupt?

An interrupt means:

> When an important event happens, the Arduino temporarily stops its normal work and immediately handles that event.

In this project:

```text
D2 → Interrupt Input
D13 → ISR Response
```

---

## How it works

D2 is normally HIGH because we use:

```cpp
pinMode(D2, INPUT_PULLUP);
```

When D2 is connected to GND:

```text
HIGH → LOW
```

an interrupt occurs.

The Arduino then runs the ISR:

```cpp
void interruptHandler()
```

---

## What does the ISR do?

The ISR:

1. Checks debounce time
2. Increases interrupt count
3. Toggles D13

The count is increased using:

```cpp
interruptCount++;
```

D13 is toggled using:

```cpp
PORTB ^= (1 << PB5);
```

D13 is connected to **PB5** inside the ATmega328P.

---

## Simple Flow

```text
Press / connect D2 to GND
          ↓
      Interrupt
          ↓
         ISR
          ↓
   Increase Count
          ↓
      Toggle D13
          ↓
   Display on LCD
```

---

## Debouncing

Mechanical buttons can create multiple unwanted signals.

So the code waits for:

```text
50 milliseconds
```

between accepted interrupts.

This prevents one button press from being counted many times.

---

# Task 2 – Watchdog Reset

## What is Watchdog?

A watchdog is like a **safety timer**.

If the program gets stuck and does not reset the watchdog timer, the watchdog automatically resets the Arduino.

---

## In this project

The watchdog timeout is:

```text
1 second
```

The code enables it using:

```cpp
wdt_enable(WDTO_1S);
```

The program intentionally does not call:

```cpp
wdt_reset();
```

So after about 1 second:

```text
Watchdog expires
       ↓
Arduino resets
```

---

# Reset Reason

After the Arduino resets, we want to know:

> Why did the Arduino reset?

The ATmega328P provides the:

```text
MCUSR
```

register.

MCUSR means:

**MCU Status Register**

It contains reset information.

---

# WDRF

The code checks:

```cpp
if (resetCause & (1 << WDRF))
```

`WDRF` means:

**Watchdog System Reset Flag**

If WDRF is set:

```text
WDRF = 0
```

it means:

> The previous reset was caused by the watchdog.

---

# EEPROM

The project stores the fault code:

```text
101
```

in EEPROM.

EEPROM is memory that keeps its data even after the Arduino resets or powers off.

So the sequence is:

```text
Save Fault Code 101
        ↓
Watchdog Reset
        ↓
Arduino starts again
        ↓
Read EEPROM
        ↓
Find Fault Code 101
```

---

# Simple Watchdog Flow

```text
Arduino Starts
      ↓
Save Fault Code 101
      ↓
Start Watchdog
      ↓
Wait for Watchdog
      ↓
Watchdog expires
      ↓
Arduino Resets
      ↓
Check MCUSR
      ↓
WDRF = 1
      ↓
Reset Reason = WATCHDOG
      ↓
Read Fault Code
      ↓
Display 101
```

---

# What We Demonstrated

### Task 1

We demonstrated:

* External interrupt
* ISR
* Interrupt counting
* Debouncing
* D13 response

### Task 2

We demonstrated:

* Watchdog timer
* Automatic reset
* Reset reason detection
* MCUSR
* WDRF
* EEPROM fault storage

---

# Important Result

The main learning from this module is:

```text
INTERRUPT
   ↓
Handle important event immediately

WATCHDOG
   ↓
Detect and recover from a stuck program

MCUSR + WDRF
   ↓
Find why the Arduino reset

EEPROM
   ↓
Remember the previous fault
```

---

# Conclusion

Module 3 teaches how embedded firmware can:

**respond to external events using interrupts and recover from software problems using the watchdog timer.**
