/*
 * JTAG2.cpp
 *
 * Created: 08-12-2017 19:47:27
 *  Author: JMR_2
 *  Author: askn37 at github.com
 */

#include "JTAG2.h"
#include "JICE_io.h"
#include "NVM.h"
#include "NVM_v2.h"
#include "NVM_v3.h"
#include "NVM_v4.h"
#include "crc16.h"
#include "UPDI_hi_lvl.h"
#include "dbg.h"
#include <avr/wdt.h>

// *** Writeable Parameter Values ***
JTAG2::baud_rate JTAG2::PARAM_BAUD_RATE_VAL;
uint8_t JTAG2::ConnectedTo;
// *** JTAGICEmkII packet ***
JTAG2::packet_t JTAG2::packet;

// Local objects
namespace {
  // *** Local variables ***
  uint32_t before_addr;
  uint16_t before_seqnum;
  uint16_t flash_pagesize;
  uint8_t eeprom_pagesize;
  uint8_t nvmctrl_version = '0';
  #ifdef ENABLE_PSEUDO_SIGNATURE
  uint8_t dummy_sig[4] = { 0xff, 0xff, 0xff, 0xff };
  #endif
  bool chip_erased;
  bool bootrow_update;

  // *** Local functions declaration ***
  bool check_pagesize (uint16_t seed, uint16_t test);
  void NVM_write_bulk_single(uint32_t addr24, uint16_t repeat);
  void NVM_write_bulk(const uint32_t addr24, uint16_t repeat);
  void NVM_write_bulk_wide(const uint32_t addr24, uint16_t length);
  void NVM_write_userrow(const uint32_t addr24, uint16_t length);
  void NVM_write_fuse_v0(const uint16_t addr16, uint8_t data);
  void NVM_write_flash_v0(const uint16_t addr16, uint16_t length, bool is_bound);
  void NVM_write_flash_v2(const uint32_t addr24, uint16_t length, bool is_bound);
  void NVM_write_flash_v3(const uint32_t addr24, uint16_t length, bool is_bound);
  void NVM_write_flash_v4(const uint32_t addr24, uint16_t length, bool is_bound);
  void NVM_write_eeprom_v0(const uint16_t addr16, uint16_t length);
  void NVM_write_eeprom_v2(const uint16_t addr16, uint16_t length);
  void NVM_write_eeprom_v3(const uint16_t addr16, uint16_t length);
  void NVM_write_eeprom_v4(const uint16_t addr16, uint16_t length);
  void include_extra_info(const uint8_t sernumlen);
  void set_nvmctrl_version();

  // *** Signature response message ***
  FLASH<uint8_t> sgn_resp[29] {
    JTAG2::RSP_SIGN_ON, 1,  // MESSAGE_ID, COMM_ID
    1, JTAG2::PARAM_FW_VER_M_MIN_VAL, JTAG2::PARAM_FW_VER_M_MAJ_VAL, JTAG2::PARAM_HW_VER_M_VAL,
    1, JTAG2::PARAM_FW_VER_S_MIN_VAL, JTAG2::PARAM_FW_VER_S_MAJ_VAL, JTAG2::PARAM_HW_VER_S_VAL,
    0, 0, 0, 0, 0, 0,
    'J', 'T', 'A', 'G', 'I', 'C', 'E', ' ', 'm', 'k', 'I', 'I', 0
  };
}

// *** Packet functions ***
bool JTAG2::receive() {
  while (JICE_io::get() != MESSAGE_START) {
    #ifndef DISABLE_HOST_TIMEOUT
    if ((SYS::checkTimeouts() & WAIT_FOR_HOST)) return false;
    #endif
  }
  uint16_t crc = CRC::next(MESSAGE_START);
  for (uint16_t i = 0; i < 6; i++) {
    crc = CRC::next(packet.raw[i] = JICE_io::get(), crc);
  }
  if (packet.size_word[0] > sizeof(packet.body)) return false;
  if (JICE_io::get() != TOKEN) return false;
  crc = CRC::next(TOKEN, crc);
  for (uint16_t i = 0; i < packet.size_word[0]; i++) {
    crc = CRC::next(packet.body[i] = JICE_io::get(), crc);
  }
  if ((uint16_t)(JICE_io::get() | (JICE_io::get() << 8)) != crc) return false;
  return true;
}

void JTAG2::answer() {
  uint16_t crc = CRC::next(JICE_io::put(MESSAGE_START));
  for (uint16_t i = 0; i < 6; i++) {
    crc = CRC::next(JICE_io::put(packet.raw[i]), crc);
  }
  crc = CRC::next(JICE_io::put(TOKEN), crc);
  for (uint16_t i = 0; i < packet.size_word[0]; i++) {
    crc = CRC::next(JICE_io::put(packet.body[i]), crc);
  }
  JICE_io::put(crc);
  JICE_io::put(crc >> 8);
}

