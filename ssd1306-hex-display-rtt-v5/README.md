# ssd1306-hex-display-rtt-v5 (HL GPIO sequencer test code)

Based on parts of HL 9d9bba4, this directory contains some GPIO sequencer test code, pretty much enough to drive the SCL and SDA signals connected to the ssd1306 display controller. Also the RTT is included to provide means for debug output over the JTAG in case further debugging would be needed. For instructions on how to program the target please see teeensy-pixhawk-6c-stm32h743.sh also the actual source code contains definitions on how to use RTT and how the SCL and SDA signals should be connected. The power for the LCD module comes from the GPS1 port of the pixhawk 6c.

