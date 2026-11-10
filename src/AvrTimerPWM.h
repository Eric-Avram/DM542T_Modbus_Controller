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
         * @brief Constructor
         * @param frequency The desired PWM frequency in Hz
         * @param dutyCycle The desired duty cycle as a percentage (0.0-1.0)
         */
        AvrTimerPWM(float frequency, float dutyCycle);
        AvrTimerPWM();  // Default constructor with default frequency(2000Hz) and duty cycle(50%)

        void attach(TIMER timer);
        void pwmOn (uint16_t PIN_NO = 0);
        void pwmOff(uint16_t PIN_NO = 0);
        float setTimer          (PWM_MODE mode, float freq);
        float setPWMMode        (PWM_MODE m);
        float setPWMFrequency   (float freqHz,  float lo_limit = frq_default_lo_limit, float hi_limit = frq_default_hi_limit);
        float changePWMFrequency(float freq_hz, float lo_limit = frq_default_lo_limit, float hi_limit = frq_default_hi_limit);
        bool  rampPWMFrequency  (float targetHz, float accelHzPerS = 500.0f, float lo_limit = frq_default_lo_limit, float hi_limit = frq_default_hi_limit);
        void  setPWMDuty        (float duty, uint8_t pin = 0        );
        
        void resetError()           { pwmError = PWM_ERROR::NONE; }
        void resetWarning()         { pwmWarning = PWM_WARNING::NONE; }
        PWM_ERROR   getError()      { return pwmError; }
        PWM_WARNING getWarning()    { return pwmWarning; }
        PWM_MODE    getPwmMode()    { return pwmMode;  }

        float     getPwmFrequencyActual()    { return pwmFreqActual; }
        float     getPwmFrequencyRequested() { return pwmFreqReq;    }
        float     getPwmDuty9()  { return pwmDuty9;  }
        float     getPwmDuty10() { return pwmDuty10; }  

        uint16_t  getPwmTop()    { return pwmTop;    }
        uint8_t   getPwmCS()     { return pwmCS;     }  

        uint8_t   getPwmStatus()    { return pwmEnabled;    }                                   // bit0 = pin 9 connected, bit1 = pin 10 connected
        // bool      isPwmEnabled(uint8_t pin) { return (pwmEnabled & getControlWord(pin)) != 0; } // Check if PWM is enabled for a specific pin
        bool      isPwmConfigured() { return pwmConfigured; }
        // bool      isValidFrequencyRange (const uint16_t &lo_limit, const uint16_t &hi_limit) { return (lo_limit > 0 && hi_limit > 0 && lo_limit < hi_limit); }
        bool      isValidFrequencyRange (const float &lo_limit, const float &hi_limit)       { return (lo_limit > 0 && hi_limit > 0 && lo_limit < hi_limit); }


    private:
        PWM_ERROR  pwmError;
        PWM_WARNING pwmWarning;
        PWM_MODE   pwmMode;
        float      pwmFreqReq;       // last successfully applied frequency request
        float      pwmFreqActual;    // last successfully applied frequency actual
        float      pwmDuty9;
        float      pwmDuty10;
        uint16_t   pwmTop;
        uint8_t    pwmCS;

        uint8_t   pwmEnabled;       // bit0 = pin 9 connected, bit1 = pin 10 connected
        bool      pwmConfigured;

        uint8_t comBits();
        void updatePins();
        void raiseError(PWM_ERROR e) { if (pwmError == PWM_ERROR::NONE) pwmError = e; }  // keeps the first error until resetError()
        uint32_t rampLastMs;
 };