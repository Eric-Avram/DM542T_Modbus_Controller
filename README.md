# DM542T Modbus Controller

## Introduction
A while back, I've partially built the BCN3D Moveo as a personal side project. (see https://github.com/BCN3D/BCN3D-Moveo). Unfortunately, at the time I didn't have any Arduino Mega to test it out, and mostly wanted to integrate it with ROS2, but never got to do it. 

Lately I've been having some extra time so I decided to go back to it, and realized  that I could use this robot as a test bench for Codesys Projects involving robotics, plus it sounds like a fun side project. 

I have some StepperOnline DM542T drivers laying around and some Arduino Nano clones, as well as an old RPI3B+, hence how I got to try to develop this controller. 

## Documentation:
* [Test Results & Dev docs](Tests%20%26%20Results.md)

## Architecture
*TBD*

## Tools:
*  [ModbusTool](https://github.com/ClassicDIY/ModbusTool.git) for testing serial communication 

## Build & Dependencies
| Item | Version / Setting |
|---|---|
| MCU | ATmega328P @ 16 MHz (Arduino Nano clone, CH340 USB, micro-USB) |
| Board setting | Arduino Nano, Processor: ATmega328P (Old Bootloader) |
| Arduino AVR core | arduino:avr 1.8.8 |
| IDE | Arduino IDE 2.3.10 (arduino-cli 1.5.1) |
| Libraries | [Modbus-Serial](https://github.com/epsilonrt/modbus-serial) 2.0.6, [Modbus-Arduino](https://github.com/epsilonrt/modbus-arduino) 1.3.0 (dependency) |
| Serial | USB, 115200 baud, 8E1 |

> **WARNING:** Remove the 10 µF capacitor between RST and GND before uploading firmware (see [Troubleshooting](Tests%20%26%20Results.md#troubleshooting)).

## Wiring
Dashed lines are not connected (floating).

```mermaid
flowchart LR
    PC["CODESYS<br/>Modbus RTU master"]
    PSU["Lab bench power supply<br/>common GND"]
    NANO["Arduino Nano<br/>ATmega328P"]
    R1["R1<br/>22 Ω"]
    C1["C1 10 µF<br/>remove before upload"]
    DRV["StepperOnline<br/>DM542T"]
    MOT["Stepper motor<br/>17HM19"]
    NC["n.c."]

    PC <-->|"USB (CH340)<br/>115200 8E1"| NANO
    PSU -->|"+12 V to VIN<br/>GND to GND"| NANO
    PSU -->|"+24 V to +Vdc<br/>GND to GND"| DRV
    NANO -->|"D9 (PWM)"| R1
    R1 -->|"PUL+"| DRV
    NANO -->|"D8 to DIR+"| DRV
    NANO ---|"GND to PUL- and DIR-"| DRV
    NANO ---|"RST"| C1
    C1 ---|"GND"| NANO
    DRV -.-|"ENA+/ENA-<br/>ALM+/ALM-"| NC
    DRV ---|"A+ / A-: coil A<br/>B+ / B-: coil B"| MOT
```

## Modbus Register Map
Slave ID 1. Addresses are 0-based. Each register type is one contiguous block, so it can be read with a single request.

### Coils (FC01 read, FC05/FC15 write)
| Address | Name | Access | Description |
|---|---|---|---|
| 0x00 | `_coil_dir9` | R/W | Direction output for pin 9 (D8) |
| 0x01 | `_coil_dir10` | R/W | Direction output for pin 10. Not used yet, no DIR pin assigned |
| 0x02 | `_coil_pwm_start9` | R/W | 1 = PWM on pin 9, 0 = off |
| 0x03 | `_coil_pwm_start10` | R/W | 1 = PWM on pin 10, 0 = off |
| 0x04 | `_coil_pwm_configured` | R | 1 = Timer1 configured. Written by the firmware |

### Holding Registers (FC03 read, FC06/FC16 write)
| Address | Name | Access | Unit | Description |
|---|---|---|---|---|
| 0x00 | `_hreg_set_pwm_duty9` | R/W | % | Duty cycle setpoint for pin 9 (0-100) |
| 0x01 | `_hreg_set_pwm_duty10` | R/W | % | Duty cycle setpoint for pin 10 (0-100). Not applied yet |
| 0x02 | `_hreg_pwm_mode` | R/W | - | PWM mode: 0 = NORMAL, 1 = PH_CORRECT, 2 = CTC, 3 = FAST_PWM, 4 = FAST_PWM_PH_CORRECT. Not applied yet |
| 0x03 | `_hreg_pwm_lo_limit` | R/W | Hz | Lower frequency limit (default 125) |
| 0x04 | `_hreg_pwm_hi_limit` | R/W | Hz | Upper frequency limit (default 20000) |
| 0x05 | `_hreg_set_pwm_freq` | R/W | Hz | Frequency setpoint. The output ramps to it at 2000 Hz/s |

### Input Registers (FC04 read)
| Address | Name | Access | Unit | Description |
|---|---|---|---|---|
| 0x00 | `_ireg_pwm_enabled` | R | bitmask | Bit 0 = pin 9 enabled, bit 1 = pin 10 enabled |
| 0x01 | `_ireg_pwm_top` | R | counts | Timer1 TOP (ICR1) |
| 0x02 | `_ireg_pwm_cs` | R | - | Timer1 clock select: 1 = /1, 2 = /8, 3 = /64, 4 = /256, 5 = /1024 |
| 0x03 | `_ireg_pwm_duty9` | R | % | Applied duty cycle on pin 9 |
| 0x04 | `_ireg_pwm_duty10` | R | % | Applied duty cycle on pin 10 |
| 0x05 | `_ireg_pwm_freq` | R | Hz | Requested frequency, the current ramp position after the limits |
| 0x06 | `_ireg_pwm_freq_actual` | R | Hz | Actual frequency from TOP and the prescaler, truncated to whole Hz |

To check if a ramp is finished, compare the setpoint (holding 0x05) with the requested frequency (input 0x05), which is equal to it once the ramp is done. The actual frequency (input 0x06) never matches the setpoint exactly because TOP is rounded, and the difference grows with the frequency (see [Tests & Results](Tests%20%26%20Results.md#11-modbus-rtu)).

## Known Issues
* Starting/stopping PWM (`pwmOn`/`pwmOff`) can cut or emit a partial STEP pulse (±1 step per start/stop). Planned fix: start/stop via OCR1x = 0 in mode 8.
* `mb.task()` blocks for ~3–4 ms per Modbus request.
* Frequency registers are 16-bit, so values above 65 535 Hz can't be written in one register.
* Pin D10 has a PWM channel but no DIR pin yet.
* Modbus performance is affected by the ModbusSerial Library, which is using delay() to send messages in both the send() and sendPDU() functions. The current implementation plan for the whole controller implies havily that the delay function is not used in the loop. 
* The 16 MHz clock of the Nano clone runs ~0.33% slow. The actual frequency register is calculated with exactly 16 MHz, so it reads ~0.33% high (e.g. 18018 Hz vs 17958 Hz measured), and ramps are ~0.64% slower than set (1987 Hz/s instead of 2000 Hz/s). See [Tests & Results](Tests%20%26%20Results.md#11-modbus-rtu).
* With the current Codesys channel setup (4 cyclic write channels and 3 read channels), register values reach Codesys every ~140 ms instead of the configured 100 ms / 50 ms, because the master's request queue never empties. Planned fix: writes on rising edge, one read for all input registers.

---

## Changelog
### V0.1 Default Arduino analogWrite
* Added modbus support using the Arduino provided [Modbus-Serial Lib](https://github.com/epsilonrt/modbus-serial)
* Tested modbus function using [Modbus Tool](https://github.com/ClassicDIY/ModbusTool.git)
* Added prototype for own pwm function
* Added simple analogWrite on modbus trigger. 

### V0.2 Bit-Banging PWM
* Added a bit-banged PWM function (per [Secrets of Arduino PWM](https://docs.arduino.cc/tutorials/generic/secrets-of-arduino-pwm/))
* Added the Test & Results doc and first oscilloscope evidence (analogWrite vs bit-banging)
* Cleaned unused code blocks

### V0.3 Direct PWM via Timer1
* PWM generated directly by Timer1 on pins 9/10; removed all previous PWM generation methods
* Added `ADDR` namespace for Modbus addresses (now in HEX) and `PWM` namespace for PWM data
* Added Modbus addresses for duty, frequency and on/off coils
* Added `pwmOn()`/`pwmOff()` to connect/disconnect Timer1 from pin 9, 10 or both
* Fixed 500 Hz / 50% duty for testing; recorded evidence
* Removed the versioning header from the sketch (changelog is kept here)

### V0.4 Control Word & Register Expansion
* Added more Modbus addresses for motor control and PWM parameters (TOP, CS, limits, ...)
* Added signal generation constraints (122 Hz to 20 kHz for this setup)
* `pwmOn()`/`pwmOff()` now only validate the command and update the status word; `updatePins()` applies it to the port/timer
* Added a wrapper for the on/off control word to avoid redundant code
* Tested with a 22 Ω series resistor: overshoot reduced to ~5%

### V0.5 Timer Setup & Frequency Change
* Added `setTimer()`: sets all Timer1 registers in an atomic block, picks the best prescaler for the operating mode, sets pin modes
* Added helpers for the WGM control bits per mode and for converting duty to OCR
* Split frequency handling:
    * `setPWMFrequency()` sets the frequency and (re)configures the timer; meant for `setup()` or fixed frequencies
    * `changePWMFrequency()` changes the frequency while the timer runs; meant for `loop()` or interrupts
* Glitch-free frequency change: a higher TOP is written directly, a lower TOP is applied by the Timer1 overflow ISR at the next BOTTOM (no ±1 step, ~1 period delay)
* `setPWMDuty()` now also applies the value to the registers
* Added actual-frequency register and Modbus frequency data
* Added default frequency limits (125 Hz to 20 kHz) and lo/hi limit registers; invalid limits fall back to the defaults
* Fixed: duty stored unclamped when set above 100%
* Fixed: requested frequency not recorded when the new TOP equals the old one
* Fixed: duty not recorded before the timer is configured

### V0.6 Porting The PWM Funcitons to separate library
* Moved all the PWM functions from the sketch to the `AvrTimerPWM` library, using the Arduino library folder structure
* Added the library README, CHANGELOG, LICENSE, `keywords.txt` and `library.properties`
* Moved the linear ramp from the sketch to the library as `rampPWMFrequency()`
* Changed the global enums to enum classes to avoid name conflicts

### V0.7 Modbus RTU Register Map & Codesys Testing
* Tested an ENC28J60 Ethernet shield for Modbus TCP: the shield uses D10 for CS/SS, which is one of the Timer1 PWM pins, so Modbus TCP is dropped for now and development continues with Modbus RTU
* Split the register map into coils, holding registers (setpoints and settings) and input registers (read-only values), each as one block without gaps, so Codesys can read each type with a single request
* Moved all the register updates into `write_modbus()` (previously `update_modbus()`)
* Added a Codesys test program (Raspberry Pi as host) that runs a frequency sweep over Modbus RTU; the archive is in the Testing Programs folder
* Measured the sweep with the oscilloscope and with the Codesys trace, and compared both with the simulation
* Added error and warning reporting
* Added Doxygen documentation in the header and organised the code into regions
* Fixed: ramp could get inverted at low frequencies; the new TOP is now compared with ICR1 instead of the previous TOP
* Fixed: last ramp time was stored in a static variable shared by all objects; it is now a private member of the class
* The current library only works for Timer 1, but I am planning to add the code for Timer 0 and 2 in the near future.

## Target Requirements
| ID | Category | Requirement | Status |
|---|---|---|---|
| COM-1 | Communication | Stable Modbus RTU communication at 115200 baud, 8E1, over USB serial | Met |
| COM-2 | Communication | CODESYS compatibility as a Modbus slave | Met |
| PWM-1 | PWM generation | Frequency within ±5% of target | Met at 500 Hz (V0.4); wider range not tested yet |
| PWM-2 | PWM generation | Duty cycle within ±5% of target | Met at 50% (V0.4) |
| PWM-3 | PWM generation | Variable frequency up to 200 kHz (DM542T limit), 50% duty recommended, minimum pulse width 2.5 µs | Partial: 125 Hz to 20 kHz by default |
| PWM-4 | PWM generation | Ramping, to avoid current spikes from the motor | Planned |
| PWM-5 | PWM generation | Configurable timer: PWM ported to a library so each object can be attached to a timer (e.g. `pwm1.attach(timer1)`, `pwm2.attach(timer2)`) | Planned |
| SAF-1 | Safety | Communication watchdog: stop pulse generation if no Modbus request is received within a configurable timeout | Open |
| SAF-2 | Safety | Safe state at power-up and reset: PWM off and outputs LOW until explicitly started by the master | Implemented in `setup()`, not verified |
| SAF-3 | Safety | MCU watchdog (WDT): reset into the safe state if the firmware locks up | Open |
| SAF-4 | Safety | Direction changes only while stopped, or with DIR stable at least 5 µs before the next step edge (DM542T spec) | Open |
| SAF-5 | Safety | Defined stop behaviour: normal stop (ramped, once ramping exists) and emergency stop (immediate) | Open |
| MOT-1 | Motion control | Step counting: emitted-step counter and/or "move N steps" command for open-loop positioning | Open |
| MOT-2 | Motion control | Up to 2 independent axes per Nano (one per free timer: Timer1, Timer2) | Planned |
| MOT-3 | Motion control | 2 synchronized axes on one timer (pins 9 and 10 driven at the same frequency) | Implemented (Timer1) |
| MOT-4 | Motion control | Configurable motion parameters: speed, acceleration, direction | Planned |
| CTL-1 | Control | Error handling and reporting | Open |
| DIA-1 | Diagnostics | Error/status register with defined error codes, including the reason a command was rejected | Open |
| DIA-2 | Diagnostics | Firmware version register | Open |
| DIA-3 | Diagnostics | Communication error counters (CRC errors, timeouts) | Open |
| CFG-1 | Configuration | Input validation: out-of-range writes are rejected or clamped, and the result is reported | Partial: frequency limits |
| CFG-2 | Configuration | Persistent settings in EEPROM (limits, slave ID, baud rate); lowest priority | Planned |
| TIM-1 | Timing | Interrupts disabled for at most 100 µs at a time, so Modbus at 115200 baud doesn't lose bytes | Open, not measured |
| TIM-2 | Timing | Frequency change applied within one PWM period | Implemented (V0.5), not measured |
| REG-1 | Modbus registers | Start/stop PWM | Implemented |
| REG-2 | Modbus registers | Duty setpoint | Implemented |
| REG-3 | Modbus registers | Direction change | Implemented |
| REG-4 | Modbus registers | Frequency setpoint | Implemented |
| REG-5 | Modbus registers | Frequency feedback | Implemented |
| REG-6 | Modbus registers | Frequency generation lower/upper boundaries | Implemented |
| REG-7 | Modbus registers | Connected drive capabilities | TBD |
| REG-8 | Modbus registers | Connected motor capabilities | TBD |
