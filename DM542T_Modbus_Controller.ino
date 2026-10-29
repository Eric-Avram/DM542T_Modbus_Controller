/****************************************************
 * Project Name: DM542T Modbus Controller
 * Author: 
 * Date: 28/10/2026
 ****************************************************/
#define MbSerial Serial
#define Baudrate 115200

#pragma region Libraries 
   #include <ModbusSerial.h>
   // #include <util/atomic.h>   // I think atomic blocks may be needed for setting frequency
#pragma endregion

#pragma region Pinout
   constexpr uint8_t PUL_PIN {9U};
   constexpr uint8_t DIR_PIN {8U};
#pragma endregion

// Namespace for storing modbus addresses
namespace ADDR{
   
      //** Low -level control 
   constexpr uint8_t _hreg_set_pwm_duty9  = 0x04;
   constexpr uint8_t _hreg_set_pwm_duty10 = 0x05;
   constexpr uint8_t _hreg_set_pwm_freq   = 0x06;

   constexpr uint8_t _hreg_pwm_duty9   = 0x0A;  // R 
   constexpr uint8_t _hreg_pwm_duty10  = 0x0B;  // R 
   constexpr uint8_t _hreg_pwm_freq    = 0x0C;  // R feedback

   constexpr uint8_t _coil_pwm_configured = 0x00;
   constexpr uint8_t _coil_dir9           = 0x01;
   constexpr uint8_t _coil_pwm_start9     = 0x02;
   constexpr uint8_t _coil_dir10          = 0x03;
   constexpr uint8_t _coil_pwm_start10    = 0x04;

}

namespace PWM {
   constexpr uint16_t PWM_PRESCALERS[] = {1, 8, 64, 256, 1024};
   enum PWM_MODE{
      NORMAL,
      PH_CORRECT,
      CTC,
      FAST_PWM,
      FAST_PWM_PH_CORRECT
   };
   static PWM_MODE pwmMode    = PH_CORRECT;
   static float    pwmFreq    = 0.0f;     
   static uint16_t pwmTop     = 0;
   static float    pwmDuty9   = 0.0f;
   static float    pwmDuty10  = 0.0f;
}


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

   mb.addHreg(ADDR::_hreg_pwm_duty9,   (int)(PWM::pwmDuty9 * 100));
   mb.addHreg(ADDR::_hreg_pwm_duty10,  (int)(PWM::pwmDuty10 * 100));

   mb.addHreg(ADDR::_hreg_pwm_freq,    PWM::pwmFreq);
   mb.addHreg(ADDR::_hreg_set_pwm_duty9,   (int)(PWM::pwmDuty9 * 100));
   mb.addHreg(ADDR::_hreg_set_pwm_duty10,  (int)(PWM::pwmDuty10 * 100));
   mb.addHreg(ADDR::_hreg_set_pwm_freq,    PWM::pwmFreq);

   mb.addCoil(ADDR::_coil_dir9, false);
   mb.addCoil(ADDR::_coil_pwm_start9,  false);
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

void pwmOff(uint16_t PIN_NO ){
   (PIN_NO == 9)  ? (TCCR1A &= ~(_BV(COM1A1)), PORTB &= ~(_BV(PB1))) : 
   (PIN_NO == 10) ? (TCCR1A &= ~(_BV(COM1B1)), PORTB &= ~(_BV(PB2))) : 
   (TCCR1A &= ~(_BV(COM1A1) | _BV(COM1B1)), PORTB &= ~(_BV(PB1) | _BV(PB2)));
   // switch (PIN_NO) {
   //    case 9:
   //       TCCR1A &= ~(_BV(COM1A1));  // disconnect the pin from the Timer1 (restore normal operation)
   //       PORTB &= ~(_BV(PB1));      // force pin to low
   //       break;
   //    case 10:
   //       TCCR1A &= ~(_BV(COM1B1));
   //       PORTB &= ~(_BV(PB2));
   //       break;
   //    default:                   // no pin specified, turn everything off
   //       TCCR1A &= ~(_BV(COM1A1) | _BV(COM1B1));  // disconnect both pins from Timer1
   //       PORTB &= ~(_BV(PB1) | _BV(PB2));         // force D9 and D10 LOW
   // }
}

void pwmOn(uint16_t PIN_NO ){
   TCCR1A |= (PIN_NO == 9) ? (_BV(COM1A1)) : 
   (PIN_NO == 10) ? (_BV(COM1B1)) : 
   (_BV(COM1A1) | _BV(COM1B1));
   //    switch (PIN_NO) {
   //    case 9:
   //       TCCR1A |= (_BV(COM1A1));  // reconnect the pin from the Timer1 
   //       break;
   //    case 10:
   //       TCCR1A |= (_BV(COM1B1));
   //       break;
   //    default:                                     // no pin specified, turn everything on
   //       TCCR1A |= _BV(COM1A1) | _BV(COM1B1);     // reconnect both pins to Timer1
   // }
}


void update_params(){   // update modbus data
   mb.setHreg(ADDR::_hreg_pwm_duty9,   (PWM::pwmDuty9 * 100));
   mb.setHreg(ADDR::_hreg_pwm_duty10,  (PWM::pwmDuty10 * 100));
   mb.setHreg(ADDR::_hreg_pwm_freq,    PWM::pwmFreq);
}


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


#pragma endregion
