# DM542T Modbus Controller

## Introduction
A while back, I've partially built the BCN3D Moveo as a personal side project. (see https://github.com/BCN3D/BCN3D-Moveo). Unfortunately, at the time I didn't have any Arduino Mega to test it out, and mostly wanted to integrate it with ROS2, but never got to do it. 

Lately I've been having some extra time so I decided to go back to it, and realized  that I could use this robot as a test bench for Codesys Projects involving robotics, plus it sounds like a fun side project. 

I have some StepperOnline DM542T drivers laying around and some Arduino Nano clones, as well as an old RPI3B+, hence how I got to try to develop this controller. 

## Documentation:
* [Test Results & Dev docs](Tests & Results.md)

## Architecture
*TBD*

## Tools:
*  [ModbusTool](https://github.com/ClassicDIY/ModbusTool.git) for testing serial communication 

---

## Changelog
### V0.1 Default Arduino analogWrite
* Added modbus support using the Arduino provided [Modbus-Serial Lib](https://github.com/epsilonrt/modbus-serial)
* Tested modbus function using [Modbus Tool](https://github.com/ClassicDIY/ModbusTool.git)
* Added prototype for own pwm function
* Added simple analogWrite on modbus trigger. 

## Target Requirements:
* Stable Modbus RTU communication at 8E1, 115200 Baud via Serial Com (USB)
* Stable PWM generation within +-5% frequency target
* Stable PWM duty cycle within 5% duty target 
* Codesys compatibility as a Modbus Slave
* Per DM542T datasheet, variable PWM frequency in the range of 0 to 200kHz with a stable duty cycle of 50% (per recommendation), and a minimal pulse width of 2.5us 
* Error Handling & Reporting
* Ramping PWM capabilities (later) to avoid any current spikes coming from the motor
* Modbus Registers for:
    * Start/stop PWM
    * Duty setpoint
    * Direction Change
    * Frequency setpoint
    * Frequency feedback 
    * Frequency generation lower/upper boundries
    * (TBD) Connected Drive capabilities
    * (TBD) Connected Motor capabilities
