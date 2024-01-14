/*
 * JTAG2.h
 *
 * Created: 12-11-2017 11:10:31
 *  Author: JMR_2
 *  Author: askn37 at github.com
 */

#ifndef JTAG2_H_
#define JTAG2_H_

#include "sys.h"

namespace JTAG2 {

  // *** Parameter IDs ***
  enum parameter {
    PAR_TARGET_SIGNATURE = 0x1D,  // (Extended) Return to UPDI SIB
    PAR_PDI_OFFSET_START = 0x32,  // (Extended) Not used but responsive
    PAR_PDI_OFFSET_END   = 0x33,  // (Extended) Not used but responsive
    PAR_HW_VER    = 0x01,
    PAR_FW_VER    = 0x02,
    PAR_EMU_MODE  = 0x03,
    PAR_BAUD_RATE = 0x05,
    PAR_VTARGET   = 0x06
  };

  // *** valid values for PARAM_BAUD_RATE_VAL
  enum baud_rate {
    BAUD_2400   = 0x01,
    BAUD_4800,
    BAUD_9600,
    BAUD_19200,         // default
    BAUD_38400,
    BAUD_57600,
    BAUD_115200,
    BAUD_14400,
        // Extension to jtagmkII protocol: extra baud rates, standard series.
    BAUD_153600,
    BAUD_230400,
    BAUD_460800,
    BAUD_921600,
        // Extension to jtagmkII protocol: extra baud rates, binary series.
    BAUD_128000,
    BAUD_256000,
    BAUD_512000,
    BAUD_1024000,
        // Extension to jtagmkII protocol: extra baud rates, binary series.
    BAUD_150000,
    BAUD_200000,
    BAUD_250000,
    BAUD_300000,
    BAUD_400000,
    BAUD_500000,
    BAUD_600000,
    BAUD_666666,
    BAUD_1000000,
    BAUD_1500000,
    BAUD_2000000,
    BAUD_3000000,
    BAUD_LOWER = BAUD_2400,
    BAUD_UPPER = BAUD_3000000
  };

  // *** Parameter Values ***
  constexpr uint8_t PARAM_HW_VER_M_VAL      = 0x01;
  constexpr uint8_t PARAM_HW_VER_S_VAL      = 0x01;
  constexpr uint8_t PARAM_FW_VER_M_MIN_VAL  = 0x00;
  constexpr uint8_t PARAM_FW_VER_M_MAJ_VAL  = 0x06;
  constexpr uint8_t PARAM_FW_VER_S_MIN_VAL  = 0x00;
  constexpr uint8_t PARAM_FW_VER_S_MAJ_VAL  = 0x06;
  extern uint8_t PARAM_EMU_MODE_VAL;  // init=2, SPI=3
  extern uint8_t ConnectedTo;
  extern baud_rate PARAM_BAUD_RATE_VAL;
  constexpr uint16_t PARAM_VTARGET_VAL      = 5000;

  // *** General command constants ***
  enum cmnd {
    CMND_SIGN_OFF         = 0x00,
    CMND_GET_SIGN_ON      = 0x01,
    CMND_SET_PARAMETER    = 0x02,
    CMND_GET_PARAMETER    = 0x03,
    CMND_WRITE_MEMORY     = 0x04,
    CMND_READ_MEMORY      = 0x05,
    CMND_GO               = 0x08,
    CMND_RESET            = 0x0b,
    CMND_SET_DEVICE_DESC  = 0x0c,
    CMND_GET_SYNC         = 0x0f,
    CMND_ENTER_PROGMODE   = 0x14,
    CMND_LEAVE_PROGMODE   = 0x15,
    CMND_XMEGA_ERASE      = 0x34,
    CMND_SET_XMEGA_PARAMS = 0x36  /* not supported */
  };
  // *** JTAG Mk2 Single byte status responses ***
  enum response {
    // Success
    RSP_OK                    = 0x80,
    RSP_PARAMETER             = 0x81,
    RSP_MEMORY                = 0x82,
    RSP_SIGN_ON               = 0x86,
    // Error
    RSP_FAILED                = 0xA0,
    RSP_ILLEGAL_PARAMETER     = 0xA1,
    RSP_ILLEGAL_MEMORY_TYPE   = 0xA2,
    RSP_ILLEGAL_MEMORY_RANGE  = 0xA3,
    RSP_ILLEGAL_MCU_STATE     = 0xA5,
    RSP_ILLEGAL_VALUE         = 0xA6,
    RSP_ILLEGAL_BREAKPOINT    = 0xA8,
    RSP_ILLEGAL_JTAG_ID       = 0xA9,
    RSP_ILLEGAL_COMMAND       = 0xAA,
    RSP_NO_TARGET_POWER       = 0xAB,
    RSP_DEBUGWIRE_SYNC_FAILED = 0xAC,
    RSP_ILLEGAL_POWER_STATE   = 0xAD
  };

