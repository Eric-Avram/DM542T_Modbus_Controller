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
   constexpr uint8_t PUL_PIN_1 {9U};
   // constexpr uint8_t PUL_PIN_2 {10U};

   constexpr uint8_t DIR_PIN_1 {8U};
   // constexpr uint8_t DIR_PIN_2 {11U};

#pragma endregion

// Namespace for storing modbus addresses
namespace ADDR {

   //** Low -level control 0x00 - 0x09
   // Prer for Timer 2
   // constexpr uint8_t _hreg_set_pwm_duty3 = 0x04;
   // constexpr uint8_t _hreg_set_pwm_duty11 = 0x04;

   // PWM Frequency Generation Settings
   constexpr uint8_t _hreg_set_pwm_duty9  = 0x00;
   constexpr uint8_t _hreg_set_pwm_duty10 = 0x01;
   constexpr uint8_t _hreg_pwm_mode       = 0x02;
   constexpr uint8_t _hreg_pwm_lo_limit   = 0x03;
   constexpr uint8_t _hreg_pwm_hi_limit   = 0x04;
   constexpr uint8_t _hreg_set_pwm_freq   = 0x05; 

   //** PWM Control
   constexpr uint8_t _coil_dir9           = 0x00;
   constexpr uint8_t _coil_dir10          = 0x01;
   constexpr uint8_t _coil_pwm_start9     = 0x02;
   constexpr uint8_t _coil_pwm_start10    = 0x03;
   constexpr uint8_t _coil_pwm_configured = 0x04;

   //** PWM Calculated Values
   constexpr uint8_t _ireg_pwm_enabled     = 0x00;
   constexpr uint8_t _ireg_pwm_top         = 0x01;
   constexpr uint8_t _ireg_pwm_cs          = 0x02;
   constexpr uint8_t _ireg_pwm_duty9       = 0x03;
   constexpr uint8_t _ireg_pwm_duty10      = 0x04;
   constexpr uint8_t _ireg_pwm_freq        = 0x05;
   constexpr uint8_t _ireg_pwm_freq_actual = 0x06;
}

#pragma region Objects & Handlers
   ModbusSerial mb (MbSerial, 1);
   AvrTimerPWM PWM_Timer(2500.0f, 0.5f);
#pragma endregion

#pragma region Function Prototypes

void write_modbus();
#pragma endregion

float pwmFreqReq = 0.0f; 
void setup() {
   MbSerial.begin (Baudrate, MB_PARITY_EVEN); // prefer this line in accordance with the modbus standard.
   while (! MbSerial);
   mb.config (Baudrate);
   
   pinMode(DIR_PIN_1, OUTPUT);
   pinMode(PUL_PIN_1, OUTPUT);

   // pinMode(DIR_PIN_2, OUTPUT);
   // pinMode(PUL_PIN_2, OUTPUT);

   delay(250);
   
   PWM_Timer.setPWMMode(PWM_MODE::FAST_PWM_PH_CORRECT);
   PWM_Timer.setPWMFrequency(5000.0f);
   PWM_Timer.setPWMDuty(0.5f, 9);
   PWM_Timer.pwmOff();   // reset, clean state

   delay(250);
   mb.setAdditionalServerData ("DM542T CONTROLLER"); //Report Server ID function (0x11) 

   mb.addIreg(ADDR::_ireg_pwm_enabled,       PWM_Timer.getPwmStatus());             // 0x00
   mb.addIreg(ADDR::_ireg_pwm_top,           PWM_Timer.getPwmTop());                // 0x01
   mb.addIreg(ADDR::_ireg_pwm_cs,            PWM_Timer.getPwmCS());                 // 0x02
   mb.addIreg(ADDR::_ireg_pwm_duty9,  (uint16_t) (lroundf(PWM_Timer.getPwmDuty9() * 100)));  // 0x03
   mb.addIreg(ADDR::_ireg_pwm_duty10, (uint16_t) (lroundf(PWM_Timer.getPwmDuty10() * 100))); // 0x04
   mb.addIreg(ADDR::_ireg_pwm_freq,          PWM_Timer.getPwmFrequencyRequested()); // 0x05
   mb.addIreg(ADDR::_ireg_pwm_freq_actual,   PWM_Timer.getPwmFrequencyActual());    // 0x06                   

   mb.addHreg(ADDR::_hreg_set_pwm_duty9,  (uint16_t)  (lroundf(PWM_Timer.getPwmDuty9() * 100)));   // 0x00
   mb.addHreg(ADDR::_hreg_set_pwm_duty10, (uint16_t)  (lroundf(PWM_Timer.getPwmDuty10() * 100)));  // 0x01
   mb.addHreg(ADDR::_hreg_pwm_mode,       static_cast<uint16_t>(PWM_Timer.getPwmMode())); // 0x02
   mb.addHreg(ADDR::_hreg_pwm_lo_limit,      frq_default_lo_limit);                       // 0x03
   mb.addHreg(ADDR::_hreg_pwm_hi_limit,      frq_default_hi_limit);                       // 0x04
   pwmFreqReq = PWM_Timer.getPwmFrequencyRequested();
   mb.addHreg(ADDR::_hreg_set_pwm_freq,      pwmFreqReq);                                 // 0x05

   mb.addCoil(ADDR::_coil_dir9,        false);
   mb.addCoil(ADDR::_coil_dir10,       false);
   mb.addCoil(ADDR::_coil_pwm_start9,  false);
   mb.addCoil(ADDR::_coil_pwm_start10, false);
   mb.addCoil(ADDR::_coil_pwm_configured, PWM_Timer.isPwmConfigured());

   delay(250);
   write_modbus();  // update modbus data
}

