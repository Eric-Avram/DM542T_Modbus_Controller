# Test & Results Report

## 1. Modbus RTU Communication

## 2. PWM Generation
| Project Version 	| Frequency (Target) 	| Frequency (Result) 	| Pulse Duty (Target) 	| Pulse Duty (Result) 	| Overshoot % 	| Rise Time	| Evidence File|
|-----------------	|--------------------	|--------------------	|---------------------	|---------------------	|-------------	|--------	|------------- |
|V0.1-AnalogWrite()	|      500Hz (default)|        488.6       	|         50%         	|        50.2%        	|      13-14%  	|   244ns	| [evidence_00_V0.1](Test-Results\evidence_00_V01.png)|
|V0.2-Bit-Banging 	|      500Hz (default)|      486.25        	|         50%         	|        53.4%        	|      13-14% 	| 25.575ns| [evidence_01_v0.2](Test-Results\evidence_01_V02.png)|
|V0.3-Timer1 PWM  	|      500Hz         	|       498.4        	|       50%           	|          50.0%      	|      14%    	|    64ns	|[evidence_02_V03](Test-Results\evidence_02_V03.png)|
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

<details><summary>Comparison between the 3 tested methods</summary>

The following values (and the ones found in the evidence folder) are the avg. values measured by the oscilloscope over a count of 1000 samples.

| Function Used   	| Frequency Deviation	| Pulse Duty Deviation	| Overshoot % 	| Rise Time	|
|-----------------	|--------------------	|--------------------	|-------------	|--------	|
| analogWrite()   	|      11.4Hz        	|        0.2%        	|    13-14%   	|  244ns 	|
| Bit-Banging     	|      13.75Hz       	|        3.4%        	|    13-14%   	| 25.575ns  |
| Direct PWM      	|       1.6Hz        	|        0.0%        	|    13-14%   	|   64ns  	|

For this application's use case, I will continue with Direct PWM method, since it shows the most accurate frequency control. Rise time and overshoot can be corrected by improving the hardware setup.

After adding a 22Ohms resistor on the PWM output pin (PIN9) and powering the Arduino Nano via the VIN pin at 12V, the overshoot is lowered to a steady ~4-5% with a rise of ~12.5ns, which is well within my expectations.

</details>


<details open><summary>Connecting pins to timers</summary>

**Sources**: 
* [Secrets of Arduino PWM](https://docs.arduino.cc/tutorials/generic/secrets-of-arduino-pwm/)
* [Controllerstech Tutorial on Arduino PWM](https://controllerstech.com/arduino-pwm-analogwrite-tutorial/)

### Pinout
|   Timer   |   Pins    |  Base Frequency   |   Dependencies          |   Prescaler Options   |
|-----------|-----------|-------------------|-------------------      |-----------------------|
|   Timer0  |   5 & 6   |  16MHz            |Arduino Time Functions   |          5            |                       
|   Timer1  |   9 & 10  |  16MHz            |   None                  |          5            |
|   Timer2  |   3 & 11  |  16MHz            |   None                  |          8            |

<!-- ### Registers  -->


### Formulas
**Current Output Frequency** $$ f = \frac{f_{clk}}  {(2 * prescaler * top)} $$
**TOP Value:** $$ Top = \frac{f_{clk}}{(2*prescaler*f)}$$

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