void JTAG2::delay_exec() {
  // wait for transmission complete
  JICE_io::flush();
  // set baud rate
  JICE_io::set_baud(PARAM_BAUD_RATE_VAL);
}

// *** Set status function ***
void JTAG2::set_status(response status_code) {
  packet.size_word[0] = 1;
  packet.body[0] = status_code;
}
void JTAG2::set_status(response status_code, uint8_t param) {
  packet.size_word[0] = 2;
  packet.body[0] = status_code;
  packet.body[1] = param;
  before_addr = ~0;
}

// *** General command functions ***

void JTAG2::sign_on() {
  // Initialize JTAGICE2 variables
  JTAG2::PARAM_BAUD_RATE_VAL = JTAG2::BAUD_19200;
  // Send sign on message
  packet.size_word[0] = sizeof(sgn_resp);
  for (uint8_t i = 0; i < sizeof(sgn_resp); i++) {
    packet.body[i] = sgn_resp[i];
  }
  JTAG2::ConnectedTo |= 0x02; //now connected to host
  bootrow_update = false;
  chip_erased = false;
  before_addr = ~0;
  nvmctrl_version = '0';
  #ifdef ENABLE_PSEUDO_SIGNATURE
  *(uint32_t*)&dummy_sig[4] = -1;
  #endif
}

void JTAG2::get_parameter() {
  uint8_t & status = packet.body[0];
  uint8_t & parameter = packet.body[1];
  switch (parameter) {
    case PAR_HW_VER:
      packet.size_word[0] = 3;
      packet.body[1] = PARAM_HW_VER_M_VAL;
      packet.body[2] = PARAM_HW_VER_S_VAL;
      break;
    case PAR_FW_VER:
      packet.size_word[0] = 5;
      packet.body[1] = PARAM_FW_VER_M_MIN_VAL;
      packet.body[2] = PARAM_FW_VER_M_MAJ_VAL;
      packet.body[3] = PARAM_FW_VER_S_MIN_VAL;
      packet.body[4] = PARAM_FW_VER_S_MAJ_VAL;  // select FWV=6,7
      break;
    case PAR_EMU_MODE:
      packet.size_word[0] = 2;
      packet.body[1] = 6; /* EMULATOR_MODE_PDI */
      break;
    case PAR_BAUD_RATE:
      packet.size_word[0] = 2;
      packet.body[1] = PARAM_BAUD_RATE_VAL;
      break;
    case PAR_VTARGET:
      packet.size_word[0] = 3;
      packet.body[1] = PARAM_VTARGET_VAL & 0xFF;
      packet.body[2] = PARAM_VTARGET_VAL >> 8;
      break;
    case PAR_TARGET_SIGNATURE:
    {
      packet.size_word[0] = 33;
      auto & sib = *(uint8_t (*)[32]) &packet.body[1];
      UPDI::read_sib(sib);
      break;
    }
    default:
      set_status(RSP_ILLEGAL_PARAMETER);
      return;
  }
  status = RSP_PARAMETER;
  return;
}

void JTAG2::set_parameter() {
  uint8_t param_type = packet.body[1];
  uint8_t param_val = packet.body[2];
  switch (param_type) {
    case PAR_EMU_MODE:
    case PAR_PDI_OFFSET_START:
    case PAR_PDI_OFFSET_END:
      break;
    case PAR_BAUD_RATE:
      // check if baud rate parameter is valid
      if ((param_val >= BAUD_LOWER) && (param_val <= BAUD_UPPER)) {
        PARAM_BAUD_RATE_VAL = (baud_rate)param_val;
        break;
      }
      // else fall through (invalid baud rate)
    default:
      set_status(RSP_ILLEGAL_PARAMETER);
      return;
  }
  set_status(RSP_OK);
}

void JTAG2::set_device_descriptor() {
  /* CMND_SET_DEVICE_DESC (Old style PP/HV/PDI/SPI type device descriptor) */
  const struct device_descriptor *desc = (struct device_descriptor*)(&packet.body[1]);
  flash_pagesize  = *(uint16_t*)(&desc->uiFlashPageSize[0]);
  eeprom_pagesize = desc->ucEepromPageSize;

  /* Get additional parameters compatible with UPDI4AVR */
  // if ((desc->ucSPMCRAddress & '0') == '0') nvmctrl_version = desc->ucSPMCRAddress;
  // if ((desc->ucIDRAddress & '0') == '0') hvupdi_support = desc->ucIDRAddress;

  // Now they've told us what we're talking to, and we will try to connect to it
  /* Initialize or enable UPDI */
  UPDI_io::put(UPDI_io::double_break);
  UPDI::stcs(UPDI::reg::Control_A, 0x06);
  set_nvmctrl_version();
  JTAG2::ConnectedTo |= 0x01; // connected to target

  #ifndef INCLUDE_EXTRA_INFO_JTAG
  // overwrite response set_nvmctrl_version
  set_status(RSP_OK);
  #else
  packet.body[0] = RSP_OK;
  #endif
}