  // *** memory types for CMND_{READ,WRITE}_MEMORY ***
  enum mem_type {
    MTYPE_IO_SHADOW     = 0x30,   // cached IO registers?
    MTYPE_SRAM          = 0x20,   // target's SRAM or [ext.] IO registers
    MTYPE_EEPROM        = 0x22,   // EEPROM, what way?
    MTYPE_EVENT         = 0x60,   // ICE event memory
    MTYPE_SPM           = 0xA0,   // flash through LPM/SPM
    MTYPE_FLASH_PAGE    = 0xB0,   // flash in programming mode
    MTYPE_EEPROM_PAGE   = 0xB1,   // EEPROM in programming mode
    MTYPE_FUSE_BITS     = 0xB2,   // fuse bits in programming mode
    MTYPE_LOCK_BITS     = 0xB3,   // lock bits in programming mode
    MTYPE_SIGN_JTAG     = 0xB4,   // signature in programming mode
    MTYPE_OSCCAL_BYTE   = 0xB5,   // osccal cells in programming mode
    MTYPE_CAN           = 0xB6,   // CAN mailbox
    MTYPE_FLASH         = 0xC0,   // xmega (app.) flash
    MTYPE_BOOT_FLASH    = 0xC1,   // xmega boot flash
    MTYPE_EEPROM_XMEGA  = 0xC4,   // xmega EEPROM in debug mode
    MTYPE_USERSIG       = 0xC5,   // xmega user signature
    MTYPE_PRODSIG       = 0xC6    // xmega production signature
  };

  // *** erase modes for CMND_XMEGA_ERASE ***
  enum erase_mode {
    XMEGA_ERASE_CHIP,
    XMEGA_ERASE_APP,
    XMEGA_ERASE_BOOT,
    XMEGA_ERASE_EEPROM,
    XMEGA_ERASE_APP_PAGE,
    XMEGA_ERASE_BOOT_PAGE,
    XMEGA_ERASE_EEPROM_PAGE,
    XMEGA_ERASE_USERSIG
  };

  // *** JTAGICEmkII packet ***
  constexpr uint8_t MESSAGE_START = 0x1B;
  constexpr int MAX_BODY_SIZE = 700;  // Note: should not be reduced to less than 300 bytes.
  union packet_t {
    uint8_t raw[6 + MAX_BODY_SIZE];
    struct {
      union {
        uint16_t number;
        uint8_t number_byte[2];
      };
      union {
        uint32_t size;
        uint16_t size_word[2];
        uint8_t size_byte[4];
      };
      uint8_t body[MAX_BODY_SIZE];
    };
  } extern packet;
  constexpr uint8_t TOKEN = 0x0E;

