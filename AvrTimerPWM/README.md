# AvrTimerPWM

Variable-frequency hardware PWM for the ATmega328P (Arduino Nano, Uno), generated directly by Timer1 on pins 9 and 10.

Unlike `analogWrite()`, the frequency can be set by the user and changed while the timer is running. Frequency changes are synchronized with the timer so that no pulses are lost or added, which makes the library suitable for the STEP input of stepper motor drivers.

## Requirements
* ATmega328P. Other microcontrollers are rejected at compile time. The frequency figures below assume a 16 MHz clock.
* Timer1 is used exclusively by this library. Do not combine it with `analogWrite()` on pins 9 and 10, the Servo library, or any other code that uses Timer1.
* The library defines `TIMER1_OVF_vect`, so a sketch can't define its own handler for it.

## Usage
```cpp
#include <AvrTimerPWM.h>

AvrTimerPWM pwm(5000.0f, 0.5f);   // 5 kHz, 50% duty

void setup() {
   pwm.setPWMMode(FAST_PWM_PH_CORRECT);   // configures and starts the timer
   pwm.pwmOn(9);                          // connect the output to pin 9
}

void loop() {
   pwm.changePWMFrequency(6000.0f);       // change it while running
}
```

The constructor only stores the values. Timer registers are written from `setup()` onwards, because the Arduino core reconfigures Timer1 before `setup()` runs.

## API
| Function | Description |
|---|---|
| `AvrTimerPWM(frequency, dutyCycle)` | Stores the frequency in Hz and the duty cycle (0.0 to 1.0) |
| `setPWMMode(mode)` | Sets the waveform mode and configures the timer. Returns the actual frequency |
| `setPWMFrequency(freq, lo, hi)` | Clamps the frequency to the limits and reconfigures the timer, restarting the counter. Returns the actual frequency |
| `changePWMFrequency(freq, lo, hi)` | Changes the frequency without restarting the timer. Returns the actual frequency, or 0 if rejected |
| `setPWMDuty(duty, pin)` | Sets the duty cycle (0.0 to 1.0) on pin 9, pin 10, or both with pin 0 |
| `pwmOn(pin)` / `pwmOff(pin)` | Connects or disconnects pin 9, pin 10, or both with pin 0 |
| `setTimer(mode, freq)` | Low-level timer configuration, without frequency limits. Returns the actual frequency, or 0 if rejected |
| `getPwmFrequencyActual()` / `getPwmFrequencyRequested()` | Last actual and last requested frequency |
| `getPwmDuty9()` / `getPwmDuty10()` | Stored duty cycle per pin |
| `getPwmMode()` / `getPwmTop()` / `getPwmCS()` | Current mode, TOP value and clock select bits |
| `getError()` / `resetError()` | Reads or clears the stored error code |

### Modes
| `PWM_MODE` | AVR mode | Frequency | Duty |
|---|---|---|---|
| `FAST_PWM_PH_CORRECT` | 8, Phase and Frequency Correct | $f_{clk} / (2 \cdot N \cdot TOP)$ | Yes |
| `PH_CORRECT` | 10, Phase Correct | $f_{clk} / (2 \cdot N \cdot TOP)$ | Yes |
| `FAST_PWM` | 14, Fast PWM | $f_{clk} / (N \cdot (TOP + 1))$ | Yes |
| `CTC` | 12, CTC | $f_{clk} / (2 \cdot N \cdot (TOP + 1))$ | No |
| `NORMAL` | 0, Normal | $f_{clk} / (65536 \cdot N)$ | No |

$N$ is the prescaler: 1, 8, 64, 256 or 1024.

`changePWMFrequency()` only works in `FAST_PWM_PH_CORRECT`, the mode used for on-the-fly changes.

## Limits
* Pins 9 and 10 share Timer1, so they always run at the same frequency. The duty cycle is set per pin.
* The frequency limits default to 125 Hz and 20 kHz. Pass `lo` and `hi` to use a different range.
* `changePWMFrequency()` keeps the current prescaler and requires $40 \le TOP \le 65535$. With prescaler 1 that is about 122 Hz to 200 kHz. A frequency outside that range is rejected.
* `changePWMFrequency()` applies a new frequency within about one PWM period.
* Starting or stopping the output with `pwmOn()` / `pwmOff()` isn't synchronized with the timer and can produce one partial pulse.
* `attach()` is not implemented yet. Only Timer1 is supported.

## License
MIT, see [LICENSE](LICENSE).
