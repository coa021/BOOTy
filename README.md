TODO: Finish up

# Custom Bootloader - BOOTy

This project represents my take on creating a custom secure bootloader, alongside application that has OTA update.  
Bootloader expects of firmware image to start with 512 bytes application header that consists of magic constant, version, firmware size, crc32 of the firmware, and P-256 signature.  
In order to get signature, first the application (without header ofc) goes through SHA-256 and then that SHA256 is signed and we store that signature into header.  



## Down below is explanation of each folder and how to use what

### Application
Application project  
Prints app version and blinks led that is it.


### Bootloader
Bootloader project  
Verifies validity of the signed firmware image, for both update and main application. If error occurs during main app validation, it will try to verify the image in update sector, no matter if it is an older version. If the update firmware version is GT(>) current running version it will try to verify that image and update upon success. If both the main and update firmware application are corrupted, it will get stuck into error handler, it will just blink LED.

### Shared
Includes files shared between both projects:
- memory_map.ld - Centralized memory mapping to one file,
- flash_layout.h - Address defines,
- flash/ - FLASH operations in once place,
- custom_crc/ - CRC 32 and 16 in one place.
  
You can define your own custom memory layout inside memory_map.ld and grab the values in flash_layout.h 
### Docs
Documentation.  

### Tools
Consists of a couple of tools that helped me throughout development:  
- flash.sh - Shell script to sign the application and flash bootloader and application on specified addresses. Used with st link v2,
- send_update.py - Sends updated signed firmware version over UART to MCU,
- sign_firmware.py - Used to sign firmware with app header.  


### Tests
Currently includes only one test file that tests UART communication and parsing logic with MCU.