#pragma once
#include <Arduino.h>
#include <util/atomic.h>   

#if !defined(__AVR_ATmega328P__)
#error "AvrTimerPWM currently supports the ATmega328P only"
#endif

/**
 * @enum TIMER
 * @brief AVR timers selection
 */
enum class TIMER {
    TIMER_0 = 0,
    TIMER_1 = 1,
    TIMER_2 = 2
};
/**
 * @enum PWM_MODE
 * @brief PWM modes for AVR timers, see datasheet 
 * for details at https://avr-guide.github.io/timers-on-the-atmega328/
 */
enum class PWM_MODE:uint8_t {
    NORMAL,
    PH_CORRECT,
    CTC,
    FAST_PWM,
    FAST_PWM_PH_CORRECT
};
/**
 * @enum PWM_ERROR
 * @brief Error codes for PWM configuration
 */
enum class PWM_ERROR {
    NONE = 0,
    PWM_MODE_INCORRECT,
    PWM_UNCONFIGURED,
    FRQ_TOO_LOW,
    FRQ_OUT_OF_BOUNDS,
    PRESCALER_OUT_OF_BOUNDS,
    FRQ_HI_LO_INVALID,
    ACCEL_INVALID,
    PIN_INVALID
};
/**
 * @enum PWM_WARNING
 * @brief The request was applied, but had to be adjusted
 */
enum class PWM_WARNING {
    NONE = 0,
    FRQ_CONSTRAINTED_LO,
    FRQ_CONSTRAINTED_HI,
    FRQ_HI_LO_INVALID,
    DUTY_CONSTRAINED
};

constexpr uint16_t PWM_PRESCALERS[] = { 1, 8, 64, 256, 1024 }; // add up to 8 when implementing timer 2
constexpr uint16_t frq_default_lo_limit = 125;  // prescaler 1 gives ~122.07Hz, so choose a value above that to make sure default is valid.
constexpr uint16_t frq_default_hi_limit = 20000;

