#ifndef __PATA_H__
#define __PATA_H__

// Software implementation of the Parallel ATA protocol

#include <stdint.h>
#include <stdio.h>

// Status Register:
// 7--- 6--- 5--- 4--- 3--- 2--- 1--- 0---
// BSY  DRDY DF   DSC  DRQ  CORR IDX  ERR

// Error Register:
// 7--- 6--- 5--- 4--- 3--- 2--- 1--- 0---
//      UNC  MC   IDNF ABRT MCR  TKNF AMNF

typedef struct pata_t pata_t;

struct pata_t {
  FILE *io_file;
  size_t io_total_sectors;
  
  char io_serial[20];
  
  uint16_t io_buffer[65536];
  uint32_t io_offset;
  
  uint16_t sector_count;
  uint32_t lba;
  
  uint8_t device_control;
  uint8_t device_head;
  
  uint8_t status, error;
};

void pata_init(pata_t *drive, FILE *file);

void     pata_write(pata_t *drive, uint8_t offset, uint16_t data);
uint16_t pata_read(pata_t *drive, uint8_t offset);

#endif