  /* old PP/HV/PDI device descriptor, for firmware version 6 or earlier */
  struct device_descriptor {  // LSB First
    unsigned char ucReadIO[8];            /*LSB = IOloc 0, MSB = IOloc63 */
    unsigned char ucReadIOShadow[8];      /*LSB = IOloc 0, MSB = IOloc63 */
    unsigned char ucWriteIO[8];           /*LSB = IOloc 0, MSB = IOloc63 */
    unsigned char ucWriteIOShadow[8];     /*LSB = IOloc 0, MSB = IOloc63 */
    unsigned char ucReadExtIO[52];        /*LSB = IOloc 96, MSB = IOloc511 */
    unsigned char ucReadIOExtShadow[52];  /*LSB = IOloc 96, MSB = IOloc511 */
    unsigned char ucWriteExtIO[52];       /*LSB = IOloc 96, MSB = IOloc511 */
    unsigned char ucWriteIOExtShadow[52]; /*LSB = IOloc 96, MSB = IOloc511 */
    unsigned char ucIDRAddress;           /*IDR address */
    unsigned char ucSPMCRAddress;         /*SPMCR Register address and dW BasePC */
    unsigned char ucRAMPZAddress;         /*RAMPZ Register address in SRAM I/O */
    unsigned char uiFlashPageSize[2];     /*Device Flash Page Size, Size = */
    unsigned char ucEepromPageSize;       /*Device Eeprom Page Size in bytes */
    unsigned char ulBootAddress[4];       /*Device Boot Loader Start Address */
    unsigned char uiUpperExtIOLoc[2];     /*Topmost (last) extended I/O */
    unsigned char ulFlashSize[4];         /*Device Flash Size */
    unsigned char ucEepromInst[20];       /*Instructions for W/R EEPROM */
    unsigned char ucFlashInst[3];         /*Instructions for W/R FLASH */
    unsigned char ucSPHaddr;              /* stack pointer high */
    unsigned char ucSPLaddr;              /* stack pointer low */
    unsigned char uiFlashpages[2];        /* number of pages in flash */
    unsigned char ucDWDRAddress;          /* DWDR register address */
    unsigned char ucDWBasePC;             /* base/mask value of the PC */
    unsigned char ucAllowFullPageBitstream; /* FALSE on ALL new */
    unsigned char uiStartSmallestBootLoaderSection[2];
    unsigned char EnablePageProgramming;  /* For JTAG parts only, */
    unsigned char ucCacheType;            /* CacheType_Normal 0x00, */
    unsigned char uiSramStartAddr[2];     /* Start of SRAM */
    unsigned char ucResetType;            /* Selects reset type. ResetNormal = 0x00 */
    unsigned char ucPCMaskExtended;       /* For parts with extended PC */
    unsigned char ucPCMaskHigh;           /* PC high mask */
    unsigned char ucEindAddress;          /* Selects reset type. [EIND address...] */
    unsigned char EECRAddress[2];         /* EECR memory-mapped IO address */
  };

  /* New Xmega device descriptor, for firmware version 7 and above */
  struct xmega_device_desc {  // LSB First
    unsigned char whatever[2];            // cannot guess; must be 0x0002
    unsigned char datalen;                // length of the following data, = 47
    unsigned char nvm_app_offset[4];      // NVM offset for application flash
    unsigned char nvm_boot_offset[4];     // NVM offset for boot flash
    unsigned char nvm_eeprom_offset[4];   // NVM offset for EEPROM
    unsigned char nvm_fuse_offset[4];     // NVM offset for fuses
    unsigned char nvm_lock_offset[4];     // NVM offset for lock bits
    unsigned char nvm_user_sig_offset[4]; // NVM offset for user signature row
    unsigned char nvm_prod_sig_offset[4]; // NVM offset for production sign. row
    unsigned char nvm_data_offset[4];     // NVM offset for data memory (SRAM + IO)
    unsigned char app_size[4];            // size of application flash
    unsigned char boot_size[2];           // size of boot flash
    unsigned char flash_page_size[2];     // flash page size
    unsigned char eeprom_size[2];         // size of EEPROM
    unsigned char eeprom_page_size;       // EEPROM page size
    unsigned char nvm_base_addr[2];       // IO space base address of NVM controller
    unsigned char mcu_base_addr[2];       // IO space base address of MCU control
  };

  // *** Parameter initialization ***

  // *** Packet functions ***
  bool receive();
  void answer();
  void delay_exec();

  // *** Set status function ***
  void set_status(response) __attribute__ ((noinline));
  void set_status(response, uint8_t) __attribute__ ((noinline));

  // *** General command functions ***
  void sign_on();
  void get_parameter();
  void set_parameter();
  void set_device_descriptor();

  // *** ISP command functions ***
  void enter_progmode();
  void leave_progmode();
  void go();
  /* void read_signature(); */
  void read_mem();
  void write_mem();
  void erase();
}

#endif /* JTAG2_H_ */