// *** Target mode set functions ***
// *** Sets MCU in program mode, if possibe ***
void JTAG2::enter_progmode() {
  const uint8_t system_status = UPDI::CPU_mode<0xEF>();
  switch (system_status) {
    // reset in progress, may be caused by WDT
    case 0x21:
      /* fall-thru */
    // in normal operating mode, reset held (why can reset be held here, anyway? Needs investigation)
    case 0xA2:
      /* fall-thru */
    // in normal operation mode
    case 0x82:
      // Disable (ACC PHY.) Time-Out Detection
      UPDI::stcs(UPDI::reg::Control_A, 0x13); // DTD Set, RSD Unset, Guard Time Value := 16 bitclock

      // Reset the MCU now, to prevent the WDT (if active) to reset it at an unpredictable moment
      if (!UPDI::CPU_reset()){ //if we timeout while trying to reset, we are not communicating with chip, probably wiring error.
        set_status(RSP_NO_TARGET_POWER, system_status);
        break;
      }
      // At this point we need to check if the chip is locked, if so don't attempt to enter program mode
      // This is because the previous chip reset may enable a lock bit that was just written (it only becomes active after reset).
      if (UPDI::CPU_mode<0x01>()) {
        set_status(RSP_ILLEGAL_MCU_STATE, system_status | 0x01); // return system status (bit 0 will be set to indicate the MCU is locked)
        break;
      }
      // Now we have time to enter program mode (this mode also disables the WDT)
      // Write NVM unlock key (allows read access to all addressing space)
      UPDI::write_key(UPDI::NVM_Prog);
      // Request reset
      if (!UPDI::CPU_reset()){ //if we timeout while trying to reset, we are not communicating with chip, probably wiring error.
        set_status(RSP_NO_TARGET_POWER, system_status);
        break;
      }
      /* fall-thru */
    // already in program mode
    case 0x08: //make sure we're really in programming mode
      // Turn on LED to indicate program mode
      SYS::setLED();
      #if defined(DEBUG_ON)
      // report the chip revision
      DBG::debug('R',UPDI::lds_b_l(0x0F01));
      #endif
      set_status(RSP_OK);
      // Optional debug info; moved to separate function to declutter code
      include_extra_info( (nvmctrl_version == '0') ? 9 : 15 );
      break;
    default:
      // If we're somehow NOT in programming mode now, that's no good - inform host of this unfortunate state of affairs
      set_status(RSP_ILLEGAL_MCU_STATE, system_status); // return whatever system status caused this error
  }
}

// *** Sets MCU in normal runnning mode, if possibe ***
void JTAG2::leave_progmode() {
  const uint8_t system_status = UPDI::CPU_mode<0xEF>();
  bool reset_ok = false;
  set_status(RSP_OK);
  if (system_status & 0x08) {
    // Please wait until NVMCTRL_STATUS completes before performing leave
    if      (nvmctrl_version == '0') NVM::wait<false>();
    else if (nvmctrl_version == '2') NVM_v2::wait<false>();
    else if (nvmctrl_version == '4') NVM_v4::wait<false>();
    else          /* ver 3 or 5 */   NVM_v3::wait<false>();
  }
  switch (system_status) {
    // in program mode
    case 0x08:
    case 0x83:
      reset_ok = UPDI::CPU_reset();
      // already in normal mode
    /* fall-thru */
    case 0x82:
      // Turn off LED to indicate normal mode
      SYS::clearLED();
      if (!reset_ok) {
        /* this is a strange situation indeed, but tell the host anyway! */
        set_status(RSP_NO_TARGET_POWER, system_status);
      }
      break;
    // in other modes fail and inform host of wrong mode
    default:
      set_status(RSP_ILLEGAL_MCU_STATE, system_status);
  }
}

