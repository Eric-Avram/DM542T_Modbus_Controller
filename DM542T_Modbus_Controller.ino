/****************************************************
 * Project Name: DM542T Modbus Controller
 * Author: 
 * Date: 28/10/2026
 ****************************************************/
#define MbSerial Serial
#define Baudrate 115200

#pragma region Libraries 
   #include <ModbusSerial.h>
   #include <util/atomic.h>   
#pragma endregion

#pragma region Pinout
   constexpr uint8_t PUL_PIN {9U};
   constexpr uint8_t DIR_PIN {8U};
#pragma endregion

// Namespace for storing modbus addresses
namespace ADDR {

   //** Low -level control 0x00 - 0x09
   constexpr uint8_t _hreg_set_pwm_duty9 = 0x04;
   constexpr uint8_t _hreg_set_pwm_duty10 = 0x05;
   constexpr uint8_t _hreg_set_pwm_freq = 0x06;

   //** PWM Settings 0x0A - 0x14 (10-20)
   constexpr uint8_t _hreg_pwm_enabled = 0x0A;
   constexpr uint8_t _hreg_pwm_mode = 0x0B;
   constexpr uint8_t _hreg_pwm_top = 0x0C;
   constexpr uint8_t _hreg_pwm_cs = 0x0D;
   constexpr uint8_t _hreg_pwm_duty9 = 0x0E;   // R
   constexpr uint8_t _hreg_pwm_duty10 = 0x0F;  // R
   constexpr uint8_t _hreg_pwm_freq = 0x10;    // R
   constexpr uint8_t _hreg_pwm_freq_actual = 0x11;
   constexpr uint8_t _hreg_pwm_lo_limit = 0x12;
   constexpr uint8_t _hreg_pwm_hi_limit = 0x13;

   constexpr uint8_t _coil_pwm_configured = 0x00;
   constexpr uint8_t _coil_dir9 = 0x01;
   constexpr uint8_t _coil_pwm_start9 = 0x02;
   constexpr uint8_t _coil_dir10 = 0x03;
   constexpr uint8_t _coil_pwm_start10 = 0x04;

   //** Stepper Driver Settings 0x1E - 0x28 (30-40)
   constexpr uint8_t _hreg_stepper_power = 0x1E;
   constexpr uint8_t _hreg_stepper_resolution = 0x1F;
   constexpr uint8_t _err_freq_change = 0x64;
}

namespace PWM {
   constexpr uint16_t PWM_PRESCALERS[] = { 1, 8, 64, 256, 1024 }; // add up to 8 when implementing timer 2
   constexpr uint16_t frq_default_lo_limit = 125;  // prescaler 1 gives ~122.07Hz, so choose a value above that to make sure default is valid.
   constexpr uint16_t frq_default_hi_limit = 20000;
   enum PWM_MODE {
      NORMAL,
      PH_CORRECT,
      CTC,
      FAST_PWM,
      FAST_PWM_PH_CORRECT
   };
   enum PWM_ERROR {
      NONE = 0,
      PWM_MODE_INCORRECT,
      PWM_UNCONFIGURED,
      FRQ_TOO_LOW,
      PRESCALER_OUT_OF_BOUNDS,
      FRQ_CONSTRAINTED_LO,
      FRQ_CONSTRAINTED_HI,
      FRQ_OUT_OF_BOUNDS,
      FRQ_HI_LO_INVALID
   };
   static PWM_ERROR  pwmError = NONE;
   static PWM_MODE   pwmMode = PH_CORRECT;
   static float      pwmFreqReq = 0.0f;  // last successfully applied frequency request
   static float      pwmFreqActual = 0.0f;
   static float      pwmDuty9 = 0.0f;
   static float      pwmDuty10 = 0.0f;
   static uint16_t   pwmTop = 0;
   static uint8_t    pwmCS = 0;
   static uint8_t    pwmEnabled = 0;  // bit0 = pin 9 connected, bit1 = pin 10 connected
   static bool       pwmConfigured = false;
}

static volatile uint16_t pwmPendingTop = 0;      // 0 = no frequency change waiting

