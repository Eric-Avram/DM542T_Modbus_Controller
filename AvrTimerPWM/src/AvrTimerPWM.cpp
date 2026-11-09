#include "AvrTimerPWM.h"
// #define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define isValidFrequencyRange (lo_limit, hi_limit)  ((lo_limit > 0) && (hi_limit > 0) && (lo_limit < hi_limit))

static uint16_t dutyToOCR (float duty, uint16_t top, PWM_MODE m);
static uint8_t  getControlWord (uint16_t PIN_NO);
static bool isPwmMode (PWM_MODE mode);
static void wgmBits (PWM_MODE m, uint8_t &a, uint8_t &b);
static volatile uint16_t pwmPendingTop;      // 0 = no frequency change waiting

#pragma region Constructors
AvrTimerPWM::AvrTimerPWM(float frequency, float dutyCycle)
    : pwmError(PWM_ERROR::NONE), pwmMode(PWM_MODE::PH_CORRECT), pwmFreqReq(frequency), 
      pwmFreqActual(0.0f), pwmDuty9(dutyCycle), pwmDuty10(dutyCycle), pwmTop(0), pwmCS(0), 
      pwmEnabled(0), pwmConfigured(false)
{}
#pragma endregion

#pragma region Control
void AvrTimerPWM::pwmOff(uint16_t PIN_NO){   
   pwmEnabled &= ~getControlWord(PIN_NO);
   updatePins();
}

void AvrTimerPWM::pwmOn(uint16_t PIN_NO){
   pwmEnabled |= getControlWord(PIN_NO);
   updatePins();
}

void AvrTimerPWM::attach(TIMER timer) {
    // Implementation for attaching the timer
    return;
}
#pragma endregion

float AvrTimerPWM::setTimer(PWM_MODE mode, float freq) {
   if (freq <= 0.0f)  return 0.0f;
   uint8_t idx  = 0;
   uint16_t top = 0;
   float actual;

   if (mode == NORMAL) {
      // Free-running 16-bit counter: only the prescaler sets the rate (overflow frequency).
      float bestErr = 1e30f;              // get some really smol value
      for (uint8_t i = 0; i < 5; i++) {   // find the prescaler with the best error margin 
         float f = (float)F_CPU / (65536.0f * PWM_PRESCALERS[i]);
         float err = fabsf(f - freq);
         if (err < bestErr) {
            bestErr = err;
            idx = i;
         }
      }
      actual = (float)F_CPU / (65536.0f * PWM_PRESCALERS[idx]);
   } else {  
      // f = F_CPU / (k * N * (TOP + off))
      const float k      = (mode == FAST_PWM) ? 1.0f : 2.0f;
      const float offset = (mode == PH_CORRECT || mode == FAST_PWM_PH_CORRECT) ? 0.0f : 1.0f;
      float t = 0.0f;
      for (; idx < 5; idx++) {               // find the highest prescaler that yields a valid value
         t = (float)F_CPU / (k * PWM_PRESCALERS[idx] * freq) - offset;
         if (t <= 65535.0f) break;
      }
      if (idx == 5 || t < 2.0f) return 0.0f;  // too slow / too fast for this mode
      top    = (uint16_t)lroundf(t);
      actual = (float)F_CPU / (k * PWM_PRESCALERS[idx] * ((float)top + offset));
   }
   const uint8_t cs = idx + 1;

   // Nothing changed -> don't restart the timer
   if (pwmConfigured && mode == pwmMode && top == pwmTop && cs == pwmCS){
      pwmFreqReq = freq;
      pwmFreqActual = actual;
      return actual;
   }

   // Store the new data 
   pwmMode         = mode;
   pwmTop          = top;
   pwmCS           = cs;
   pwmFreqReq      = freq;
   pwmConfigured   = true;
   pwmFreqActual   = actual;

   pinMode(9, OUTPUT);
   pinMode(10, OUTPUT);
   
   uint8_t wa, wb;
   wgmBits(mode, wa, wb);

   const uint8_t com = comBits();
   const uint16_t a = dutyToOCR(pwmDuty9, top, mode);  // math outside the atomic block
   const uint16_t b = dutyToOCR(pwmDuty10, top, mode);

   ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      TIMSK1 &= ~_BV(TOIE1);  
      pwmPendingTop = 0;   // cancel any pending on-the-fly change
      TCCR1B = 0;  // stop the clock
      TCCR1A = 0;  // Normal mode, pins disconnected: OCR1x written immediately
      ICR1 = top;
      OCR1A = a;
      OCR1B = b;
      TCNT1 = 0;
      TCCR1A = wa | com;  // WGM low bits + output mode
      TCCR1B = wb | cs;   // WGM high bits + start the clock
   }
   updatePins();

   return actual;
}

