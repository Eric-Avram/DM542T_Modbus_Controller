# Changelog

## 0.1.0 (unreleased)
First version, ported from the PWM generation code of the DM542T Modbus Controller sketch.

### Added
* `AvrTimerPWM` class driving Timer1 on pins 9 and 10
* Waveform modes: Normal, CTC, Fast PWM, Phase Correct, Phase and Frequency Correct
* `setPWMFrequency()` to configure the timer, with automatic prescaler selection
* `changePWMFrequency()` to change the frequency while the timer is running, without restarting it
* `setPWMDuty()` for each pin or both pins
* `pwmOn()` / `pwmOff()` to connect and disconnect each pin
* Frequency limits, with defaults of 125 Hz to 20 kHz
* Getters for the current mode, frequency, duty, TOP and clock select values

### Known limitations
* Timer1 on the ATmega328P only; `attach()` is not implemented yet
* Starting or stopping the output can cut or emit a partial pulse