#pragma region Objects & Handlers
   ModbusSerial mb (MbSerial, 1);
#pragma endregion

#pragma region Function Prototypes
void pwmOn (uint16_t PIN_NO = 0);
void pwmOff(uint16_t PIN_NO = 0);

static float setTimer   (PWM::PWM_MODE mode, float freq);
float setPWMMode        (PWM::PWM_MODE m);
float setPWMFrequency   (float freqHz,  float lo_limit = PWM::frq_default_lo_limit, float hi_limit = PWM::frq_default_hi_limit);
float changePWMFrequency(float freq_hz, float lo_limit = PWM::frq_default_lo_limit, float hi_limit = PWM::frq_default_hi_limit);
void  setPWMDuty        (float duty, uint8_t pin = 0);

static uint16_t dutyToOCR     (float duty, uint16_t top, PWM::PWM_MODE m);
static uint8_t  comBits       ();
static void     wgmBits       (PWM::PWM_MODE m, uint8_t &a, uint8_t &b);
uint8_t         getControlWord(uint16_t PIN_NO);
static bool     isPwmMode     (PWM::PWM_MODE mode);

static void updatePins   ();
void        update_params();
#pragma endregion

void setup() {
   MbSerial.begin (Baudrate, MB_PARITY_EVEN); // prefer this line in accordance with the modbus standard.
   while (! MbSerial);
   mb.config (Baudrate);
   
   pinMode(DIR_PIN, OUTPUT);
   pinMode(PUL_PIN, OUTPUT);

   delay(250);

   setPWMMode(PWM::FAST_PWM_PH_CORRECT);
   PWM::pwmFreqActual = setPWMFrequency(5000.0f);
   setPWMDuty(0.5f, 9);
   pwmOff();   // reset, clean state

   delay(250);
   // registers hold a value of max 65535
   // to write higher values, split them, in 2 16-bit registers.
   mb.setAdditionalServerData ("DM542T CONTROLLER"); //Report Server ID function (0x11) 

   mb.addHreg(ADDR::_hreg_pwm_cs, PWM::pwmCS);
   mb.addHreg(ADDR::_hreg_pwm_duty9, (int)(PWM::pwmDuty9 * 100));
   mb.addHreg(ADDR::_hreg_pwm_duty10, (int)(PWM::pwmDuty10 * 100));
   mb.addHreg(ADDR::_hreg_pwm_enabled, PWM::pwmEnabled);
   mb.addHreg(ADDR::_hreg_pwm_freq, PWM::pwmFreqReq);
   mb.addHreg(ADDR::_hreg_pwm_mode, PWM::pwmMode);
   mb.addHreg(ADDR::_hreg_pwm_top, PWM::pwmTop);
   mb.addHreg(ADDR::_hreg_pwm_lo_limit, PWM::frq_default_lo_limit);
   mb.addHreg(ADDR::_hreg_pwm_hi_limit, PWM::frq_default_hi_limit);


   mb.addHreg(ADDR::_hreg_set_pwm_duty9,  (int)(PWM::pwmDuty9 * 100));
   mb.addHreg(ADDR::_hreg_set_pwm_duty10, (int)(PWM::pwmDuty10 * 100));
   mb.addHreg(ADDR::_hreg_set_pwm_freq, PWM::pwmFreqReq);
   mb.addHreg(ADDR::_hreg_pwm_freq_actual, PWM::pwmFreqActual);

   mb.addCoil(ADDR::_coil_pwm_configured, PWM::pwmConfigured);
   mb.addCoil(ADDR::_coil_dir9, false);
   mb.addCoil(ADDR::_coil_pwm_start9, false);
   mb.addCoil(ADDR::_coil_dir10, false);
   mb.addCoil(ADDR::_coil_pwm_start10, false);

   delay(250);
   update_params();  // update modbus data
}

#define pwmPin(condition, pin) (condition)? pwmOn(pin) : pwmOff(pin);