float AvrTimerPWM::changePWMFrequency(float freq_hz, float lo_limit, float hi_limit){
   if (freq_hz <= 0.0f) return 0.0f;
   if ((!pwmConfigured) || (pwmMode != FAST_PWM_PH_CORRECT)) return 0.0f;
   if (lo_limit > hi_limit) return 0.0f;
   freq_hz = constrain(freq_hz, lo_limit, hi_limit);

   const uint16_t _prescaler = PWM_PRESCALERS[pwmCS-1]; // select current prescaler
   const float t = (float)F_CPU / (2.0f * _prescaler * freq_hz);
   if (t > 65535.0f || t < 40.0f) return 0.0f;                    // selected frequency doesn't fit the current prescaler

   const uint16_t top    = (uint16_t)lroundf(t);
   const float    actual = (float)F_CPU / (2.0f * _prescaler * (float)top);
   pwmFreqActual = actual;

   if (top == pwmTop){
      pwmFreqReq = freq_hz;
      return actual;  
   }
   // Need to calculate twice because the timer would cahnge the frequency for both pins
   const uint16_t a = dutyToOCR(pwmDuty9,  top, pwmMode);   
   const uint16_t b = dutyToOCR(pwmDuty10, top, pwmMode);

   ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {        
      if (top > ICR1){            
         // If the new top is higher, new value can be written directly, because the current one will still generate a pulse
         // The new value for the OCR will just be saved in the buffer until the next bottom, when it updates
         // Downside is that this method delays the changes by 1 period from the command, but keeps accurate steps 
         // this helps to avoid any step skips when the new top is < than the old top. 
         TIMSK1 &= ~_BV(TOIE1);          // cancel any pending "frequency up" change
         pwmPendingTop = 0;
         ICR1  = top;
         OCR1A = a;
         OCR1B = b;
      }
      else{
         // Same as before, write the new value after bottom so it loads in the next period
         do {                                               
            TIFR1 = _BV(TOV1);
            OCR1A = a;
            OCR1B = b;
         } while (TIFR1 & _BV(TOV1));

         pwmPendingTop = top;                               // the ISR applies it at the next BOTTOM
         TIMSK1 |= _BV(TOIE1);
      }          
   }

   pwmTop     = top;
   pwmFreqReq = freq_hz;
   return actual;
}

float AvrTimerPWM::setPWMMode(PWM_MODE m) {
  if (pwmFreqReq <= 0.0f) { // frq not set, just remember the pwm mode
    pwmMode = m;
    return 0.0f;
  }  
  return setTimer(m,pwmFreqReq);
}

ISR(TIMER1_OVF_vect) {
  const uint16_t top = pwmPendingTop;
  if (top && (uint16_t)(TCNT1 + 10) < top) {     // counter must not have reached the new TOP yet
    ICR1 = top;
    pwmPendingTop = 0;
    TIMSK1 &= ~_BV(TOIE1);
  }
  // otherwise: try again at the next BOTTOM (the interrupt stays enabled)
}