// The final command to make the chip go back into normal mode, shutdown UPDI, and start running normally
void JTAG2::go() {
  UPDI::stcs(UPDI::reg::Control_B, 0x04); //set UPDISIS to tell it that we're done and it can stop running the UPDI peripheral.
  JTAG2::ConnectedTo &= ~(0x01); //record that we're no longer talking to the target
  set_status(RSP_OK);

  /* Preliminary: After updating BOOTROW of AVR_EB,              */
  /* the behavior will not be stable unless the system is reset. */
  if (bootrow_update) wdt_enable(WDTO_1S);
}

  // *** Read/Write/Erase functions ***
  void JTAG2::read_mem() {
    /***
      The differences from the original are as follows:
        - 16-bit and 24-bit address types can be used interchangeably.
        - Allow units of bytes or words.
        - Prohibit reading of 0 units.
        - Prevent reading more than 256 units.
    ***/
    const uint8_t cpu_mode = UPDI::CPU_mode();
    uint32_t address = *(uint32_t*)&packet.body[6];
    uint16_t length = *(uint16_t*)&packet.body[2];
    uint8_t *p = &packet.body[1];

    if (length == 0 || (((length >> 8 ? length >> 1 : length) - 1) >> 8)) {
      /* More than 256 repeat units are not allowed */
      /* As a result, zero length will return an error before processing */
      set_status(RSP_ILLEGAL_MEMORY_RANGE, nvmctrl_version);
      return;
    }

    packet.size_word[0] = length + 1;
    packet.body[0] = RSP_MEMORY;

    if (cpu_mode & 1) {
      /* Returns a dummy value if the locked chip */
      /* Specify `-F` to continue the operation */
      #ifdef ENABLE_PSEUDO_SIGNATURE
      const bool sign_jtag = packet.body[1] == MTYPE_SIGN_JTAG;
      #endif
      do {
        #ifdef ENABLE_PSEUDO_SIGNATURE
        *p++ = sign_jtag
          ? dummy_sig[(uint8_t)(address++) & 3]
          : 0xff;
        #else
        *p++ = 0xff;
        #endif
      } while (--length);
      return;
    }

    if (cpu_mode != 8){
      /* fail if not in program mode */
      set_status(RSP_ILLEGAL_MCU_STATE, 1);
      return;
    }

    /* in program mode */
    const bool is_word = length >> 8;
    if (is_word) {
      /* Allows reading from 258 to 512 bytes */
      /* Odd lengths will be truncated */
      length >>= 1;
    }

    /* Even if it is a 16bit bus-only product */
    /* Overflowing high-order bits are simply ignored */
    /* Set the number of repetitions (0 to 255) */
    UPDI::stptr_l(address);
    UPDI::rep(length - 1);

    /* Perform bulk read */
    UPDI_io::put(UPDI::SYNCH);
    UPDI_io::put(is_word ? 0x25 : 0x24);  // ST_INC_W/B
    do {           *p++ = UPDI_io::get();
      if (is_word) *p++ = UPDI_io::get();
    } while (--length);
  }

  void JTAG2::write_mem() {
    uint32_t address = *(uint32_t*)&packet.body[6];
    uint16_t length = *(uint16_t*)&packet.body[2];
    uint8_t cpu_mode;
    uint8_t mem_type = packet.body[1];

    set_status(RSP_OK);
    /* Received packet error retransmission exception */
    if (before_seqnum == packet.number) return;

    cpu_mode = UPDI::CPU_mode();
    if ((cpu_mode & 1) && mem_type == MTYPE_USERSIG) {
      /* Pass when writing USERROW to the locking device (using '-DFVU') */
      /* In all versions, writes to USERROW should be written   */
      /* using UROWKEY (and to a memory buffer that is          */
      /* automatically overlapped on top of USERROW).           */
      /* There are other methods, but they are not recommended. */
      /* NVMv4,5 BOOTROW address ranges cannot be handled here. */
      NVM_write_userrow(address, length);
      if (packet.body[0] == RSP_OK) before_seqnum = packet.number;
      return;
    }

    if (cpu_mode != 0x08) {
      // fail if not in program mode
      set_status(RSP_ILLEGAL_MCU_STATE, cpu_mode);
      return;
    }

    if (mem_type == MTYPE_FLASH && (address >> 24)) {
      /* Fixed AVRDUDE specific memory type confusion. */
      /* High memory region is only allowed as a flash type. */
      mem_type = MTYPE_SRAM;
      address &= 0xFFFF;
    }

    /* USERROW of NVM V0 is EEPROM. */
    if (mem_type == MTYPE_USERSIG && nvmctrl_version == '0') {
      mem_type = MTYPE_EEPROM;
    }

    bool is_bound = !chip_erased;
    switch (mem_type) {
      case MTYPE_USERSIG:     // low-code-flash-region
        /* BOOTROW is accepted as MTYPE_FLASH. */
        /* According to ATDF, BOOTROW is a PRODSIG attribute. */
        /* PRODSIG is traditionally implemented as r/o, so there is a conflict. */
      case MTYPE_PRODSIG:     // low-code-flash-region
        before_addr = ~0;
      case MTYPE_FLASH:       // low-code-flash-region
        /* This kind of memory is always considered dirty. */
        is_bound = true;
        /* Continue to next case. */
      case MTYPE_FLASH_PAGE:  // high-code-flash-region (APPCODE)
      case MTYPE_BOOT_FLASH:  // high-code-flash-region (BOOTCODE)
      {
        if (!check_pagesize(flash_pagesize, length)) {
          /* Reject deta lengths that do not match page granularity. */
          set_status(RSP_ILLEGAL_MEMORY_RANGE, nvmctrl_version);
          *(uint16_t*)&packet.body[2] = flash_pagesize;
          packet.size_word[0] = 4;
          return;
        }
        /* A page block must be erased before writing to a new page block.
           The new AVRDUDE splits large page blocks into multiple queries to read-modify-write.
           This prevents atomic operations and requires special handling. */
        if (is_bound) {
          uint32_t block_addr = address & ~(flash_pagesize - 1);
          is_bound = before_addr != block_addr;
          before_addr = block_addr;
        }
        if      (nvmctrl_version == '0') NVM_write_flash_v0(address, length, is_bound);
        else if (nvmctrl_version == '2') NVM_write_flash_v2(address, length, is_bound);
        else if (nvmctrl_version == '4') NVM_write_flash_v4(address, length, is_bound);
        else          /* ver 3 or 5 */   NVM_write_flash_v3(address, length, is_bound);

        /* Preliminary: After updating BOOTROW of AVR_EB,              */
        /* the behavior will not be stable unless the system is reset. */
        // if (nvmctrl_version >= '4' && address < NVM_v3::User_base) bootrow_update = true;
        break;
      }
      default : {
        if (length == 0 || length > 256) {
          /* More than 256 repeat units are not allowed */
          /* As a result, zero length will return an error before processing */
          set_status(RSP_ILLEGAL_MEMORY_RANGE, nvmctrl_version);
          return;
        }
        switch (mem_type) {
          case MTYPE_SRAM:
            /* General writes to IO regions */
            NVM_write_bulk(address, length);
            break;
          case MTYPE_FUSE_BITS:
          case MTYPE_LOCK_BITS:
            if (nvmctrl_version == '0') {
              /* Only NVMCTRL version 0 uses a dedicated command to write */
              NVM_write_fuse_v0(address, packet.body[10]);
              break;
            }
            /* FUSE In other versions, it is treated the same as EEPROM */
          case MTYPE_EEPROM:
          case MTYPE_EEPROM_PAGE:
          case MTYPE_EEPROM_XMEGA:
            if      (nvmctrl_version == '0') NVM_write_eeprom_v0(address, length);
            else if (nvmctrl_version == '2') NVM_write_eeprom_v2(address, length);
            else if (nvmctrl_version == '4') NVM_write_eeprom_v4(address, length);
            else          /* ver 3 or 5 */   NVM_write_eeprom_v3(address, length);
            break;
          default:
            set_status(RSP_ILLEGAL_MEMORY_TYPE, nvmctrl_version);
        }
      }
    }
    /* Keep the sequence number if completed successfully */
    if (packet.body[0] == RSP_OK) before_seqnum = packet.number;
  }

  void JTAG2::erase() {
    set_status(RSP_OK);
    /* Received packet error retransmission exception */
    if (before_seqnum == packet.number) return;

    const uint8_t erase_type = packet.body[1];
    switch (erase_type) {
      case XMEGA_ERASE_CHIP:
      {
        #ifdef ENABLE_FAST_ERASE
        if (UPDI::CPU_mode<9>() == 8) {
          if (nvmctrl_version == '0') {
            NVM::wait<false>();
            NVM::command<false>(NVM::CHER);
            NVM::wait<false>();
            NVM::command<false>(NVM::PBC);
            NVM::wait<false>();
            NVM::command<false>(NVM::NOP);
          }
          else if (nvmctrl_version == '2') {
            NVM_v2::clear();
            NVM_v2::command<false>(NVM_v2::NOCMD);
            NVM_v2::command<false>(NVM_v2::CHER);
          }
          else {  /* version 3,4,5 */
            NVM_v3::clear();
            NVM_v3::command<false>(NVM_v3::NOCMD);
            NVM_v3::command<false>(NVM_v3::CHER);
            NVM_v3::wait<false>();
            if (nvmctrl_version != '4') {
              NVM_v3::command<false>(NVM_v3::NOCMD);
              NVM_v3::command<false>(NVM_v3::FLPBCLR);
              NVM_v3::wait<false>();
              NVM_v3::command<false>(NVM_v3::NOCMD);
              NVM_v3::command<false>(NVM_v3::EEPBCLR);
              NVM_v3::wait<false>();
            }
          }
          chip_erased = true;
          break;
        }
        #endif

        // Write Chip Erase key
        UPDI::write_key(UPDI::Chip_Erase);
        // Request 1st reset
        if (!UPDI::CPU_reset()){
          /* if the reset failed, inform host, break out because the rest ain't gonna work. */
          set_status(RSP_NO_TARGET_POWER, 0);
          break;
        }

        /* Locked AVR_EA requires wait processing */
        /* Wait for both the LOCKSTATUS bit of SYS_STATUS and CHIPERASE bit of KEY_STATUS to be cleared */
        while (UPDI::CPU_mode<0x01>());
        while (UPDI::ldcs(UPDI::reg::ASI_Key_Status) & 8);
        chip_erased = true;

        /* Program mode is canceled at this point */

        // Write Activation key
        UPDI::write_key(UPDI::NVM_Prog);

        // Request 2nd reset
        if (!UPDI::CPU_reset()){
          /* if the reset failed, inform host, break out because the rest ain't gonna work */
          set_status(RSP_NO_TARGET_POWER, 0);
          break;
        }

        /* Standby processing required for locked tinyAVR-0 */
        /* Wait for SYS_RESET LOCKSTATUS bit to be released and PROGMODE bit to be set */
        while (UPDI::CPU_mode<0x09>() != 0x08);

        /* No need to proceed to enter_progmode() */
        break;
      }
      /* In the case of AVRDUDE, there is no use below from here */
      #ifdef ENABLE_ERASE_PAGE_COMMAND
      case XMEGA_ERASE_APP_PAGE:
      case XMEGA_ERASE_BOOT_PAGE:
      case XMEGA_ERASE_USERSIG:
      {
        const uint32_t address = *(uint32_t*)&packet.body[2];
        if (nvmctrl_version == '0') {
          // If NVM v1, skip page erase for usersig memory, we use erase/write during the write step
          if (erase_type != XMEGA_ERASE_USERSIG) {
            NVM::wait<false>();
            UPDI::sts_b(address, 0xff);
            NVM::command<false>(NVM::ER);
          }
        }
        else if (nvmctrl_version == '2') {
          // Wait for completion of any previous NVM command then clear it with NOOP
          NVM_v2::wait<false>();
          NVM_v2::command<false>(NVM_v2::NOCMD);
          NVM_v2::command<false>(NVM_v2::FLPER);
          UPDI::sts_b_l(address, 0xff);
        }
        else if (nvmctrl_version == '4') {
          // Wait for completion of any previous NVM command then clear it with NOOP
          NVM_v4::wait<false>();
          NVM_v4::command<false>(NVM_v4::NOCMD);
          NVM_v4::command<false>(NVM_v4::FLPER);
          UPDI::sts_b_l(address, 0xff);
        }
        else {
          NVM_v3::wait<false>();
          NVM_v3::command<false>(NVM_v3::NOCMD);
          UPDI::sts_b_l(address, 0xff);
          NVM_v3::command<false>(NVM_v3::FLPER);
        }
        break;
      }
      case XMEGA_ERASE_EEPROM_PAGE:
        // Ignore page erase for eeprom, we use erase/write during the write step
        break;
      #endif
      default:
        set_status(RSP_FAILED);
    }
    /* Keep the sequence number if completed successfully */
    if (packet.body[0] == RSP_OK) before_seqnum = packet.number;
  }