void loop() {
   mb.task();
   digitalWrite(DIR_PIN, mb.coil(ADDR::_coil_dir9));

   pwmPin(mb.coil(ADDR::_coil_pwm_start9), 9);
   pwmPin(mb.coil(ADDR::_coil_pwm_start10), 10);

   if (mb.Hreg(ADDR::_hreg_set_pwm_duty9) != mb.Hreg(ADDR::_hreg_pwm_duty9)){
      setPWMDuty((mb.Hreg(ADDR::_hreg_set_pwm_duty9)/100.0f), 9);
      update_params();
   }
   if (mb.Hreg(ADDR::_hreg_set_pwm_freq) != mb.Hreg(ADDR::_hreg_pwm_freq)){
      // setPWMFrequency(mb.Hreg(ADDR::_hreg_set_pwm_freq));
      const uint16_t freq_hz =  mb.Hreg(ADDR::_hreg_set_pwm_freq);
      const uint16_t lo_limit = mb.hreg(ADDR::_hreg_pwm_lo_limit);   
      const uint16_t hi_limit = mb.hreg(ADDR::_hreg_pwm_hi_limit);
      if (isValidFrequencyRange(lo_limit, hi_limit)){
         if (PWM::pwmError == PWM::FRQ_HI_LO_INVALID) PWM::pwmError = PWM::NONE; // reset error once the range reset
         PWM::pwmFreqActual = changePWMFrequency((float)freq_hz, lo_limit, hi_limit);
      }
      else {
         PWM::pwmError = PWM::FRQ_HI_LO_INVALID;   
         PWM::pwmFreqActual = changePWMFrequency((float)freq_hz, PWM::frq_default_lo_limit, PWM::frq_default_hi_limit);
      }
      update_params();
   }
}


#pragma region Functions
// ================ Basic Control ================

void pwmOff(uint16_t PIN_NO){   
   PWM::pwmEnabled &= ~getControlWord(PIN_NO);
   updatePins();
}

void pwmOn(uint16_t PIN_NO){
   PWM::pwmEnabled |= getControlWord(PIN_NO);
   updatePins();
}

static void updatePins() {
  if (!(PWM::pwmEnabled & 1)) PORTB &= ~_BV(PB1);
  if (!(PWM::pwmEnabled & 2)) PORTB &= ~_BV(PB2);

  TCCR1A = (TCCR1A & 0x0F) | comBits();
}

static bool isPwmMode(PWM::PWM_MODE mode) {
  return mode == PWM::PH_CORRECT || 
         mode == PWM::FAST_PWM   || 
         mode == PWM::FAST_PWM_PH_CORRECT;
}
// ================ /Basic Control/ ================

// ================  PWM Control  ================

// Set frequency in Hz (shared by pins 9 and 10). Returns the actual frequency, or 0. 
//TODO:: Add a way to also select which timer. (Maybe wrap this up in a lib, add pointers to OCRA/OCRB registers and use that)
bool isValidFrequencyRange(const uint16_t &lo_limit, const uint16_t &hi_limit){
   return (lo_limit > 0 && hi_limit > 0 && lo_limit < hi_limit);
}

float setPWMFrequency(float freqHz,float lo_limit, float hi_limit) { 
   // Constraing the frequency to the set limit. If the modbus server limits are invalid, usen the default value
   // More permissive than, changeFrequency() because it resets the timer
   if (lo_limit < hi_limit){   // valid limit values, if not just default 
      return setTimer(PWM::pwmMode, constrain(freqHz, (lo_limit >= 0) ? lo_limit : PWM::frq_default_lo_limit, 
                                                      (hi_limit > 0) ? hi_limit : PWM::frq_default_hi_limit));
   }
   else return setTimer(PWM::pwmMode, constrain(freqHz, PWM::frq_default_lo_limit, PWM::frq_default_hi_limit));
}

float setPWMMode(PWM::PWM_MODE m) {
  if (PWM::pwmFreqReq <= 0.0f) { // frq not set, just remember the pwm mode
    PWM::pwmMode = m;
    return 0.0f;
  }  
  return setTimer(m, PWM::pwmFreqReq);
}

