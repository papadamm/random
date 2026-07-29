To use r7s72100 GR-PEACH with OpenOCD some soldering is needed:
- Add a mini 10-pin JTAG header
- Optionally, consider adding a zero ohm resistor for reset handling

In case the SoC does not reset properly, consider a software workaround.
For instance, U-boot configured to execute from on-chip RAM may be used.

To use U-Boot on GR-Peach two connections are required:
- JTAG with OpenOCD to control the ARM CPU Core.
- For power and console access the micro-USB port in the corner is used.

Upstream GR-PEACH is using P6_2 and P6_3 for console by default.

This is how to make use of minicom on Mac OS X:
% minicom -b 115200 -D /dev/tty.usbmodem141202

In a separate terminal OpenOCD (version 0.12.0) is started like
this in case a SEGGER J-Link debugger is used:
% openocd -f interface/jlink.cfg -c "transport select jtag" \
  -f target/renesas_r7s72100.cfg -c "adapter speed 50000"

Then in a third terminal the following OpenOCD commands may be used
to load U-Boot to on-chip memory and start it from there:
% telnet localhost 4444
> reset_config srst_only
> reset halt
> load_image u-boot-gr-peach-onchip-ram-boot-v2025.04-rc2-20250214.bin 0x20100000
> arm core_state arm
> resume 0x20100000

Now serial console output from U-Boot should end up on the minicom terminal.

In theory Ethernet networking and flash programming is possible, however
the exact level of hardware support in upstream U-Boot remains to be seen.

To build a similar U-Boot binary, simply use the grpeach_defconfig
CONFIG_TEXT_BASE has been changed from 0x18000000 to 0x20100000.
