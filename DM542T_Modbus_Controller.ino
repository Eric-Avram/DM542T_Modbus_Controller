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
constexpr uint8_t _hreg_pwm_lo_limit = 0x11;
constexpr uint8_t _hreg_pwm_hi_limit = 0x12;

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
   enum PWM_MODE {
      NORMAL,
      PH_CORRECT,
      CTC,
      FAST_PWM,
      FAST_PWM_PH_CORRECT
   };
   static PWM_MODE   pwmMode = PH_CORRECT;
   static float      pwmFreqReq = 0.0f;  // last successfully applied frequency request
   static float      pwmDuty9 = 0.0f;
   static float      pwmDuty10 = 0.0f;
   static uint16_t   pwmTop = 0;
   static uint8_t    pwmCS = 0;
   static uint8_t    pwmEnabled = 0;  // bit0 = pin 9 connected, bit1 = pin 10 connected
   static bool       pwmConfigured = false;
}
float frq_lo_limit = 122;
float frq_hi_limit = 20000;

#pragma region Objects & Handlers
   ModbusSerial mb (MbSerial, 1);
#pragma endregion

#pragma region Function Prototypes
// float setPWMMode(PWM::PWM_MODE m);
// float setPWMFrequency(float freq_hz);
// void setPWMDuty(float duty, uint8_t pin = 0);
void pwmOff(uint16_t PIN_NO = 0 );
void pwmOn (uint16_t PIN_NO = 0);

#pragma endregion

void setup() {
   MbSerial.begin (Baudrate, MB_PARITY_EVEN); // prefer this line in accordance with the modbus standard.
   while (! MbSerial);
   mb.config (Baudrate);
   
   pinMode(DIR_PIN, OUTPUT);
   pinMode(PUL_PIN, OUTPUT);

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
   mb.addHreg(ADDR::_hreg_pwm_lo_limit, frq_lo_limit);
   mb.addHreg(ADDR::_hreg_pwm_hi_limit, frq_hi_limit);


   mb.addHreg(ADDR::_hreg_set_pwm_duty9, (int)(PWM::pwmDuty9 * 100));
   mb.addHreg(ADDR::_hreg_set_pwm_duty10, (int)(PWM::pwmDuty10 * 100));
   mb.addHreg(ADDR::_hreg_set_pwm_freq, PWM::pwmFreqReq);

   mb.addCoil(ADDR::_coil_pwm_configured, PWM::pwmConfigured);
   mb.addCoil(ADDR::_coil_dir9, false);
   mb.addCoil(ADDR::_coil_pwm_start9, false);
   mb.addCoil(ADDR::_coil_dir10, false);
   mb.addCoil(ADDR::_coil_pwm_start10, false);
 
   TCCR1A = 0;            // Clean timer
   TCCR1B = 0;
   TCNT1  = 0;
   // The timer is in ph correct mode, so the counter goes up then down
   /* To calculate the output frequency, divide the clock hz by the prescaler to get the time it gets for one ramp:
   f_clk (Clock) = 16MHz = 16 000 000Hz
   prescaler = 1 (for now)
   f (target) = 500 Hz
   TOP = timer counts up to this value saved in ICR instead of 0 to 255, so:
   f = f_clk / prescaler / top / 2 OR f = f_clk / (2 * prescaler * top)
   500 Hz = 16 000 000 / (2 * 1 * top) => top = 16 000 000 / (2 * 500) = 16000
   OCRA is the value to count up to, before the counter goes down, so for a 50% duty cycle, the OCRA is duty * TOP
   */
   ICR1  = 16000;           // TOP -> 40 kHz
   OCR1A = 8000;           // 50% duty on D9 (OCR1 is duty*ICR1)
   OCR1B = 100;           // 50% duty on D10

   // Non-inverting output on both channels, WGM11 = 1 (mode 10, low bits)
   TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM11);

   // WGM13 = 1 (mode 10, high bits), CS10 = 1 -> prescaler /1, timer starts
   TCCR1B = _BV(WGM13) | _BV(CS10);
 
   delay(1000);
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
      setPWMFrequency(mb.Hreg(ADDR::_hreg_set_pwm_freq));
      update_params();
   }
}


