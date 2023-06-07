#include <stdint.h>
#include <stdio.h>
#include <scmp.h>

// #define DEBUG(...) printf(__VA_ARGS__)
#define DEBUG(...)

static inline uint16_t scmp_step(uint16_t px, int16_t disp) {
  return ((px + disp) & 0x0FFF) | (px & 0xF000);
}

void scmp_tick(scmp_t *scmp) {
  scmp->cycles++;
  
  if (scmp->cycles_left) {
    scmp->cycles_left--;
    return;
  } else {
    scmp->cycles_left = 1;
  }
  
  if ((scmp->sr & 0x10) && (scmp->sr & 0x08)) {
    scmp->sr &= 0xC7;
    
    uint16_t temp = scmp->p3;
    
    scmp->p3 = scmp->pc;
    scmp->pc = temp;
    
    scmp->cycles_left = 13; // DINT + XPPC
    
    scmp->cycles_left--;
    return;
  }
  
  uint8_t opcode[2];
  
  scmp->pc = scmp_step(scmp->pc, 1);
  opcode[0] = scmp->f_read(scmp, scmp->pc);
  
  DEBUG("(AC=0x%02X) (0x%02X) ", scmp->ac, opcode[0]);
  
  if (opcode[0] & 0x80) {
    scmp->pc = scmp_step(scmp->pc, 1);
    opcode[1] = scmp->f_read(scmp, scmp->pc);
  }
  
  if (opcode[0] & 0x40) {
    if (opcode[1] == 0x80) {
      opcode[1] = scmp->e;
    }
    
    uint8_t oper = (opcode[0] >> 3) & 0x07;
    int16_t disp = (int8_t)(opcode[1]);
    
    uint16_t address;
    uint8_t value;
    
    scmp->cycles_left = 6;
    
    if (opcode[0] & 0x80) {
      if ((opcode[0] & 0x07) == 4) {
        value = opcode[1];
        scmp->cycles_left += 4;
      } else if (opcode[0] & 0x04) {
        if (disp < 0) {
          scmp->px[opcode[0] & 0x03] = scmp_step(scmp->px[opcode[0] & 0x03], disp);
        }
        
        address = scmp->px[opcode[0] & 0x03];
        
        if (oper != 1) {
          value = scmp->f_read(scmp, address);
        }
        
        if (disp >= 0) {
          scmp->px[opcode[0] & 0x03] = scmp_step(scmp->px[opcode[0] & 0x03], disp);
        }
        
        scmp->cycles_left += 12;
      } else {
        address = scmp_step(scmp->px[opcode[0] & 0x03], disp);
        
        if (oper != 1) {
          value = scmp->f_read(scmp, address);
        }
        
        scmp->cycles_left += 12;
      }
    } else {
      value = scmp->e;
    }
    
    if (oper == 0) {
      DEBUG("LD", value);
      
      scmp->ac = value;
    } else if (oper == 1) {
      DEBUG("ST");
      
      scmp->f_write(scmp, address, scmp->ac);
    } else if (oper == 2) {
      DEBUG("AND");
      
      scmp->ac &= value;
    } else if (oper == 3) {
      DEBUG("OR");
      
      scmp->ac |= value;
    } else if (oper == 4) {
      DEBUG("XOR");
      
      scmp->ac ^= value;
    } else if (oper == 5) {
      DEBUG("DAD");
      
      // TODO: Decimal addition
      scmp->cycles_left += 5;
    } else if (oper == 6 || oper == 7) {
      scmp->cycles_left++;
      
      if (oper == 7) {
        DEBUG("CAD");
        
        value = ~value;
        scmp->cycles_left++;
      } else {
        DEBUG("ADD");
      }
      
      uint16_t result = scmp->ac + value + !!(scmp->sr & 0x80);
      scmp->sr = (scmp->sr & 0x3F) | ((!!(result & 0xFF00)) << 7) | (((!!(result & 0xFF00)) ^ (!!(result & 0x80))) << 6);
      
      scmp->ac = result;
    }
  } else if (opcode[0] & 0x80 && opcode[0] != 0x8F && opcode[1] == 0x80) {
    opcode[1] = scmp->e;
  }
  
  if (opcode[0] == 0x00) {
    DEBUG("HALT");
    
    // TODO: Halt
    scmp->cycles_left = 8;
  } else if (opcode[0] == 0x01) {
    DEBUG("XAE");
    
    uint8_t temp = scmp->ac;
    
    scmp->ac = scmp->e;
    scmp->e = temp;
    
    scmp->cycles_left = 7;
  } else if (opcode[0] == 0x02) {
    DEBUG("CCL");
    
    scmp->sr &= 0x7F;
    scmp->cycles_left = 5;
  } else if (opcode[0] == 0x03) {
    DEBUG("SCL");
    
    scmp->sr |= 0x80;
    scmp->cycles_left = 5;
  } else if (opcode[0] == 0x04) {
    DEBUG("DINT");
    
    scmp->sr &= 0xF7;
    scmp->cycles_left = 6;
  } else if (opcode[0] == 0x05) {
    DEBUG("IEN");
    
    scmp->sr |= 0x08;
    scmp->cycles_left = 6;
  } else if (opcode[0] == 0x06) {
    DEBUG("CSA");
    
    scmp->ac = scmp->sr;
    scmp->cycles_left = 5;
  } else if (opcode[0] == 0x07) {
    DEBUG("CAS");
    
    scmp->sr = scmp->ac;
    scmp->cycles_left = 6;
  } else if (opcode[0] == 0x08) {
    DEBUG("NOP");
    
    scmp->cycles_left = 5;
  } else if (opcode[0] == 0x19) {
    DEBUG("SIO");
    
    scmp->s_out = scmp->e & 1;
    scmp->e = (scmp->e >> 1) | ((uint8_t)(scmp->s_in) << 7);
    
    scmp->cycles_left = 5;
  } else if (opcode[0] == 0x1C) {
    DEBUG("SR");
    
    scmp->ac = (scmp->ac >> 1);
    scmp->cycles_left = 5;
  } else if (opcode[0] == 0x1D) {
    DEBUG("SRL");
    
    scmp->ac = (scmp->ac >> 1) | ((!!(scmp->sr & 0x80)) << 7);
    scmp->cycles_left = 5;
  } else if (opcode[0] == 0x1E) {
    DEBUG("RR");
    
    scmp->ac = (scmp->ac >> 1) | (scmp->ac << 7);
    scmp->cycles_left = 5;
  } else if (opcode[0] == 0x1F) {
    DEBUG("RRL");
    
    uint8_t carry = scmp->ac & 1;
    
    scmp->ac = (scmp->ac >> 1) | ((!!(scmp->sr & 0x80)) << 7);
    scmp->sr = (scmp->sr & 0x7F) | (carry << 7);
    
    scmp->cycles_left = 5;
  } else if ((opcode[0] & 0xFC) == 0x30) {
    DEBUG("XPAL");
    
    uint8_t temp = scmp->ac;
    
    scmp->ac = scmp->px[opcode[0] & 0x03] & 0xFF;
    scmp->px[opcode[0] & 0x03] = (scmp->px[opcode[0] & 0x03] & 0xFF00) | temp;
    
    scmp->cycles_left = 8;
  } else if ((opcode[0] & 0xFC) == 0x34) {
    DEBUG("XPAH");
    
    uint8_t temp = scmp->ac;
    
    scmp->ac = (scmp->px[opcode[0] & 0x03] >> 8) & 0xFF;
    scmp->px[opcode[0] & 0x03] = (scmp->px[opcode[0] & 0x03] & 0xFF) | ((uint16_t)(temp) << 8);
    
    scmp->cycles_left = 8;
  } else if ((opcode[0] & 0xFC) == 0x3C) {
    DEBUG("XPPC");
    
    uint16_t temp = scmp->pc;
    
    scmp->pc = scmp->px[opcode[0] & 0x03];
    scmp->px[opcode[0] & 0x03] = temp;
    
    scmp->cycles_left = 7;
  } else if (opcode[0] == 0x8F) {
    DEBUG("DLY");
    
    scmp->cycles_left = 13 + 2 * (uint32_t)(scmp->ac) + 514 * (uint32_t)(opcode[1]);
    scmp->ac = 0xFF;
  } else if ((opcode[0] & 0xFC) == 0x90) {
    DEBUG("JMP");
    
    scmp->pc = scmp->px[opcode[0] & 0x03] + (int8_t)(opcode[1]);
    scmp->cycles_left = 11;
  } else if ((opcode[0] & 0xFC) == 0x94) {
    DEBUG("JP");
    
    if (!(scmp->ac & 0x80)) {
      scmp->pc = scmp->px[opcode[0] & 0x03] + (int8_t)(opcode[1]);
      scmp->cycles_left = 11;
    } else {
      scmp->cycles_left = 9;
    }
  } else if ((opcode[0] & 0xFC) == 0x98) {
    DEBUG("JZ");
    
    if (!scmp->ac) {
      scmp->pc = scmp->px[opcode[0] & 0x03] + (int8_t)(opcode[1]);
      scmp->cycles_left = 11;
    } else {
      scmp->cycles_left = 9;
    }
  } else if ((opcode[0] & 0xFC) == 0x9C) {
    DEBUG("JNZ");
    
    if (scmp->ac) {
      scmp->pc = scmp->px[opcode[0] & 0x03] + (int8_t)(opcode[1]);
      scmp->cycles_left = 11;
    } else {
      scmp->cycles_left = 9;
    }
  } else if ((opcode[0] & 0xFC) == 0xA8) {
    DEBUG("ILD");
    
    // TODO: Increment and load
    scmp->cycles_left = 22;
  } else if ((opcode[0] & 0xFC) == 0xB8) {
    DEBUG("DLD");
    
    // TODO: Decrement and load
    scmp->cycles_left = 22;
  }
  
  scmp->cycles_left--; // This pseudo-cycle actually counts towards the total cycle count
  scmp->sr &= 0xCF;
  
  DEBUG("\n");
}
