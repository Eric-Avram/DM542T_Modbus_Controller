# Test & Results Report

## 1. Modbus RTU Communication

## 2. PWM Generation
| Project Version 	| Frequency (Target) 	| Frequency (Result) 	| Pulse Duty (Target) 	| Pulse Duty (Result) 	| Overshoot % 	| Rise Time	| Evidence File|
|-----------------	|--------------------	|--------------------	|---------------------	|---------------------	|-------------	|--------	|------------- |
|V0.1-AnalogWrite()	|      500Hz (default)|        488.6       	|         50%         	|        50.2%        	|      13-14%  	|   244ns	| [evidence_00_V0.1](Test-Results\evidence_00_V01.png)|
|V0.2-Bit-Banging 	|      500Hz (default)|      486.25        	|         50%         	|        53.4%        	|      13-14% 	| 25.575ns| [evidence_001_v0.2](Test-Results\evidence_01_V02.png)|
|                 	|                    	|                    	|                     	|                     	|             	|        	|              |
|                 	|                    	|                    	|                     	|                     	|             	|        	|              |
|                 	|                    	|                    	|                     	|                     	|             	|        	|              |
|                 	|                    	|                    	|                     	|                     	|             	|        	|              |


## 3. Observations & Troubleshooting
<details><summary>Testing: analogWrite() vs Bit-Banging</summary>
Both methods would yield acceptable performance for simple PWM tasks (e.g. LED intensity control), and bit-banging seems to be a good alternative when you need to generate some PWM when no actual PWM pins are available. 

### Bit-Banging
Bit-banging is useful when there is the need of changing the frequency on the fly, like in this project. However, in the current very trivial implementation (ignoring shared static state of the function), this method has a few downsides:
* The used millisecond resolution will not accurately give the correct result because integer resolution is used, and the highest possible frequency in this case is 1kHz:
```C
   if (freq != lastFreq){
      lastFreq = freq;
      interval_ms = 1000/freq;
   }
```
* millis() slowly creates a drift because it throws away the overshoot

### AnalogWrite()
* The measured waveform is correct and within decent margins
* Frequency cannot be changed since this method is linked to the timer directly. If frequency change is a requirement, it will be more precise to work with the timer directly. 

So far, none of these two methods seem good enough for the purpose of this project, therefore I will continue to implement one the timers from the Atmega chip.
The Atmega chip has 3 timers (T0, T1, T2); internally T0 is used for the integrated time function and I would like to keep the functionality for now, therefore only T1 and T2 are usable. 
</details>

### Troubleshooting
**Details:** Here you can find all the relevant issues I came along while doing this project.  

<details>
  <summary>Arduino Nano reset loop when connected to Codesys</summary>

* **Problem definition:** Codesys would reset the whole com port when there is a CRC error or if the modbus device doesn't respond correctly. This leads to the Nano resetting itself, however, in this time frame Codesys would send another modbus request, before the Nano would boot up completely. This leads to Codesys resetting the port again, leading to an infinite loop of the Nano being rebooted. 
* **Solution 1 (HW):** Add a 10uF capacitor between the GND and RST pins on the Nano to block the reset via USB. The capacitor needs to be removed when uploading a new FW.
* **Solution 2 (SW):** Disable Codesys auto-reset function and reset the port manually every 5 seconds. 
* **Status:** <span style="color: green;"> Closed (Solution 1) </span>
</details>

<details><summary> High Overshoot when generating PWM </summary>

* **Problem definition:**
* **Solution (HW)**: Add a ~22 Ohm resistor on the PWM pin. This raises time from 10ns to 12ns, but brings down overshoot from ~12% to ~5% at 500Hz  
</detail>