void AvrTimerPWM::setPWMDuty(float duty, uint8_t pin) {
   if (!(pin == 9) && !(pin == 10) && !(pin == 0)) return;

   duty = constrain(duty, 0.0f, 1.0f);
   if (pin== 0 || pin == 9)  pwmDuty9 = duty;
   if (pin== 0 || pin == 10) pwmDuty10 = duty;
   if (!pwmConfigured || !isPwmMode(pwmMode)) return;

   const uint16_t OCR = dutyToOCR(duty, pwmTop, pwmMode);

   switch (pin){
      case 9:
         ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            OCR1A = OCR;
         }
         break;
      case 10:
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            OCR1B = OCR;
         }
         break;
      case 0:
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            OCR1A = OCR;
            OCR1B = OCR;
         }
         break;
      default:
         return;
   }
}

float AvrTimerPWM::setPWMFrequency(float freqHz,float lo_limit, float hi_limit) { 
   // Constraing the frequency to the set limit. If the limits are invalid, usen the default value
   // More permissive than, changeFrequency() because it resets the timer
   if (lo_limit < hi_limit){   // valid limit values, if not just default 
      return setTimer(pwmMode, constrain(freqHz, (lo_limit >= 0) ? lo_limit : frq_default_lo_limit, 
                                                      (hi_limit > 0) ? hi_limit : frq_default_hi_limit));
   }
   else return setTimer(pwmMode, constrain(freqHz, frq_default_lo_limit, frq_default_hi_limit));
}

uint8_t AvrTimerPWM::comBits() {
  const bool pwm = isPwmMode(pwmMode);
  uint8_t com = 0;
  if (pwmEnabled & 1) com |= pwm ? _BV(COM1A1) : _BV(COM1A0);
  if (pwmEnabled & 2) com |= pwm ? _BV(COM1B1) : _BV(COM1B0);
  return com;
}

void AvrTimerPWM::updatePins() {
  if (!(pwmEnabled & 1)) PORTB &= ~_BV(PB1);
  if (!(pwmEnabled & 2)) PORTB &= ~_BV(PB2);

  TCCR1A = (TCCR1A & 0x0F) | comBits();
}

#pragma region Helper Functions
/********************* HELPER FUNCTIONS **********************/

static bool isPwmMode(PWM_MODE mode) {
  return mode == PH_CORRECT || 
         mode == FAST_PWM   || 
         mode == FAST_PWM_PH_CORRECT;
}

static uint16_t dutyToOCR(float duty, uint16_t top, PWM_MODE m) {
  switch (m) {
    case PH_CORRECT:
    case FAST_PWM_PH_CORRECT:
      return (uint16_t)lroundf(duty * top);  // 0..TOP
    case FAST_PWM:
      {
        uint32_t v = (uint32_t)lroundf(duty * ((float)top + 1.0f));  // period is TOP+1 clocks
        return (v > top) ? top : (uint16_t)v;                        // OCR = TOP -> constant high
      }
    default:
      return 0;  // duty not applicable
  }
}

static uint8_t getControlWord(uint16_t PIN_NO){
   const bool start_W0 = (PIN_NO == 0 || PIN_NO == 9);    
   const bool start_W1 = (PIN_NO == 0 || PIN_NO == 10);  
   // add more here
   uint8_t control = 0;
   if (!start_W0 && !start_W1) return 0;    // nothing to do
   if (start_W0) control |= 0b01;   
   if (start_W1) control |= 0b10; 
   return control;
}

static void wgmBits(PWM_MODE m, uint8_t &a, uint8_t &b) {
  switch (m) {
    case FAST_PWM_PH_CORRECT:
      a = 0;
      b = _BV(WGM13);
      break;  // mode 8
    case PH_CORRECT:
      a = _BV(WGM11);
      b = _BV(WGM13);
      break;  // mode 10
    case CTC:
      a = 0;
      b = _BV(WGM13) | _BV(WGM12);
      break;  // mode 12
    case FAST_PWM:
      a = _BV(WGM11);
      b = _BV(WGM13) | _BV(WGM12);
      break;  // mode 14
    default:
      a = 0;
      b = 0;
      break;  // mode 0
  }
}

#pragma endregion