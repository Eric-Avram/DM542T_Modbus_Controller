# Test & Results Report

## 1. Modbus RTU Communication

## 2. PWM Generation
| Project Version 	| Frequency (Target) 	| Frequency (Result) 	| Pulse Duty (Target) 	| Pulse Duty (Result) 	| Overshoot % 	| Rise Time	| Evidence File|
|-----------------	|--------------------	|--------------------	|---------------------	|---------------------	|-------------	|--------	|------------- |
|       V0.1      	|      500Hz (default)  |        488.6       	|         50%         	|        50.2%        	|      13-14%  	|   244ns	| [evidence_00_V0.1](Test-Results\V0.1-Default-analogWrite\evidence_00_V01.png)|
|                 	|                    	|                    	|                     	|                     	|             	|        	|              |
|                 	|                    	|                    	|                     	|                     	|             	|        	|              |
|                 	|                    	|                    	|                     	|                     	|             	|        	|              |
|                 	|                    	|                    	|                     	|                     	|             	|        	|              |
|                 	|                    	|                    	|                     	|                     	|             	|        	|              |


## 3. Observations & Troubleshooting
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