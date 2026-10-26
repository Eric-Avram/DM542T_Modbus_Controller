/****************************************************
 * Project Name: 
 * Author: 
 * Date: 2025-10-04
 * Description: 
 Uses
 ****************************************************/
// Set verbosity level (0 = nothing, 1 = errors only, 2 = errors + warnings, 3 = full debug)
#define VERBOSE_LEVEL 3
#define PROJECT_NAME "Joint Controller"
#define AUTHOR "ERIC ANDREI AVRAM"
#define VERSION_MAJOR 0
#define VERSION_MINOR 1
#define VERSION_PATCH 0

#define MbSerial Serial
#define Baudrate 115200

#pragma region Libraries 
   #include "Versioning.h"
   #include <ModbusSerial.h>
#pragma endregion

#pragma region Constants
   constexpr uint8_t PUL_PIN {9U};
   constexpr uint8_t DIR_PIN {8U};

   const int addr_movement_done  = 0;
   const int addr_movement_dir   = 1;
   const int addr_move_infinite  = 2;
   // const int addr_command_home   = 3;
   const int sanity_check   = 3;
   const int addr_command_pulses = 4;
#pragma endregion

#pragma region Globals
//  Add global variables here
unsigned long previousMillis = 0;
unsigned long interval = 100;
constexpr uint8_t dutyCycle = 128;
#pragma endregion


#pragma region Objects & Handlers
   ModbusSerial mb (MbSerial, 1);
#pragma endregion


#pragma region Function Prototypes

#pragma endregion
bool debugger = false;

void setup() {
   // #if VERBOSE_LEVEL > 0
   //    Serial.begin(115200);
   //    LOG_PRINT("Starting Serial Communication.");
   //    while (!Serial) {
   //       LOG_PRINT(".");
   //       yield();
   //    }
   // #endif

   MbSerial.begin (Baudrate, MB_PARITY_EVEN); // prefer this line in accordance with the modbus standard.
   while (! MbSerial){
      LOG_PRINT(".");
   }

   mb.config (Baudrate);
   
   pinMode(PUL_PIN, OUTPUT);
   pinMode(DIR_PIN, OUTPUT);
   delay(250);
   // registers hold a value of max 65535
   // to write higher values, write the numbers in binary, then split them, in 2 16-bit registers. 
   mb.addCoil(addr_movement_done, false);  // TRUE when movement reached position
   mb.addCoil(addr_movement_dir, true);   // TRUE for right 
   mb.addCoil(addr_move_infinite, false);  // TRUE to keep turning 
   mb.addCoil(sanity_check, false);  // TRUE to keep turning 
   
   LOG_PRINT("\n >>> Boot up completed. V.");
   LOG_PRINTLN(PROJECT_NAME);
   LOG_PRINTLN(PROJECT_VERSION);
   LOG_PRINTLN(AUTHOR);

   delay(1000);
}

void loop() {
   mb.task();
   digitalWrite(DIR_PIN, mb.coil(addr_movement_dir));
   // if (mb.coil(addr_move_infinite) && cycle.wake){
   // if (cycle.switchMode()){
   //    if (cycle.wake){
   //       digitalWrite(PUL_PIN, HIGH);
   //       mb.setCoil(addr_movement_done, true);

   //    }
   //    else {
   //       digitalWrite(PUL_PIN, LOW);
   //       mb.setCoil(addr_movement_done, false);
   //    }
   // }
   // 1kHz = every 1 millisecond
   if (mb.coil(addr_move_infinite)){
      // pwm(PUL_PIN, 1000);
      // Test at whatever frequency this pulls out and 50% duty cycle. 
      analogWrite(PUL_PIN, 128);
   }
   else {
      analogWrite(PUL_PIN, 0);
   }

   mb.setCoil(sanity_check, mb.coil(addr_move_infinite));
}


#pragma region Functions
void pwm(const uint8_t &PIN_NO, unsigned long interval){
   unsigned long currentMillis = millis();
   if (currentMillis - previousMillis >= interval){
      // PIN_STATE = !PIN_STATE;
      previousMillis = currentMillis;
      digitalWrite(PIN_NO, HIGH);
      debugger = true;
   }

   // digitalWrite(PIN_NO, ((PIN_STATE)?HIGH:LOW));
   Serial.write(debugger);
}
#pragma endregion