// Duty 0.0 - 1.0 for pin 9, 10, or both (0). Only has an effect in the PWM modes. 
void setPWMDuty(float duty, uint8_t pin) {
   if (!(pin == 9) && !(pin == 10) && !(pin == 0)) return;

   duty = constrain(duty, 0.0f, 1.0f);
   if (pin== 0 || pin == 9) PWM::pwmDuty9 = duty;
   if (pin== 0 || pin == 10) PWM::pwmDuty10 = duty;
   if (!PWM::pwmConfigured || !isPwmMode(PWM::pwmMode)) return;

   const uint16_t OCR = dutyToOCR(duty, PWM::pwmTop, PWM::pwmMode);

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

// Change PWM frq of an already running timer
float changePWMFrequency(float freq_hz, float lo_limit, float hi_limit){
   if (freq_hz <= 0.0f) return 0.0f;
   if ((!PWM::pwmConfigured) || (PWM::pwmMode != PWM::FAST_PWM_PH_CORRECT)) return 0.0f;
   if (lo_limit > hi_limit) return 0.0f;
   freq_hz = constrain(freq_hz, lo_limit, hi_limit);

   const uint16_t _prescaler = PWM::PWM_PRESCALERS[PWM::pwmCS-1]; // select current prescaler
   const float t = (float)F_CPU / (2.0f * _prescaler * freq_hz);
   if (t > 65535.0f || t < 40.0f) return 0.0f;                    // selected frequency doesn't fit the current prescaler

   const uint16_t top    = (uint16_t)lroundf(t);
   const float    actual = (float)F_CPU / (2.0f * _prescaler * (float)top);

   if (top == PWM::pwmTop){
      PWM::pwmFreqReq = freq_hz;
      return actual;  
   }
   // Need to calculate twice because the timer would cahnge the frequency for both pins
   const uint16_t a = dutyToOCR(PWM::pwmDuty9,  top, PWM::pwmMode);   
   const uint16_t b = dutyToOCR(PWM::pwmDuty10, top, PWM::pwmMode);

   ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {        
      if (top > PWM::pwmTop){            
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

   PWM::pwmTop     = top;
   PWM::pwmFreqReq = freq_hz;
   return actual;
}

static float setTimer(PWM::PWM_MODE mode, float freq) {
   if (freq <= 0.0f)  return 0.0f;
   uint8_t idx  = 0;
   uint16_t top = 0;
   float actual;

   if (mode == PWM::NORMAL) {
      // Free-running 16-bit counter: only the prescaler sets the rate (overflow frequency).
      float bestErr = 1e30f;              // get some really smol value
      for (uint8_t i = 0; i < 5; i++) {   // find the prescaler with the best error margin 
         float f = (float)F_CPU / (65536.0f * PWM::PWM_PRESCALERS[i]);
         float err = fabsf(f - freq);
         if (err < bestErr) {
            bestErr = err;
            idx = i;
         }
      }
      actual = (float)F_CPU / (65536.0f * PWM::PWM_PRESCALERS[idx]);
   } else {  
      // f = F_CPU / (k * N * (TOP + off))
      const float k      = (mode == PWM::FAST_PWM) ? 1.0f : 2.0f;
      const float offset = (mode == PWM::PH_CORRECT || mode == PWM::FAST_PWM_PH_CORRECT) ? 0.0f : 1.0f;
      float t = 0.0f;
      for (; idx < 5; idx++) {               // find the highest prescaler that yields a valid value
         t = (float)F_CPU / (k * PWM::PWM_PRESCALERS[idx] * freq) - offset;
         if (t <= 65535.0f) break;
      }
      if (idx == 5 || t < 2.0f) return 0.0f;  // too slow / too fast for this mode
      top    = (uint16_t)lroundf(t);
      actual = (float)F_CPU / (k * PWM::PWM_PRESCALERS[idx] * ((float)top + offset));
   }
   const uint8_t cs = idx + 1;

   // Nothing changed -> don't restart the timer
   if (PWM::pwmConfigured && mode == PWM::pwmMode && top == PWM::pwmTop && cs == PWM::pwmCS){
      PWM::pwmFreqReq = freq;
      return actual;
   }

   // Store the new data 
   PWM::pwmMode         = mode;
   PWM::pwmTop          = top;
   PWM::pwmCS           = cs;
   PWM::pwmFreqReq      = freq;
   PWM::pwmConfigured   = true;

   pinMode(9, OUTPUT);
   pinMode(10, OUTPUT);
   
   uint8_t wa, wb;
   wgmBits(mode, wa, wb);
   const uint8_t com = comBits();
   const uint16_t a = dutyToOCR(PWM::pwmDuty9, top, mode);  // math outside the atomic block
   const uint16_t b = dutyToOCR(PWM::pwmDuty10, top, mode);

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

ISR(TIMER1_OVF_vect) {
  const uint16_t top = pwmPendingTop;
  if (top && (uint16_t)(TCNT1 + 10) < top) {     // counter must not have reached the new TOP yet
    ICR1 = top;
    pwmPendingTop = 0;
    TIMSK1 &= ~_BV(TOIE1);
  }
  // otherwise: try again at the next BOTTOM (the interrupt stays enabled)
}
// ================ /PWM Control/ ================

// ================ Helper Functions ================
// Selects / calculcates the output compare registers values based on PWM mode.
static uint16_t dutyToOCR(float duty, uint16_t top, PWM::PWM_MODE m) {
  switch (m) {
    case PWM::PH_CORRECT:
    case PWM::FAST_PWM_PH_CORRECT:
      return (uint16_t)lroundf(duty * top);  // 0..TOP
    case PWM::FAST_PWM:
      {
        uint32_t v = (uint32_t)lroundf(duty * ((float)top + 1.0f));  // period is TOP+1 clocks
        return (v > top) ? top : (uint16_t)v;                        // OCR = TOP -> constant high
      }
    default:
      return 0;  // duty not applicable
  }
}

// Select the correct Waveform Generation Mode bits based on the PWM Mode
static void wgmBits(PWM::PWM_MODE m, uint8_t &a, uint8_t &b) {
  switch (m) {
    case PWM::FAST_PWM_PH_CORRECT:
      a = 0;
      b = _BV(WGM13);
      break;  // mode 8
    case PWM::PH_CORRECT:
      a = _BV(WGM11);
      b = _BV(WGM13);
      break;  // mode 10
    case PWM::CTC:
      a = 0;
      b = _BV(WGM13) | _BV(WGM12);
      break;  // mode 12
    case PWM::FAST_PWM:
      a = _BV(WGM11);
      b = _BV(WGM13) | _BV(WGM12);
      break;  // mode 14
    default:
      a = 0;
      b = 0;
      break;  // mode 0
  }
}

static uint8_t comBits() {
  const bool pwm = isPwmMode(PWM::pwmMode);
  uint8_t com = 0;
  if (PWM::pwmEnabled & 1) com |= pwm ? _BV(COM1A1) : _BV(COM1A0);
  if (PWM::pwmEnabled & 2) com |= pwm ? _BV(COM1B1) : _BV(COM1B0);
  return com;
}

// Returns the PWMEnabled status cotrol word (will add more with TMR2)
uint8_t getControlWord(uint16_t PIN_NO){
   const bool start_W0 = (PIN_NO == 0 || PIN_NO == 9);    
   const bool start_W1 = (PIN_NO == 0 || PIN_NO == 10);  
   // add more here
   uint8_t control = 0;
   if (!start_W0 && !start_W1) return 0;    // nothing to do
   if (start_W0) control |= 0b01;   
   if (start_W1) control |= 0b10; 
   return control;
}
// ================ /Helper Functions/ ================


// ================ Modbus Communication ================
void update_params(){   // update modbus data
   mb.setHreg(ADDR::_hreg_pwm_duty9,   (PWM::pwmDuty9 * 100));
   mb.setHreg(ADDR::_hreg_pwm_duty10,  (PWM::pwmDuty10 * 100));
   mb.setHreg(ADDR::_hreg_pwm_freq,     PWM::pwmFreqReq);
   mb.setHreg(ADDR::_hreg_pwm_freq_actual, PWM::pwmFreqActual);
}
// ================ /Modbus Communication/ ================
#pragma endregion
