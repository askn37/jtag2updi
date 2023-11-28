/*
 * NVM_v3.h (NVM_v5.h)
 *
 * Created: 05-11-2023 22:51:16
 *  Author: askn37 at github.com
 */

#ifndef NVM_V3_H_
#define NVM_V3_H_

#include <stdint.h>
#include "UPDI_lo_lvl.h"

namespace NVM_v3 {
  // *** Base Addresses ***
  enum base {
    /* The layout of version 3 is the same as vserion 2 */
    /* Below is the layout of version 5 */
    NVM_base    = 0x1000,   /* Base address of the NVM controller */
    Fuse_base   = 0x1050,   /* Base address of the fuses  EEPROM */
    Sig_base    = 0x1080,   /* Base address of the signature ROM */
    Boot_base   = 0x1100,   /* Base address of the bootrow FLASH */
    User_base   = 0x1200,   /* Base address of the userrow FLASH */
    EEPROM_base = 0x1400    /* Base address of the main EEPROM */
  };

  // *** NVM Registers (offsets from NVN_base are enum default values) ***
  enum reg {
    CTRLA,
    CTRLB,
    CTRLC,    // version 5 only
    Reserved_3,
    INTCTRL,
    INTFLAGS,
    STATUS,
    Reserved_7,
    DATA_lo,
    DATA_hi,
    Reserved_A,
    Reserved_B,
    ADDR_lo,
    ADDR_hi,
    ADDR_ho,  // MSB :0 select data bus(ST/LD), :1 select code bus(SPM/LPM)
    ADDR_hq   // version 5 only
  };

  // *** NVM Commands (Execute by updating CTRLA after writing to memory) ***
  enum cmnd {
    NOCMD       = 0x00,    /* No command */
    NOOP        = 0x01,    /* No operation */
    FLPW        = 0x04,    /* Flash Page Write */
    FLPERW      = 0x05,    /* Flash Page Erase and Write */
    FLPER       = 0x08,    /* Flash Page Erase */
    FLMPER2     = 0x09,    /* Flash 2-page erase enable (See eratta) */
    FLMPER4     = 0x0A,    /* Flash 4-page erase enable (See eratta) */
    FLMPER8     = 0x0B,    /* Flash 8-page erase enable (See eratta) */
    FLMPER16    = 0x0C,    /* Flash 16-page erase enable (See eratta) */
    FLMPER32    = 0x0D,    /* Flash 32-page erase enable (See eratta) */
    FLPBCLR     = 0x0F,    /* Flash Page Buffer Clear */
    EEPW        = 0x14,    /* EEPROM Page Write */
    EEPERW      = 0x15,    /* EEPROM Page Erase and Write */
    EEPER       = 0x17,    /* EEPROM Page Erase */
    EEPBCLR     = 0x1F,    /* EEPROM Page Buffer Clear */
    CHER        = 0x20,    /* Chip Erase Command (UPDI only) */
    EECHER      = 0x30     /* EEPROM Erase Command (UPDI only) */
  };

  // *** NVM Functions ***
  template <bool preserve_ptr>
  void command (uint8_t cmd) {
    uint32_t temp;
    /* preserve UPDI pointer if requested */
    if (preserve_ptr) temp = UPDI::ldptr_l();
    /* Execute NVM command */
    UPDI::sts_b_l(NVM_v3::NVM_base + NVM_v3::CTRLA, cmd);
    /* restore UPDI pointer if requested */
    if (preserve_ptr) UPDI::stptr_l(temp);
  }

  template <bool preserve_ptr>
  void wait () {
    uint32_t temp;
    /* preserve UPDI pointer if requested */
    if (preserve_ptr) temp = UPDI::ldptr_l();
    /* Wait while NVM is busy from previous operations */
    while (UPDI::lds_b_l(NVM_v3::NVM_base + NVM_v3::STATUS) & 0x03);
    /* restore UPDI pointer if requested */
    if (preserve_ptr) UPDI::stptr_l(temp);
  }

  void clear () {
    UPDI::sts_b_l(NVM_v3::NVM_base + NVM_v3::STATUS, 0);
  }
}

#endif /* NVM_V3_H_ */