// *** Local functions definition ***
namespace {
  bool check_pagesize (uint16_t seed, uint16_t test) {
    while (test != seed) {
      seed >>= 1;
      if (seed < 2) return false;
    }
    return true;
  }

  void NVM_write_bulk_single (uint32_t address, uint16_t length) {
    uint8_t *p = &JTAG2::packet.body[10];
    if (length == 1) return UPDI::sts_b_l(address, *p);
    UPDI::stptr_l(address);
    do { UPDI::stinc_b(*p++); } while (--length);
  }

  void NVM_write_bulk (const uint32_t address, uint16_t length) {
    uint8_t *p = &JTAG2::packet.body[10];
    if (length == 1) return UPDI::sts_b_l(address, *p);
    UPDI::stptr_l(address);
    #ifdef NO_ACK_WRITE
    UPDI::stcs(UPDI::reg::Control_A, 0x1B); // RSD ON
    #endif
    UPDI::rep(length - 1);
    UPDI_io::put(UPDI::SYNCH);
    UPDI_io::put(0x64); // ST_INC_B
    do {
      UPDI_io::put(*p++);
      #ifndef NO_ACK_WRITE
      UPDI_io::get();
      #endif
    } while (--length);
    #ifdef NO_ACK_WRITE
    UPDI::stcs(UPDI::reg::Control_A, 0x13); // RSD OFF
    #endif
  }