#define pwmPin(condition, pin) (condition)? PWM_Timer.pwmOn(pin) : PWM_Timer.pwmOff(pin)

void loop() {
   mb.task();
   digitalWrite(DIR_PIN_1, mb.coil(ADDR::_coil_dir9));
   // digitalWrite(DIR_PIN_2, mb.coil(ADDR::_coil_dir10));

   pwmPin(mb.coil(ADDR::_coil_pwm_start9), 9);
   pwmPin(mb.coil(ADDR::_coil_pwm_start10), 10);
   
   const uint16_t set_duty9 = mb.Hreg(ADDR::_hreg_set_pwm_duty9);
   if (set_duty9 != mb.Ireg(ADDR::_ireg_pwm_duty9)){
      PWM_Timer.setPWMDuty((set_duty9)/100.0f, 9);
      write_modbus();
   }

   const uint16_t freq_hz =  mb.Hreg(ADDR::_hreg_set_pwm_freq);
   if (freq_hz != mb.Ireg(ADDR::_ireg_pwm_freq)){
      const uint16_t lo_limit = mb.hreg(ADDR::_hreg_pwm_lo_limit);   
      const uint16_t hi_limit = mb.hreg(ADDR::_hreg_pwm_hi_limit);
      PWM_Timer.rampPWMFrequency(freq_hz, 2000.0f, lo_limit, hi_limit);
      
      write_modbus();
   }
}


// ================ Modbus Communication ================
void write_modbus(){   // update modbus data
   mb.setIreg(ADDR::_ireg_pwm_enabled,       PWM_Timer.getPwmStatus());             // 0x00
   mb.setIreg(ADDR::_ireg_pwm_top,           PWM_Timer.getPwmTop());                // 0x01
   mb.setIreg(ADDR::_ireg_pwm_cs,            PWM_Timer.getPwmCS());                 // 0x02
   mb.setIreg(ADDR::_ireg_pwm_duty9,  (uint16_t) (lroundf(PWM_Timer.getPwmDuty9() * 100)));  // 0x03
   mb.setIreg(ADDR::_ireg_pwm_duty10, (uint16_t) (lroundf(PWM_Timer.getPwmDuty10() * 100))); // 0x04
   mb.setIreg(ADDR::_ireg_pwm_freq,          PWM_Timer.getPwmFrequencyRequested()); // 0x05
   mb.setIreg(ADDR::_ireg_pwm_freq_actual,   PWM_Timer.getPwmFrequencyActual());    // 0x06

   mb.setCoil(ADDR::_coil_pwm_configured, PWM_Timer.isPwmConfigured());
}

// ================ /Modbus Communication/ ================
#pragma endregion
