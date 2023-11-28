# JTAG2UPDI (Clone)

This is a static fork of the [ElTangas/jtag2updi](https://github.com/ElTangas/jtag2updi) firmware. Although most of the code is the same, there is no upstream linkage as it involves large-scale changes.

Branching point: https://github.com/ElTangas/jtag2updi/tree/07be876105e0b9cfedf2723b0ac88780bcae50d8

The difference between the two should be obtained by `diff -uBw jtag2updi-master/source jtag2updi-main/src`. `JTAG2.cpp` is almost a separate file.

[ORIGINAL READEME(en_US)](READEME_orig.md) [English version of this document(en_US)](READEME.md) [Japanese version of this document(ja_JP)](READEME_jp.md)

## What has changed

- Compatible with NVMCTRL version 0,2,3,5. The correspondence in the original text is up to 0 and 2.
  - Support tinyAVR-0/1/2, megaAVR-0, AVR_DA/DB/DD/EA/EB.
    - Compatible with UPDI layer SIB (System Information Block).
  - Operation confirmed devices: ATtiny202 ATmega4809 AVR32DA32 AVR128DB32 AVR32DD14 AVR64DD32 AVR64EA32 (as of November 2023)
 Compatible with AVRDUDE 7.3.
  - Operation confirmed: AVRDUDE 6.3 7.0 7.1 7.2 7.3 (under development as of November 2023)
  - Only partial memory blocks can be rewritten using the `-D` option. (as the bootloader does)
   - This allows you to also use the `write/elase <memtyp>` command in interactive mode.
  - Compatible with locked devices.
    - Blind writing of USERROW.
    - LOCK_BITS Unlock.
  - Enhanced memory range specification violation detection.
    - Bulk read/write of up to 256 words (512 bytes) for FLASH memory regions. Others can read/write in bulk up to 256 bytes.
    - Allows 256word FLASH bulk reading/writing of AVR_DA/DB/DD.
  - `230400`, `460800`, and `500000` can be practically specified in many environments with the `-b` option.
    - This does not apply to Arduino compatible machines. Note that when reading and writing large-capacity bulk, you are subject to host-side timeout/error constraints. (Generally, the faster the communication speed, the higher the USB packet loss rate)
  - Some future experimental expansion attempts.
    - Not compatible with High-Voltage control. This requires additional hardware and special control code. There are two different specifications for his HV control for current UPDI-enabled devices. There is no easy way to support both. That's what makes this problem so difficult.
- Firmware build and installation has been verified only for Arduino IDE 1.8.x/2.x. Not sure about other methods.
  - The `source` directory has been changed to `src` to match the Arduino IDE specifications. The location of the `jtag2updi.ino` file is also different.
  - Unsupported accompanying files have been removed. You may need to copy it from the original location.

Equipment that can be installed with firmware:

- ATmegaX8 series --- Arduino UNO Rev.3, Arduino Mini, Arduino Micro (ATmega328P)
- megaAVR-0 series --- Arduino UNO WiFi Rev.2 (ATmega4809)
- AVR_DA/DB Series --- Microchip Curio City

> After installing the firmware, we recommend installing an additional 10uF capacitor between RESET and GND to disable the auto-reset function.

Many of the technical elements come from the knowledge gained from the process of developing [UPDI4AVR](https://github.com/askn37/multix-zinnia-updi4avr-firmware-builder/).
Since UPDI4AVR can only be installed on megaAVR-0 or later devices, his JTAG2UPDI (Clone) was developed to make effective use of older Arduinos.

## Technical explanation

### NVMCTRL

NVMCTRL specifications differ depending on each system. The versions are classified into versions 0 to 5, depending on the numbers read from his SIB. They are not compatible with each other because they have different control register positions and definition values, and different control methods.

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

> Symbols in *italics* are defined but do not actually function. \
> No known implementation of version 1. \
> Version 4 is assigned to AVR_DU, but currently there is no actual version and no information.

The most significant bit of `NVMCTRL_ADDR2` has a special meaning: 0 is used as a flag to specify the address of the general data area, and 1 is used as a flag to specify the address of the FLASH code area (currently up to 128KiB). Version 0 does not have this, and data and code are not differentiated within the same 64KiB space.

In all versions, FLASH memory is word-oriented. If you do not follow word alignment, you will not get the expected results.

NVM writing writes memory data directly to the target address (using UPDI's ST/STS command or assembly's ST/SPM command). Then, the specified address is reflected in the ADDR register, the memory data is reflected in the DATA register, and then passed to internal transfer processing.

### MVMCTRL version 0

This system is the only one with a 16-bit address bus system, and the code space is within 64 KiB, the same as the data space. However, the actual code area start address offset starts from 0x8000 for the tinyAVR series and 0x4000 for the megaAVR series, so they must be treated separately.

In addition, this system has the following characteristics:

- It has a special working buffer memory that does not appear in the memory space, and is shared by FLASH and EEPROM.
- Writing a command code to CTRLA after filling the buffer memory starts the actual memory write/erase execution.
- EEPROM can also be written in bulk with the same page granularity as FLASH, resulting in faster processing.
- USERROW can be written using the same command code as EEPROM.
- FUSE memory has special dedicated write commands. After writing directly to the DATA and ADDR registers, he writes a dedicated command to CTRLA.

> Writing to USERROW uses a special method under UPDI control, so there is no need to worry about the actual memory type.

### MVMCTRL version 2

This system has no working buffer memory. It has only one word buffer (that is, DATAL/H register). Therefore, the processing procedure is different.

- Write the desired command code to CTRLA and activate the controller.
- Write memory data to the target NVM address using ST/SPM instructions.
- Check the STATUS register, and after completing internal processing, write the NOCMD (or NOOP) command to CTRLA to stop the controller.
- Memory data can be written continuously in the FLASH area as long as it does not exceed the page range.
- In the EEPROM area, only word granularity (2 bytes) can be written consecutively.
- FUSE can be written using the same command codes as EEPROM.
- USERROW can be written using the same command code as FLASH.

> Writing to USERROW uses a special method under UPDI control, so there is no need to worry about the actual memory type.

Only this series has a FLASH page granularity of 512 bytes. However, since NVM writers that support this size are not common, `AVRDUDE` tries to write it in two blocks of 256 bytes.

### MVMCTRL version 3,5

This system has different working buffer memories for his FLASH and EEPROM. Therefore, memory data can be written continuously within that page range.

- Write NOCMD (or NOOP) command to CTRLA to stop internal processing.
- Write memory data to the target NVM address using ST/SPM instructions.
- For both FLASH and his EEPROM, memory data can be written continuously as long as it does not exceed the page range.
- After filling the buffer memory, write the desired command code to CTRLA. When the controller is activated and the target processing is completed, the processing result is reflected in the STATUS register. The controller is deactivated at the end of processing.
- FUSE can be written using the same command codes as EEPROM.
- USERROW can be written using the same command code as FLASH.
- BOOTROW (specific to version 5) uses the same directive codes as FLASH.

> Writing to USERROW uses a special method under UPDI control, so there is no need to worry about the actual memory type.

Since CTRLC is not used in NVM rewrite work, versions 3 and 5 can share the same control logic. However, since many of the physical addresses in the NVM area are different, they are handled differently.

### Partial memory block write

To be able to use the `-D` option of `AVRDUDE` and the `write/erase <memtype>` command in interactive mode, each memory area must support page erasure. The original JTAG2UPDI did not support this, so these could not be used. What it actually does is what a typical bootloader would do: first erase the page at the specified address, then write the given data to its memory.

At this time, in NVMCTRL version 2, which has a 512-byte page, `AVRDUDE` divides the data into multiple memory blocks and sends the data, so it is necessary to consider whether or not to erase the page according to the starting address. For the same reason, if the AVRDUDE configuration file does not specify a setting that differs from the page granularity of the real device, partial memory block writes will not give correct results (because the buffer contents will be incorrect).

### Locked device compatible

Only USERROW can be written to a locked device. However, reading is not allowed. To make this possible, the following conditions had to be met.

- Signatures cannot be read from locked devices. `AVRDUDE` stops the sequence if the desired signature and the read signature do not match, unless the `-F` option is specified.
- If JTAG2UPDI (or any other NVM writer) returns an error before this determination, the sequence will not advance to the correct break position.
- Since the "silicon revision confirmation" memory read added in `AVRDUDE 7.3` is performed before the signature read, returning an error here will interfere with the sequence that should be performed after this. This cannot be avoided with `-F` or `-V`. Therefore, it was not possible to permanently unlock a locked device by writing a valid LOCK_BITS.

> This is because `AVRDUDE` violates the JTAGICE mkII protocol rules, but since it is a proprietary extension, no correction can be expected at this time.

As a logical consequence, it became necessary to modify JTAG2UPDI so that all memory reads from locked devices return normal responses filled with dummy values. By doing so, his USERROW write using `-F` and release operation using FUSE/LOCK write were enabled correctly.

### Enhanced memory range specification violation detection

This is a function that specifically rejects the specification of an unacceptable amount of write/read data. If rejected, `AVRDUDE` often falls back to single-byte read/write operations, so this is intentionally controlled from the JTAG2UPDI side. Usually, the cause of the error is a mistake in editing the configuration file. However, in the past, it was simply executed according to the settings, so it was not easy to understand why the abnormal operation occurred.

- Do not allow reading or writing of 0 bytes. This will not happen unless you set it intentionally. (but not impossible)
- Do not allow EEPROM writing with a data length that exceeds the true page granularity. This can especially occur when writing to EEPROM of the AVR_Dx family, where bulk writing is not possible.
- Writing to FLASH is prohibited unless it matches the page granularity. This disallows 1-byte unit writes due to fallback. This is especially true when writing the `-D` option (the buffer memory becomes invalid), which is fatal. Exceptionally, AVR_EB's BOOTROW allows him to write 64 bytes, and AVR_Dx allows him to write 256 bytes (half the page granularity).

> It is currently undecided which memory type value BOOTROW will actually be implemented with, but it is assumed that it will be one for FLASH use.

### Extensions inherited from UPDI4AVR

## CMND_GET_PARAMETER

Supports `PAR_TARGET_SIGNATURE` inquiry. This reads and returns the SIB (System Information Block) from UPDI if possible. Since SIB is not a memory read in the general sense, using `CMND_GET_PARAMETER` is required by the JTAGICE mkII protocol specification. The returned data length is 32 bytes unlike `PDI/SPI` devices (which are 2 bytes).

## Build options

These macro declarations are provided in `sys.h`.

### NO_ACK_WRITE

Enabled by default. Allow UPDI bulk data writing. Disabling it will reduce the transfer speed by half.

### DISABLE_HOST_TIMEOUT

Disabled by default. Client side JATG communication timeout is not limited. This is useful when using interactive mode. However, if the host side implements his keepalive, it is not necessary.

### DISABLE_TARGET_TIMEOUT

Disabled by default. Do not limit UDPI communication timeout. However, it has no practical meaning unless you disable his JTAG communication timeout on the host side.

### INCLUDE_EXTRA_INFO_JTAG

Disabled by default. Add the obtained detailed device information to the response of the `SET_DEVICE_DESCRIPTPOR` packet. If you run `AVRDUDE` with `-vvvv`, it will be displayed, but it is not human readable.

### ENABLE_PSEUDO_SIGNATURE

Enabled by default. Returns a pseudo signature when the device is locked. The following six types of pseudo signatures are currently generated.

|Code|Description|
|-|-|
|0x1e 0x74 0x30|Locked tinyAVR
|0x1e 0x6d 0x30|Locked megaAVR
|0x1e 0x41 0x32|Locked AVR_DA/DB/DD
|0x1e 0x41 0x33|Locked AVR_EA
|0x1e 0x41 0x34|Locked AVR_DU
|0x1e 0x41 0x35|Locked AVR_EB

By adding the corresponding part settings to the configuration file, you will be able to easily visualize the differences in the error output.

```sh
#------------------------------------------------------------
# Locked Device
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

> The 2nd byte indicates the first byte value of SIB, and the 3rd byte indicates the NVMCTRL version.

## Copyright and Contact

Twitter(X): [@askn37](https://twitter.com/askn37) \
BlueSky Social: [@multix.jp](https://bsky.app/profile/multix.jp) \
GitHub: [https://github.com/askn37/](https://github.com/askn37/) \
Product: [https://askn37.github.io/](https://askn37.github.io/)

Copyright (c) 2023 askn (K.Sato) multix.jp \
Released under the MIT license \
[https://opensource.org/licenses/mit-license.php](https://opensource.org/licenses/mit-license.php) \
[https://www.oshwa.org/](https://www.oshwa.org/)