/**
 * @class AvrTimerPWM
 * @brief Class to generate PWM signals using AVR timers
 */

 class AvrTimerPWM {
    public:
        /**
         * @brief Constructor, only stores the values
         * Call setPWMMode() or setPWMFrequency() in setup() to apply them.
         * @param frequency The desired PWM frequency in Hz
         * @param dutyCycle The desired duty cycle as a fraction (0.0-1.0)
         */
        AvrTimerPWM(float frequency, float dutyCycle);
        /**
         * @brief Default constructor, only stores the default frequency (2000Hz) and duty cycle (50%)
         * Call setPWMMode() or setPWMFrequency() in setup() to apply them.
         */
        AvrTimerPWM();

        void attach(TIMER timer);

        /**
         * @brief Turn on PWM output for the specified pin
         * @param PIN_NO The pin number (9 or 10). 0 or none specified will enable both pins.
         * @warning Raises PWM_UNCONFIGURED if the timer isn't configured, and PIN_INVALID for any other pin number.
         */
        void pwmOn (uint16_t PIN_NO = 0);

        /**
         * @brief Turn off PWM output for the specified pin
         * @param PIN_NO The pin number (9 or 10). 0 or none specified will disable both pins.
         * @warning Raises PIN_INVALID for any other pin number.
         */
        void pwmOff (uint16_t PIN_NO = 0);

        /**
         * @brief Set the timer configuration
         * Applies the frequency and the stored duty cycle, selecting the prescaler automatically.
         * @note Ignores the frequency limits, and restarts the counter if the configuration changed.
         * @param mode The PWM mode
         * @param freq The desired frequency
         * @return The actual frequency achieved, or 0 if rejected
         */
        float setTimer (PWM_MODE mode, float freq);

        /**
         * @brief Set the PWM mode
         * If no frequency is set yet, it just remembers the mode for later use.
         * If a frequency is already set, it applies the new mode to the timer.
         * @param m The desired PWM mode
         * @return The actual frequency achieved, or 0 if no frequency is set or the request was rejected
         */
        float setPWMMode (PWM_MODE m);

        /**
         * @brief Set the PWM frequency and configure the timer
         * This function constrains the frequency within the specified limits. If the limits are invalid, default values are used.
         * It restarts the counter if the configuration changed, so use changePWMFrequency() while the timer is running.
         * @param freqHz The desired frequency in Hz
         * @param lo_limit The lower limit for the frequency
         * @param hi_limit The upper limit for the frequency
         * @warning A warning is raised if the limits are invalid or the frequency was constrained.
         * @warning Raises FRQ_TOO_LOW or FRQ_OUT_OF_BOUNDS if no timer setting can produce the frequency.
         * @return The actual frequency achieved, or 0 if rejected
         */
        float setPWMFrequency   (float freqHz,  float lo_limit = frq_default_lo_limit, float hi_limit = frq_default_hi_limit);

        /**
         * @brief Change the PWM frequency while the timer is running
         * This function constrains the frequency within the specified limits and keeps the current prescaler.
         * The new frequency is applied within one PWM period.
         * @warning It only works in FAST_PWM_PH_CORRECT mode.
         * @param freq_hz The desired frequency in Hz
         * @param lo_limit The lower limit for the frequency
         * @param hi_limit The upper limit for the frequency
         * @warning Raises PWM_UNCONFIGURED or PWM_MODE_INCORRECT if the timer isn't configured or is in another mode.
         * @warning Raises FRQ_TOO_LOW for a frequency of 0 or below, and FRQ_HI_LO_INVALID if lo_limit is above hi_limit.
         * @warning Raises PRESCALER_OUT_OF_BOUNDS if the frequency doesn't fit the current prescaler.
         * @warning If the frequency is outside the limits, a warning will be raised and the frequency will be constrained to the limits.
         * @return The actual frequency achieved, or 0 if rejected
         */
        float changePWMFrequency(float freq_hz, float lo_limit = frq_default_lo_limit, float hi_limit = frq_default_hi_limit);

        /**
         * @brief Ramps the PWM frequency towards a target frequency at a specified acceleration rate.
         * This function should be called every loop() to gradually change the PWM frequency towards the target frequency.
         * It uses the changePWMFrequency() function to update the frequency, and it keeps track of the last time it was called to calculate the time delta for ramping.
         * If the limits are invalid, default values are used and a warning is raised.
         * @note The ramping is done in a linear fashion based on the specified acceleration rate (Hz/s).
         * @param targetHz The target frequency in Hz
         * @param accelHzPerS The acceleration in Hz/s (or steps/s^2, default is 500 Hz/s)
         * @param lo_limit The lower frequency limit
         * @param hi_limit The upper frequency limit
         * @return Returns true while ramping, false when on target or if the request failed.
         */
        bool rampPWMFrequency (float targetHz, float accelHzPerS = 500.0f, float lo_limit = frq_default_lo_limit, float hi_limit = frq_default_hi_limit);

        /**
         * @brief Sets the PWM duty cycle for a specific pin.
         * @param duty The duty cycle as a fraction (0.0 to 1.0).
         * @param pin The pin number (9 or 10), use 0 or none for both pins.
         * @note If the timer isn't configured yet, the value is stored and applied later.
         * @warning Raises PIN_INVALID for any other pin number.
         * @warning If the duty cycle is out of bounds, a warning will be raised and the duty cycle will be constrained to the limits.
         */
        void setPWMDuty (float duty, uint8_t pin = 0);
        
        /**
         * @brief Resets the error state of the PWM controller.
         * @note The first error is kept until this is called, so a new error can only be recorded after clearing it.
         */
        void resetError() { pwmError = PWM_ERROR::NONE; }

        /**
         * @brief Resets the warning state of the PWM controller.
         */
        void resetWarning() { pwmWarning = PWM_WARNING::NONE; }

        /**
         * @brief Get the current error state of the PWM controller.
         * @return The current error state as a PWM_ERROR enum value.
         */
        PWM_ERROR getError() { return pwmError; }

        /**
         * @brief Get the current warning state of the PWM controller.
         * @return The current warning state as a PWM_WARNING enum value.
         */
        PWM_WARNING getWarning() { return pwmWarning; }

        /**
         * @brief Get the current PWM mode of the controller.
         * @return The current PWM mode as a PWM_MODE enum value.
         */
        PWM_MODE getPwmMode() { return pwmMode;  }

        /**
         * @brief Get the current PWM frequency of the controller.
         * It is updated by every successful call that sets the frequency.
         * @note This function returns the actual frequency calculated from the timer settings, which may differ slightly from the requested frequency because TOP is rounded to an integer.
         * The measured frequency(hw) may also slightly differ from the actual frequency(sw) due to equipment tolerances.
         * However, the error is usually reasonably small.
         * @return The current PWM frequency as a float value.
         */
        float getPwmFrequencyActual() { return pwmFreqActual; }

        /**
         * @brief Get the last requested PWM frequency of the controller.
         * @note This is the last accepted request after being constrained to the limits, so a request above the upper limit reads back as the upper limit.
         * It differs from the actual frequency only because TOP is rounded to an integer. Rejected requests don't change it.
         * @return The last requested PWM frequency as a float value.
         */
        float getPwmFrequencyRequested() { return pwmFreqReq;    }

        /**
         * @brief Get the current PWM duty cycle for pin 9.
         * @return The current duty cycle for pin 9 as a float value (0.0 to 1.0).
         */
        float getPwmDuty9() { return pwmDuty9;  }

        /**
         * @brief Get the current PWM duty cycle for pin 10.
         * @return The current duty cycle for pin 10 as a float value (0.0 to 1.0).
         */
        float getPwmDuty10() { return pwmDuty10; }  

        /**
         * @brief Get the current PWM TOP value.
         * @return The current TOP value as a uint16_t.
         */
        uint16_t getPwmTop() { return pwmTop;    }

        /**
         * @brief Get the current Timer1 clock select bits.
         * @return The clock select value: 1 = /1, 2 = /8, 3 = /64, 4 = /256, 5 = /1024.
         */
        uint8_t getPwmCS() { return pwmCS;     }  

        /**
         * @brief Get the current PWM status.
         * This function returns a bitmask representing the current status of the PWM controller, indicating which pins are currently enabled for PWM output.
         * The status is represented as a uint8_t, where each bit corresponds to a specific pin:
         * - Bit 0: Pin 9 (OC1A) status (1 = enabled, 0 = disabled)
         * - Bit 1: Pin 10 (OC1B) status (1 = enabled, 0 = disabled)
         * @return The current PWM status as a uint8_t.
         */
        uint8_t getPwmStatus() { return pwmEnabled;    }

        /**
         * @brief Check if the PWM is configured.
         * @return True if the PWM is configured, false otherwise.
         */
        bool isPwmConfigured() { return pwmConfigured; }

        /**
         * @brief Check if a frequency limiter range is valid.
         * @param lo_limit The lower limit of the frequency range.
         * @param hi_limit The upper limit of the frequency range.
         * @return True if the frequency range is valid, false otherwise.
         */
        bool isValidFrequencyRange (const float &lo_limit, const float &hi_limit) { return (lo_limit > 0 && hi_limit > 0 && lo_limit < hi_limit); }


    private:
        PWM_ERROR  pwmError;
        PWM_WARNING pwmWarning;
        PWM_MODE   pwmMode;         
        float      pwmFreqReq;       // last successfully applied frequency request
        float      pwmFreqActual;    // last successfully applied frequency actual
        float      pwmDuty9;         // Timer 1 pin 9 duty cycle (0.0-1.0)
        float      pwmDuty10;        // Timer 1 pin 10 duty cycle (0.0-1.0)
        uint16_t   pwmTop;           // Timer 1 TOP value
        uint8_t    pwmCS;

        uint8_t   pwmEnabled;       // bit0 = pin 9 connected, bit1 = pin 10 connected
        bool      pwmConfigured;

        uint8_t comBits();
        void updatePins();
        void raiseError(PWM_ERROR e) { if (pwmError == PWM_ERROR::NONE) pwmError = e; }  // keeps the first error until resetError()
        uint32_t rampLastMs;
 };