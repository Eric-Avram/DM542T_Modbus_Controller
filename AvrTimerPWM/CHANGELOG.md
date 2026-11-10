# Changelog

## 0.1.0 (unreleased)
First version, ported from the PWM generation code of the DM542T Modbus Controller sketch.

### Added
* `AvrTimerPWM` class driving Timer1 on pins 9 and 10
* Waveform modes: Normal, CTC, Fast PWM, Phase Correct, Phase and Frequency Correct
* `setPWMFrequency()` to configure the timer, with automatic prescaler selection
* `changePWMFrequency()` to change the frequency while the timer is running, without restarting it
* `rampPWMFrequency()` to move the frequency toward a target at a constant acceleration
* `setPWMDuty()` for each pin or both pins
* `pwmOn()` / `pwmOff()` to connect and disconnect each pin
* Frequency limits, with defaults of 125 Hz to 20 kHz
* Error reporting with `getError()` / `resetError()` for rejected requests
* Warning reporting with `getWarning()` / `resetWarning()` for requests that were applied but adjusted
* Getters for the current mode, frequency, duty, TOP, clock select values and connected pins

### Known limitations
* Timer1 on the ATmega328P only; `attach()` is not implemented yet
* Starting or stopping the output can cut or emit a partial pulse
