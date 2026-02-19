# RadioMaster Boxer setup

Steps to set up a RadioMaster Boxer radio:

1. Check one of the following options:
   1. If the radio is pre-configured and behaves as expected: congratz; no need to follow any instructions on this page :stuck_out_tongue_winking_eye:
   2. If you want to do a complete radio reset following the steps under [Flash new firmware](#flash-new-firmware), then continue with step 2
2. Connect a battery to the radio
3. Turn down the top-right screw of the left joystick until the spring is strenthened enough to keep the left joystick in the middle. Possibly adjust the height of the right screw corresponding to this joystick as well
4. [Bind the radio with the receiver:](#bind-radio-and-receiver)
5. Check if the radio is recognized as USB device on your control station:
   1. Ensure radio is turned off and disconnected from control station
   2. Connect radio via top USB port to control station
   3. Check if USB device is recognized by running `lsusb` and checking if a line similar to `Bus 003 Device 014: ID 0483:df11 STMicroelectronics STM Device in DFU Mode` is displayed. If yes, continue; if no, follow the steps under [Install STM32CubeProg](#install-stm32cubeprog) and [Configure rules for ST-Link](#configure-rules-for-st-link)
6. By default, the organization uses TX Mode 2 for radio controllers (see `TX Mode 2` on [this webpage](https://www.rc-airplane-world.com/rc-transmitter-modes.html)). Therefore, for correct functioning on the platforms, follow the next steps:
   1. Follow steps under [Check SD card](#check-sd-card)
   2. Download files _radio.yml_ and _model00.yml_ from the drive in folder _root/radios/radiomaster-boxer_
   3. Copy downloaded _radio.yml_ file, remove all _.yml_ files in _RADIO_ directory on SD card of radio, and paste copied _radio.yml_ file
   4. Copy downloaded _model00.yml_ file, remove all _.yml_ files in _MODELS_ directory on SD card of radio, and paste copied _model00.yml_ file
   5. Disconnect radio from control station
   6. Restart radio
   7. Press button `MDL`
   8. Verify that there exists one model: `BABEL`
7. The radio is now ready to be used! For further information on using the radio, please consult the user guide on the drive in folder _root/radios/radiomaster-boxer_

## Flash new firmware

To flash new firmware on the RadioMaster Boxer, follow the next steps:

1. Ensure radio is turned off and disconnected from control station
2. Install a Chromium-based browsers, for example Google Chrome. See [Install Google Chrome](#install-google-chrome)
3. Open Google Chrome
4. Go to `https://buddy.edgetx.org`
5. Select the latest firmware version, for example `EdgeTX "Jolly Mon" v2.11.3`
6. Select the correct radio model: `RadioMaster Boxer`
7. Click `Flash via USB`
8. Connect radio via top USB port to control station
9. Click `Add new device`
10. Select `STM32 BOOTLOADER`
11. Click `Next`
12. Click `Start flashing`
13. If the following error appears: `Failed to execute 'open' on 'USBDevice': Access denied.`, follow the steps under [Install STM32CubeProg](#install-stm32cubeprog) and [Configure rules for ST-Link](#configure-rules-for-st-link), go back and try again
14. Click `setup your SD card`
15. Follow the steps on the webpage
16. Click `Select SD Card`
17. Select the SD card directory in the file explorer
18. Click `Allow`
19. Click `Save changes`
20. Select the same firmware version as chosen in step 5
21. Select the same radio model as chosen in step 6
22. Under _Available sounds_, select `English`
23. Click `Apply changes`

## Install Google Chrome

1. Download the debian file from google.com/chrome
2. In the file explorer, go directory where the debian file is stored, `right-click` and select `Open with App Center`
3. Click `Install` twice
4. Fill in your password

## Install STM32CubeProg

To install the STM32CubeProgrammer software, follow the next steps:

1. Go to https://www.st.com/en/development-tools/stm32cubeprog.html
2. Click `Get Software`
3. Click `Get latest` after `STM32CubeProgrammer software for Linux`
4. Follow the steps to download the zip file
5. Extract the zip file
6. Run the _.linux_ file inside the extracted directory and follow the installation steps

## Configure rules for ST-Link

To configure the rules for links with ST devices, follow the next steps:

1. Run `sudo nano /etc/udev/rules.d/45-stlink.rules`
2. Add `ATTRS{idVendor}=="0483", ATTRS{idProduct}=="df11", MODE="0666", GROUP="plugdev"`
3. Run `sudo udevadm control --reload-rules`
4. Run `sudo udevadm trigger`

## Bind radio and receiver

1. Turn on the radio
2. On the radio, press `SYS`, go to `ExpressLRS`, hold `ENTER` for a while, select `WiFi connectivity`, and select `Enable WiFi`
3. On the control station, connect to the created Wi-Fi network called `ExpressLRS TX` and enter its password: `expresslrs`
4. Open a new browser tab with address `10.0.0.1`
5. Go to `Runtime Options` and activate the field `Binding Phrase`
6. Enter the robot name, for example `Crawler01`
7. Press `SAVE`
8. Press `REBOOT`. This reboots the radio
9. After the radio reboot press `RTN` to go back to the main menu
   > :information_source: As long as the radio is not connected to a receiver, it goes into Wi-Fi mode.
10. Power the radio receiver
11. On the control station, connect to the created Wi-Fi network called `ExpressLRS RX` and enter its password: `expresslrs`
12. Repeat steps 4-8. This reboots the receiver
13. Verify the successful connection between radio and receiver:
    1. Turn off the radio and receiver
    2. Turn on the radio and receiver
    3. Check that the receiver LED turns blue. Alternatively, wait for 10 seconds, then try to turn off the radio. The radio should give a warning that the receiver is still connected

## Check SD card

To check the file contents on the SD card, follow the next steps:

1. Unplug radio
2. Turn on radio
3. Connect radio via top USB port to control station
4. Select mode `USB Storage (SD)` on radio
5. Open corresponding volume in file explorer
