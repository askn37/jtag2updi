# JTAG2UPDI (Clone)

[READEME of the original (en_US)](READEME_orig.md) / [English version of this document (en_US)](READEME.md) / [Japanese version of this document (ja_JP)](READEME_jp.md)

This is a static fork of the [ElTangas/jtag2updi](https://github.com/ElTangas/jtag2updi) firmware. Although most of the code is the same, there is no upstream linkage as it involves large-scale changes.

Branching point: https://github.com/ElTangas/jtag2updi/tree/07be876105e0b9cfedf2723b0ac88780bcae50d8

The difference between the two should be obtained by `diff -uBw jtag2updi-master/source jtag2updi-main/src`. `JTAG2.cpp` is almost completely different.

## What has changed

- Compatible with NVMCTRL version 0,2,3,5. The correspondence in the original text is up to 0 and 2.
   - Support tinyAVR-0/1/2, megaAVR-0, AVR_DA/DB/DD/EA/EB.
     - Compatible with UPDI layer SIB (System Information Block).
   - Main devices confirmed to work: ATtiny202 ATtiny412 ATtiny824 ATtiny1614 ATmega4809 AVR32DA32 AVR128DB32 AVR32DD14 AVR64DD32 AVR64EA32 (as of November 2023)
- Compatible with AVRDUDE 7.3.
   - Operation confirmed: `AVRDUDE` `7.2`, `7.3` (under development as of November 2023)
   - For `6.8` and earlier, use with a specially modified configuration file. Starting from `7.0`, the standard configuration file is sufficient.
   - Only partial memory blocks can be rewritten using the `-D` option. (as the bootloader does)
     - This allows you to also use the `write/elase <memtyp>` command in interactive mode.
   - Compatible with locked devices.
     - Blind writing of USERROW.
     - LOCK_BITS Lock/Unlock.
   - Enhanced memory range specification violation detection.
     - Allow 256 word FLASH bulk reading/writing of AVR_DA/DB/DD.
     - Allows 1 word EEPROM reading/writing of AVR_DA/DB/DD. (Standard configuration file is limited to 1 byte unit)
     - Allows 4 word EEPROM reading/writing of AVR_EA/EB.
   - `230400`, `460800`, and `500000` can be practically specified in many environments with the `-b` option.
     - This does not apply to Arduino compatible machines. Please note that when reading and writing large-capacity bulk, you are subject to host-side timeout/error restrictions. (Generally, the faster the communication speed, the higher the USB packet loss rate)
   - Some enhancements.
     - Does not support high voltage control. This requires additional hardware and specialized control code. He has two different specifications for his HV control for current UPDI-enabled devices. There is no easy way to support both at the same time, which makes the problem difficult. (See UPDI4AVR technical information)
- Firmware build and installation has been verified only for Arduino IDE 1.8.x/2.x. Not sure about other methods.
   - The `source` directory has been changed to `src` to match the Arduino IDE specifications. The location of the `jtag2updi.ino` file is also different.
   - Attached files that are not supported have been removed. Please copy from the original if necessary.

Equipment that can be installed with firmware:

- ATmegaX8 series --- Arduino UNO Rev.3, Arduino Mini, Arduino Micro (ATmega328P)
- megaAVR-0 series --- Arduino UNO WiFi Rev.2 (ATmega4809)
- AVR_DA/DB Series --- Microchip Curio City

> After installing the firmware, we recommend installing an additional 10uF capacitor between /RESET and GND to disable the auto-reset function.

Many of the technical elements come from the knowledge gained from the process of developing [UPDI4AVR](https://github.com/askn37/multix-zinnia-updi4avr-firmware-builder/).
Since UPDI4AVR can only be installed on megaAVR-0 or later devices, his JTAG2UPDI (Clone) was created to make effective use of old Arduino.

## Technical information

### NVMCTRL

NVMCTRL specifications differ depending on each system. The versions are classified into versions 0 to 5, depending on the number read from the SIB. They are not compatible with each other because they have different control register positions and definition values, and different control methods.

A list of control register definitions for each version is shown below.

|Offset|version 0|version 2|version 3|version 5|
|-|-|-|-|-|
|Series|(tiny,mega)AVR|AVR_DA/DB/DD|AVR_EA|AVR_EB
|$00|CTRLA|CTRLA|CTRLA|CTRLA
|$01|CTRLB|CTRLB|CTRLB|CTRLB
|$02|STATUS|STATUS|-|CTRLC
|$03|INTCTRL|INTCTRL|-|-
|$04|INTFLAGS|INTFLAGS|INTCTRL|INTCTRL
|$05|-|-|INTFLAGS|INTFLAGS
|$06|DATAL|DATAL|STATUS|STATUS
|$07|DARAH|DARAH|-|-
|$08|ADDRL|ADDR0|DATAL|DATAL
|$09|ADDRH|ADDR1|DATAH|DATAH
|$0A|-|ADDR2|-|-
|$0B|-|_ADDR3_|-|-
|$0C|-|-|ADDR0|ADDR0
|$0D|-|-|ADDR1|ADDR1
|$0E|-|-|ADDR2|ADDR2
|$0F|-|-|_ADDR3_|_ADDR3_

> Symbols in italics are defined but do not actually function. \
> No known implementation of version 1. \
> Version 4 is assigned to AVR_DU, but currently there is no actual version and no information.

The most significant bit of `NVMCTRL_ADDR2` has a special meaning: 0 is used as a flag to specify the address of the general data area, and 1 is used as a flag to specify the address of the FLASH code area (currently up to 128KiB). Version 0 does not have this, and data and code are not differentiated within the same 64KiB space.

In all versions, FLASH memory is word-oriented. Operations that do not follow word alignment will not yield the expected results.

NVM writing writes memory data directly to the target address (using UPDI's ST/STS command or assembly's ST/SPM command). Then, the specified address is reflected in the ADDR register, the memory data is reflected in the DATA register, and then passed to internal transfer processing.

#### NVMCTRL version 0

This system is the only one with a 16-bit address bus system, and the code space is within 64 KiB, the same as the data space. However, the actual code area start address offset starts from 0x8000 for the tinyAVR series and 0x4000 for the megaAVR series, so the two must be treated separately.

In addition, this system has the following characteristics:

-Has a special working buffer memory that does not appear in memory space and is shared by FLASH and his EEPROM.
- You can continue to write memory data to both FLASH and EEPROM as long as the buffer memory range is not exceeded.
- After filling the buffer memory, write the desired command code to CTRLA. When the controller is activated and the target processing is completed, the processing result is reflected in the STATUS register. The controller is deactivated at the end of processing.
- EEPROM can also be written in bulk with the same page granularity as FLASH, resulting in faster processing.
-USERROW can be written using the same command code as EEPROM. (However, you should use a specially prepared method.)
- FUSE memory has special dedicated write commands. After writing directly to the DATA and ADDR registers, he writes a dedicated command to CTRLA.

#### NVMCTRL version 2

This system has no working buffer memory. It has only one word buffer (that is, DATAL/H register). Therefore, the processing procedure is different.

- Write the desired command code to CTRLA and activate the controller.
- Write memory data to the target NVM address using ST/SPM instructions.
- Check the STATUS register, and after completing the desired processing, write the NOCMD (or NOOP) command to CTRLA to stop the controller.
- Memory data can be written continuously in the FLASH area as long as it does not exceed the page range.
- In the EEPROM area, only 1 word granularity (within 2 bytes) can be written continuously.
- FUSE can be written using the same command codes as EEPROM.
- USERROW can be written using the same command code as FLASH. (However, you should use a specially prepared method.)

Only in this series, the FLASH page granularity is 512 bytes. However, since NVM writers that support this size are not common, `AVRDUDE` attempts to write his data in two 256-byte blocks.

#### NVMCTRL version 3,5

This system has different working buffer memories for his FLASH and EEPROM. Therefore, memory data can be written continuously within that page range.

- Write a NOCMD (or NOOP) command to CTRLA to ensure controller deactivation.
- Write memory data to the target NVM address using ST/SPM instructions.
-Both FLASH and his EEPROM can write memory data continuously as long as they do not exceed their respective buffer memory ranges (128 bytes and 8 bytes).
- After filling the buffer memory, write the desired command code to CTRLA. When the controller is activated and the target processing is completed, the processing result is reflected in the STATUS register. The controller is deactivated at the end of processing.
- FUSE can be written using the same command codes as EEPROM.
- USERROW can be written using the same command code as FLASH. (However, you should use a specially prepared method.)
- BOOTROW (specific to version 5) uses the same directive codes as FLASH.

In his NVM rewriting work from UPDI, he does not use CTRLC, so versions 3 and 5 can share the same control processing. However, since many of the physical addresses in the NVM area are different, they are handled differently.

### Partial memory block write

To be able to use the `-D` option of `AVRDUDE` and the `write/erase <memtype>` command in interactive mode, each memory area must support page erasure. The original JTAG2UPDI could not handle these. What it actually does is what a typical bootloader would do: first erase the page at the specified address, then write the given data to its memory.

> JTAGICE mkII's original XMEGA memory page erase instruction is not used in `AVRDUDE`. Instead, normal memory read/write instructions are used instead.

At this time, in the case of a 512-byte page, since `AVRDUDE` is designed to divide the data into multiple memory blocks and send the data, consideration must be given to determining whether or not the page can be erased depending on the starting address. For the same reason, if a setting different from the page granularity of the actual device is written in the `AVRDUDE` configuration file, partial memory block writing will not show the correct processing result (the buffer contents will become invalid).

### Locked device compatible

"Device locking" is one of the functions implemented in devices mainly for security reasons. Locking prevents unauthorized tampering or reading of the contents of the flash memory or EEPROM stored in the device. However, access to UPDI is not denied, and full chip erase and blind writes to the USERROW area are permitted.

To lock the device, simply change the LOCK_BITS field to a value other than the default.

```sh
# Device lock operation
$ avrdude ... -U lock:w:0:m
```

For locked devices, only writing to the USERROW area is allowed. Since reading is not allowed, readback verification is not possible. In other words, you need at least the triple option `-F -V -U`. Only the application previously written to the device can know whether the writing was successful or not.

On the other hand, device signatures cannot be read from locked devices. If there is no `-F` option, you will not be able to proceed with any further operations.

Note that in the case of JTAG2UPDI (Clone), a false signature (when `ENABLE_PSEUDO_SIGNATURE` is enabled) is returned from the locked device, so for example, in the case of a locked ATmega4809, the following hint can be obtained (probably).

```plain
$ avrdude ... -FVU userrow:w:12,34,56,78:m

avrdude warning: bad response to enter progmode command: RSP_ILLEGAL_MCU_STATE
avrdude warning: bad response to enter progmode command: RSP_ILLEGAL_MCU_STATE
avrdude: AVR device initialized and ready to accept instructions
avrdude: device signature = 0x1e6d30 (probably megaAVR-0 locked device)
avrdude warning: expected signature for ATmega4809 is 1E 96 51

avrdude: processing -U userrow:w:12,34,56,78:m
avrdude: reading input file 12,34,56,78 for userrow/usersig
         with 4 bytes in 1 section within [0, 3]
         using 1 page and 60 pad bytes
avrdude: writing 4 bytes userrow/usersig ...
Writing | ################################################## | 100% 0.01 s 
avrdude: 4 bytes of userrow/usersig written

avrdude done.  Thank you.
```

To unlock a locked device, use the `-e -F` option to force a full chip erase. Furthermore, use `-U` to restore the lock key correctly. If you do not return the lock key, you can only unlock the door for a short period of time until the power is turned off.

```sh
# Device unlock operation for tinyAVR and megaAVR
$ avrdude ... -eFU lock:w:0xC5:m

# Device unlocking operation for AVR_Dx/Ex
$ avrdude ... -eFU lock:w:0x5C,0xC5,0xC5,0x5C:m
```

> If the UPDI control pin is disabled in the FUSE settings, it cannot be overridden and restored in the JTAG2UPDI implementation.

### Enhanced memory range specification violation detection

This is a function that specifically rejects the specification of an unacceptable amount of write/read data. If rejected, `AVRDUDE` often falls back to single-byte read/write operations, so this is intentionally controlled from the JTAG2UPDI side. The cause of the error is usually a mistake in editing the configuration file or a mistake in specifying parts.

- Do not allow reading or writing of 0 bytes. This shouldn't happen unless you set it intentionally. (but not impossible)
- Do not allow EEPROM writes with data lengths exceeding the true (limited by NVMCTRL version) page granularity.
- Writing to the FLASH area is prohibited unless it matches the page granularity or a special defined value.

### CMND_GET_PARAMETER

Supports `PAR_TARGET_SIGNATURE` inquiry. This reads and returns the SIB (System Information Block) from UPDI if possible. SIB is not a general memory read, so using `CMND_GET_PARAMETER` does not violate the JTAGICE mkII protocol rules. The returned data length is 32 bytes, unlike `PDI/SPI` devices (which are fixed at 2 bytes).

> For `PP/HV/PDI/SPI` devices, the `PAR_TARGET_SIGNATURE` query is used because the device signature cannot be read by normal memory reading. Devices from the XMEGA generation onwards do not use the device signature, as it is located in the normal IO memory area, and instead use normal memory reads.

## Build options

These macro declarations are provided in `sys.h`.

### NO_ACK_WRITE

Enabled by default. Allow UPDI bulk data writing. Disabling it will reduce memory write speed by half, but that is the only effect.

### DISABLE_HOST_TIMEOUT

Disabled by default. Client side JATG communication timeout is not limited. This is useful when using interactive mode a lot. However, if the host implements his keepalive, there is no need to enable it.

> If you cannot succeed without lowering the UPDI communication speed, the wiring load is excessive. This can usually be improved by removing excessive series resistors and pull-up resistors and making the wiring short enough.

### DISABLE_TARGET_TIMEOUT

Disabled by default. Do not limit UDPI communication timeout. It should not be used as it has significant side effects.

### INCLUDE_EXTRA_INFO_JTAG

Disabled by default. Add the obtained detailed device information to the response of the `SET_DEVICE_DESCRIPTPOR` packet. If you run `AVRDUDE` with `-vvvv`, it will be displayed, but it is not human readable.

### ENABLE_PSEUDO_SIGNATURE

Enabled by default. If UDPI is not disabled and the device is locked, a pseudo device signature will be returned. The following six types of pseudo signatures are currently generated.

|Signature|Description|
|-|-|
|0x1e 0x74 0x30|Locked tinyAVR-0/1/2
|0x1e 0x6d 0x30|Locked megaAVR-0
|0x1e 0x41 0x32|Locked AVR_DA/DB/DD
|0x1e 0x41 0x33|Locked AVR_EA
|0x1e 0x41 0x34|Locked AVR_DU
|0x1e 0x41 0x35|Locked AVR_EB

By adding the corresponding part settings to the configuration file, you will be able to easily visualize the differences. At the same time, this is also a summary of the only SIB that can be obtained from a locking device, making it clear that the device can be unlocked without HV control.

```sh
#------------------------------------------------------------
# Locked device pseudo signature
#------------------------------------------------------------

part
    id        = "tinyAVR-0/1/2 locked device";
    signature = 0x1e 0x74 0x30;
;

part
    id        = "megaAVR-0 locked device";
    signature = 0x1e 0x6d 0x30;
;

part
    id        = "AVR-DA/DB/DD locked device";
    signature = 0x1e 0x41 0x32;
;

part
    id        = "AVR-EA locked device";
    signature = 0x1e 0x41 0x33;
;

part
    id        = "AVR-DU locked device";
    signature = 0x1e 0x41 0x34;
;

part
    id        = "AVR-EB locked device";
    signature = 0x1e 0x41 0x35;
;
```

When this feature is disabled, the device signature always responds with `0xff 0xff 0xff`.

## Copyright and Contact

Twitter(X): [@askn37](https://twitter.com/askn37) \
BlueSky Social: [@multix.jp](https://bsky.app/profile/multix.jp) \
GitHub: [https://github.com/askn37/](https://github.com/askn37/) \
Product: [https://askn37.github.io/](https://askn37.github.io/)

Copyright (c) 2023 askn (K.Sato) multix.jp \
Released under the MIT license \
[https://opensource.org/licenses/mit-license.php](https://opensource.org/licenses/mit-license.php) \
[https://www.oshwa.org/](https://www.oshwa.org/)