  void NVM_write_bulk_wide (const uint32_t address, uint16_t length) {
    if (length < 4) return NVM_write_bulk(address, length);
    uint8_t *p = &JTAG2::packet.body[10];
    length = length >> 1;
    UPDI::stptr_l(address);
    #ifdef NO_ACK_WRITE
    UPDI::stcs(UPDI::reg::Control_A, 0x0E); // RSD ON
    #endif
    UPDI::rep(length - 1);
    UPDI_io::put(UPDI::SYNCH);
    UPDI_io::put(0x65);   // ST_INC_W/B
    do {
      UPDI_io::put(*p++);
      #ifndef NO_ACK_WRITE
      UPDI_io::get();
      #endif
      UPDI_io::put(*p++);
      #ifndef NO_ACK_WRITE
      UPDI_io::get();
      #endif
    } while (--length);
    #ifdef NO_ACK_WRITE
    UPDI::stcs(UPDI::reg::Control_A, 0x06); // RSD OFF
    #endif
  }

  /* In all versions, writes to USERROW should be written   */
  /* using UROWKEY (and to a memory buffer that is          */
  /* automatically overlapped on top of USERROW).           */
  /* There are other methods, but they are not recommended. */
  void NVM_write_userrow(const uint32_t address, uint16_t length) {
    // before ASI_System_Status
    const uint8_t cpu_mode = UPDI::CPU_mode();

    /* Write USERROW mode change key */
    UPDI::write_key(UPDI::UserRow_Write);

    // Request reset
    if (!UPDI::CPU_reset()){
      set_status(JTAG2::RSP_NO_TARGET_POWER, nvmctrl_version);
      return;
    }

    /* Wait until UROWPROG bit ASI_System_Status is 1 */
    while (!(UPDI::ldcs(UPDI::reg::ASI_System_Status) & 0x04));

    /* Writes an array to the specified memory */
    NVM_write_bulk_wide(address, length);

    /* Write UROWDOWN and CLKREQ bit to ASI_System_Control_A */
    UPDI::stcs(UPDI::reg::ASI_System_Control_A, 0x03);

    /* Wait until UROWPROG bit ASI_System_Status is 0 */
    while (UPDI::ldcs(UPDI::reg::ASI_System_Status) & 0x04);

    /* Write UROWWRITE bit to ASI_Key_Status */
    UPDI::stcs(UPDI::reg::ASI_Key_Status, 0x20);

    /* Write NVMPROG mode change key */
    // This step is required for NVMCTRL version 0
    if (~cpu_mode & 1) UPDI::write_key(UPDI::NVM_Prog);

    // Request reset
    if (!UPDI::CPU_reset()){
      set_status(JTAG2::RSP_NO_TARGET_POWER, nvmctrl_version);
      return;
    }
  }