#pragma region Functions
// ================ Basic Control ================
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

void pwmOff(uint16_t PIN_NO ){   
   PWM::pwmEnabled &= ~getControlWord(PIN_NO);
   updatePins();
}

void pwmOn(uint16_t PIN_NO ){
   PWM::pwmEnabled |= getControlWord(PIN_NO);
   updatePins();
}

static void updatePins() {
  if (!(PWM::pwmEnabled & 1)) PORTB &= ~_BV(PB1);
  if (!(PWM::pwmEnabled & 2)) PORTB &= ~_BV(PB2);

  TCCR1A = (TCCR1A & 0x0F) | comBits();
}

static uint8_t comBits() {
  const bool pwm = isPwmMode(PWM::pwmMode);
  uint8_t com = 0;
  if (PWM::pwmEnabled & 1) com |= pwm ? _BV(COM1A1) : _BV(COM1A0);
  if (PWM::pwmEnabled & 2) com |= pwm ? _BV(COM1B1) : _BV(COM1B0);
  return com;
}

static bool isPwmMode(PWM::PWM_MODE mode) {
  return mode == PWM::PH_CORRECT || 
         mode == PWM::FAST_PWM   || 
         mode == PWM::FAST_PWM_PH_CORRECT;
}
// ================ /Basic Control/ ================


// ================  PWM Control  ================
/* IN PROGRESS*/
// Set frequency in Hz (shared by pins 9 and 10). Returns the actual frequency, or 0. 
float setPWMFrequency(float freqHz) {
   // TODO: Add constrain if defined upper/lower bounds?
   if (freqHz <= 0.0f) return 0.0f;
   return 1.0f;
}

// Duty 0.0 - 1.0 for pin 9, 10, or both (0). Only has an effect in the PWM modes. 
void setPWMDuty(float duty, uint8_t pin) {
   duty = constrain(duty, 0.0f, 1.0f);

   if (pin == 0 || pin == 9)  PWM::pwmDuty9  = duty;
   if (pin == 0 || pin == 10) PWM::pwmDuty10 = duty;

   //TODO: Find a way to convert duty to OCR since OCR controls duty.
}

static float setTimer(PWM::PWM_MODE m, float freq) {
   if (!(freq > 0.0f)) return 0.0f;

   uint8_t idx = 0;
   uint16_t top = 0;
   float actual;

   if (m == PWM::NORMAL) {
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
      const float k      = (m == PWM::FAST_PWM) ? 1.0f : 2.0f;
      const float offset = (m == PWM::PH_CORRECT || m == PWM::FAST_PWM_PH_CORRECT) ? 0.0f : 1.0f;
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
   if (PWM::pwmConfigured && m == PWM::pwmMode && top == PWM::pwmTop && cs == PWM::pwmCS)
      return actual;

   // Store the new data 
   PWM::pwmMode         = m;
   PWM::pwmTop          = top;
   PWM::pwmCS           = cs;
   PWM::pwmFreqReq      = freq;
   PWM::pwmConfigured   = true;

   pinMode(9, OUTPUT);
   pinMode(10, OUTPUT);
   
   uint8_t wa, wb;
   wgmBits(m, wa, wb);
   const uint8_t com = comBits();
   const uint16_t a = dutyToOCR(PWM::pwmDuty9, top, m);  // math outside the atomic block
   const uint16_t b = dutyToOCR(PWM::pwmDuty10, top, m);

   ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      TIMSK1 &= ~_BV(TOIE1);  
      // pwmPendingTop = 0;   // cancel any pending on-the-fly change
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
// ================ /PWM Control/ ================

// ================ Modbus Communication ================
void update_params(){   // update modbus data
   mb.setHreg(ADDR::_hreg_pwm_duty9,   (PWM::pwmDuty9 * 100));
   mb.setHreg(ADDR::_hreg_pwm_duty10,  (PWM::pwmDuty10 * 100));
   // mb.setHreg(ADDR::_hreg_pwm_freq,    PWM::pwmFreq);
}
// ================ /Modbus Communication/ ================
#pragma endregion
