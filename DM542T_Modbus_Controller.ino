/****************************************************
 * Project Name: DM542T Modbus Controller
 * Author: 
 * Date: 28/10/2026
 ****************************************************/
#define MbSerial Serial
#define Baudrate 115200

#pragma region Libraries 
   #include <ModbusSerial.h> 
   #include "src/AvrTimerPWM.h"
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

#pragma region Objects & Handlers
   ModbusSerial mb (MbSerial, 1);
   AvrTimerPWM PWM_Timer(2500.0f, 0.5f);
#pragma endregion

#pragma region Function Prototypes

void update_params();
bool rampPWMFrequency(float targetHz, float lo_limit = frq_default_lo_limit, float hi_limit = frq_default_hi_limit, float accelHzPerS = 500.0f);
#pragma endregion

float pwmFreqReq = 0.0f; 
void setup() {
   MbSerial.begin (Baudrate, MB_PARITY_EVEN); // prefer this line in accordance with the modbus standard.
   while (! MbSerial);
   mb.config (Baudrate);
   
   pinMode(DIR_PIN, OUTPUT);
   pinMode(PUL_PIN, OUTPUT);

   delay(250);
   
   PWM_Timer.setPWMMode(PWM_MODE::FAST_PWM_PH_CORRECT);
   PWM_Timer.setPWMFrequency(5000.0f);
   PWM_Timer.setPWMDuty(0.5f, 9);
   PWM_Timer.pwmOff();   // reset, clean state

   delay(250);
   // registers hold a value of max 65535
   // to write higher values, split them, in 2 16-bit registers.
   mb.setAdditionalServerData ("DM542T CONTROLLER"); //Report Server ID function (0x11) 


   mb.addHreg(ADDR::_hreg_pwm_cs,            PWM_Timer.getPwmCS());
   mb.addHreg(ADDR::_hreg_pwm_duty9,  (uint16_t)  (PWM_Timer.getPwmDuty9() * 100));
   mb.addHreg(ADDR::_hreg_pwm_duty10, (uint16_t)  (PWM_Timer.getPwmDuty10() * 100));
   mb.addHreg(ADDR::_hreg_pwm_enabled,       PWM_Timer.getPwmStatus());
   mb.addHreg(ADDR::_hreg_pwm_freq,          PWM_Timer.getPwmFrequencyRequested());
   // mb.addHreg(ADDR::_hreg_pwm_mode,          PWM_Timer.getPwmMode());
   mb.addHreg(ADDR::_hreg_pwm_top,           PWM_Timer.getPwmTop());
   mb.addHreg(ADDR::_hreg_pwm_lo_limit,      frq_default_lo_limit);
   mb.addHreg(ADDR::_hreg_pwm_hi_limit,      frq_default_hi_limit);

   mb.addHreg(ADDR::_hreg_set_pwm_duty9,  (uint16_t)  (PWM_Timer.getPwmDuty9() * 100));
   mb.addHreg(ADDR::_hreg_set_pwm_duty10, (uint16_t)  (PWM_Timer.getPwmDuty10() * 100));
   mb.addHreg(ADDR::_hreg_set_pwm_freq,      pwmFreqReq);
   mb.addHreg(ADDR::_hreg_pwm_freq_actual,   PWM_Timer.getPwmFrequencyActual());

   mb.addCoil(ADDR::_coil_pwm_configured, PWM_Timer.isPwmConfigured());
   mb.addCoil(ADDR::_coil_dir9, false);
   mb.addCoil(ADDR::_coil_pwm_start9, false);
   mb.addCoil(ADDR::_coil_dir10, false);
   mb.addCoil(ADDR::_coil_pwm_start10, false);

   delay(250);
   update_params();  // update modbus data
}

#define pwmPin(condition, pin) (condition)? PWM_Timer.pwmOn(pin) : PWM_Timer.pwmOff(pin);

void loop() {
   mb.task();
   digitalWrite(DIR_PIN, mb.coil(ADDR::_coil_dir9));

   pwmPin(mb.coil(ADDR::_coil_pwm_start9), 9);
   pwmPin(mb.coil(ADDR::_coil_pwm_start10), 10);

   if (mb.Hreg(ADDR::_hreg_set_pwm_duty9) != mb.Hreg(ADDR::_hreg_pwm_duty9)){
      PWM_Timer.setPWMDuty((mb.Hreg(ADDR::_hreg_set_pwm_duty9)/100.0f), 9);
      update_params();
   }
   if (mb.Hreg(ADDR::_hreg_set_pwm_freq) != mb.Hreg(ADDR::_hreg_pwm_freq)){

      const uint16_t freq_hz =  mb.Hreg(ADDR::_hreg_set_pwm_freq);
      const uint16_t lo_limit = mb.hreg(ADDR::_hreg_pwm_lo_limit);   
      const uint16_t hi_limit = mb.hreg(ADDR::_hreg_pwm_hi_limit);
      PWM_Timer.rampPWMFrequency(freq_hz, 2000.0f, lo_limit, hi_limit);
      update_params();
   }
}


// ================ Modbus Communication ================
void update_params(){   // update modbus data
   mb.setHreg(ADDR::_hreg_pwm_duty9,       (uint16_t)(PWM_Timer.getPwmDuty9() * 100));
   mb.setHreg(ADDR::_hreg_pwm_duty10,      (uint16_t)(PWM_Timer.getPwmDuty10() * 100));
   mb.setHreg(ADDR::_hreg_pwm_freq,        PWM_Timer.getPwmFrequencyRequested());
   mb.setHreg(ADDR::_hreg_pwm_freq_actual, PWM_Timer.getPwmFrequencyActual());
}
// ================ /Modbus Communication/ ================
#pragma endregion