  /* Only NVMCTRL version 0 uses a dedicated command to write */
  void NVM_write_fuse_v0(const uint16_t address, uint8_t data) {
    NVM::wait<false>();
    UPDI::stptr_w(NVM::NVM_base + NVM::DATA_lo);
    UPDI::stinc_b(data);
    UPDI::stinc_b(0xFF);
    UPDI::stinc_b(address);
    UPDI::stinc_b(address >> 8);
    NVM::command<false>(NVM::WFU);
  }

  void NVM_write_flash_v0(const uint16_t address, uint16_t length, bool is_bound) {
    /* version 0 is a 32, 64 or 128 byte block */
    if (is_bound) {
      NVM::wait<false>();
      NVM::command<false>(NVM::PBC);
    }
    NVM::wait<false>();
    NVM_write_bulk(address, length);
    NVM::command<false>(NVM::ERWP);
  }

  void NVM_write_flash_v2(const uint32_t address, uint16_t length, bool is_bound) {
    /* version 2 is a 512 byte block */
    /* If the host PC's timeout is short, you can only write up to 500kbps */
    if (is_bound) {
      NVM_v2::wait<false>();
      NVM_v2::command<false>(NVM_v2::FLPER);
      UPDI::sts_b_l(address, 0xff);
      NVM_v2::wait<false>();
      NVM_v2::command<false>(NVM_v2::NOCMD);
    }
    NVM_v2::wait<false>();
    NVM_v2::command<false>(NVM_v2::FLWR);
    NVM_write_bulk_wide(address, length);
    NVM_v2::command<false>(NVM_v2::NOCMD);
  }

  void NVM_write_flash_v3(const uint32_t address, uint16_t length, bool is_bound) {
    /* version 3,5 is a 128 or 64 byte block */
    if (is_bound) {
      NVM_v3::wait<false>();
      NVM_v3::command<false>(NVM_v3::NOCMD);
      NVM_v3::command<false>(NVM_v3::FLPBCLR);
    }
    NVM_v3::wait<false>();
    NVM_v3::command<false>(NVM_v3::NOCMD);
    if (length < 4) NVM_write_bulk_single(address, length);
    else NVM_write_bulk(address, length);
    NVM_v3::command<false>(NVM_v3::FLPERW);
  }

  void NVM_write_flash_v4(const uint32_t address, uint16_t length, bool is_bound) {
    /* version 4 is a 512 byte block */
    /* If the host PC's timeout is short, you can only write up to 500kbps */
    if (is_bound) {
      NVM_v4::wait<false>();
      NVM_v4::command<false>(NVM_v4::FLPER);
      UPDI::sts_b_l(address, 0xff);
      NVM_v4::wait<false>();
      NVM_v4::command<false>(NVM_v4::NOCMD);
    }
    NVM_v4::wait<false>();
    NVM_v4::command<false>(NVM_v4::FLWR);
    NVM_write_bulk_wide(address, length);
    NVM_v4::command<false>(NVM_v4::NOCMD);
  }

  void NVM_write_eeprom_v0(const uint16_t address, uint16_t length) {
    /* Version 0 allows 32 or 64 byte bulk writes */
    NVM::wait<false>();
    NVM_write_bulk(address, length);
    NVM::command<false>(NVM::ERWP);
  }

  void NVM_write_eeprom_v2(const uint16_t address, uint16_t length) {
    if (length > 2) {
      /* version 2 can only write 2 bytes at a time */
      JTAG2::set_status(JTAG2::RSP_ILLEGAL_MEMORY_RANGE, nvmctrl_version);
      return;
    }
    NVM_v2::wait<false>();
    NVM_v2::command<false>(NVM_v2::EEERWR);
    NVM_write_bulk(address, length);
    NVM_v2::command<false>(NVM_v2::NOCMD);
  }

  void NVM_write_eeprom_v3(const uint16_t address, uint16_t length) {
    if (length > 8) {
      /* version 3,5 can only write 8 bytes at a time */
      JTAG2::set_status(JTAG2::RSP_ILLEGAL_MEMORY_RANGE, nvmctrl_version);
      return;
    }
    NVM_v3::wait<false>();
    NVM_v3::command<false>(NVM_v3::NOCMD);
    NVM_write_bulk(address, length);
    NVM_v3::command<false>(NVM_v3::EEPERW);
  }

  void NVM_write_eeprom_v4(const uint16_t address, uint16_t length) {
    if (length > 4) {
      /* version 4 can only write 4 bytes at a time */
      JTAG2::set_status(JTAG2::RSP_ILLEGAL_MEMORY_RANGE, nvmctrl_version);
      return;
    }
    NVM_v4::wait<false>();
    NVM_v4::command<false>(NVM_v4::EEERWR);
    NVM_write_bulk(address, length);
    NVM_v4::command<false>(NVM_v4::NOCMD);
  }

  void include_extra_info(const uint8_t sernumlen) {
    #if defined(INCLUDE_EXTRA_INFO_JTAG)
    // get the REV ID - I believe (will be confirming with Microchip support) that this is the silicon revision ID
    // this is particularly important with some of these chips - the tinyAVR and megaAVR parts do have differences
    // between silicon revisions, and AVR-DA-series initial rev is a basket case and will almost certainly be respun
    // before volume availability (take a look at the errata if you haven't already. There are fewer entries than on
    // tinyAVR 1-series, but they're BIG... like "digital input disabled after using analog input" "you know that
    // flashmap we said could be used with ld and st? We lied, you can only use it with ld, and even then, there are
    // cases where it won't work" "we didn't do even basic testing of 64-pin version, so stuff on those those pins is
    // hosed" (well, they didn't say they didn't test it, but it's bloody obvious that's how it happened))
    using namespace JTAG2;
    packet.size_word[0]=9+sernumlen;
    packet.body[1]='R';
    packet.body[2]='E';
    packet.body[3]='V';
    packet.body[4]=UPDI::lds_b_l(0x0F01);
    //hell, might as well get the chip serial number too!
    packet.body[5]= 'S';
    packet.body[6]= 'E';
    packet.body[7]= 'R';
    if (nvmctrl_version == '0') {
      UPDI::stptr_w(0x1103);
    } else if (nvmctrl_version == '4' || nvmctrl_version == '5') {
      UPDI::stptr_w(0x1090);
    } else {
      UPDI::stptr_l(0x1110);
    }
    UPDI::rep(sernumlen);
    packet.body[8]=UPDI::ldinc_b();
    for(uint8_t i=9;i<(9+sernumlen);i++){
      packet.body[i]=UPDI_io::get();
    }
    #ifdef DEBUG_ON
    DBG::debug("Serial Number: ");
    uint8_t *ptr=(uint8_t*)(&packet.body[8]);
    DBG::debug(ptr,sernumlen+1,0,1);
    #endif
    #elif defined(DEBUG_ON)
    // if we're not adding extended info in JTAG,
    // but debug is on, I guess we should still report this...
    uint8_t sernumber[16];
    if (nvmctrl_version == '0') {
      UPDI::stptr_w(0x1103);
    } else if (nvmctrl_version == '4' || nvmctrl_version == '5') {
      UPDI::stptr_w(0x1090);
    } else {
      UPDI::stptr_l(0x1100);
    }
    UPDI::rep(sernumlen);
    sernumber[0]=UPDI::ldinc_b();
    for(uint8_t i=1;i<(sernumlen+1);i++){
      sernumber[i]=UPDI_io::get();
    }
    #endif
  }

  void set_nvmctrl_version(){
    using namespace JTAG2;
    if (~ConnectedTo & 1) {
      UPDI_io::put(UPDI_io::double_break);
      UPDI::stcs(UPDI::reg::Control_A, 0x06);
      ConnectedTo |= 1;  // connected to target
    }
    // Cast body[1]...[32] as sub-array to receive SIB data
    auto & sib = *(uint8_t (*)[32]) &packet.body[1];
    UPDI::read_sib(sib);
    if (sib[9] == ':') {
      nvmctrl_version = sib[10];
      #ifdef ENABLE_PSEUDO_SIGNATURE
      dummy_sig[0] = 0x1e;
      dummy_sig[1] = sib[0] == ' ' ? sib[4] : sib[0];
      dummy_sig[2] = sib[10];
      #endif
    }
    #if defined(DEBUG_ON)
    DBG::debug(sib, 32, 1);
    #endif
    #ifdef INCLUDE_EXTRA_INFO_JTAG
    packet.size_word[0] = 33;
    #endif
  }
}

/* end of code */
