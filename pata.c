#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pata.h>

void pata_init(pata_t *drive, FILE *file) {
  drive->io_file = file;
  
  if (file) {
    fseek(drive->io_file, 0, SEEK_END);
    drive->io_total_sectors = ftell(drive->io_file) >> 9;
    
    for (int i = 0; i < 10; i++) {
      drive->io_serial[i] = (rand() % 94) + 33;
    }
  } else {
    drive->io_total_sectors = 0;
  }
  
  drive->io_offset = UINT32_MAX;
  
  drive->sector_count = 1;
  drive->lba = 1;
  
  drive->device_control = 0x02;
  drive->device_head = 0;
  
  drive->status = 0x40;
  drive->error = 0x01;
}

void pata_write(pata_t *drive, uint8_t offset, uint16_t data) {
  // First all offsets with CS0 active, then all offsets with CS1 active:
  
  switch (offset & 15) {
    case 0:
      if (drive->io_offset >= 256 * drive->sector_count) {
        return;
      }
      
      drive->io_buffer[drive->io_offset++] = data;
      
      if (drive->io_offset == 256 * drive->sector_count) {
        fseek(drive->io_file, (size_t)(drive->lba) << 9, SEEK_SET);
        fwrite(drive->io_buffer, 1, (size_t)(drive->sector_count) << 9, drive->io_file);
        
        fflush(drive->io_file);
      }
      
      return;
    
    case 1:
      // TODO: Features
      return;
    
    case 2:
      drive->sector_count = data & 255;
      
      if (!drive->sector_count) {
        drive->sector_count = 256;
      }
      
      return;
    
    case 3:
      drive->lba = (drive->lba & 0b00001111111111111111111100000000) | ((data & 255) << 0);
      return;
    
    case 4:
      drive->lba = (drive->lba & 0b00001111111111110000000011111111) | ((data & 255) << 8);
      return;
    
    case 5:
      drive->lba = (drive->lba & 0b00001111000000001111111111111111) | ((data & 255) << 16);
      return;
    
    case 6:
      drive->lba = (drive->lba & 0b00000000111111111111111111111111) | ((data & 15) << 24);
      drive->device_head = (data & 255);
      return;
    
    case 7:
      switch (data & 255) {
        case 0x90:
          drive->sector_count = 1;
          drive->lba = 1;
          
          drive->status = 0x40;
          
          if (drive->io_file) {
            drive->error = 0x01;
          } else {
            drive->error = 0xFF;
          }
          
          drive->device_head = 0;
          return;
        
        case 0xEC:
          drive->io_buffer[60] = (drive->io_total_sectors >> 0) & 65535;
          drive->io_buffer[61] = (drive->io_total_sectors >> 16) & 4095;
          
          drive->io_buffer[100] = (drive->io_total_sectors >> 0) & 65535;
          drive->io_buffer[101] = (drive->io_total_sectors >> 16) & 4095;
          drive->io_buffer[102] = 0;
          
          drive->status = 0x08;
          drive->io_offset = 0;
          
          memcpy(drive->io_buffer + 10, drive->io_serial, 10);
          return;
        
        case 0x91:
          drive->status = 0x40;
          return;
        
        case 0x20: case 0x21:
          if (drive->lba + drive->sector_count > drive->io_total_sectors) {
            drive->status = 0x41;
            drive->error = 0x04;
            
            return;
          }
          
          fseek(drive->io_file, (size_t)(drive->lba) << 9, SEEK_SET);
          fread(drive->io_buffer, 1, (size_t)(drive->sector_count) << 9, drive->io_file);
          
          drive->status = 0x08;
          drive->io_offset = 0;
          
          return;
        
        case 0x30: case 0x31:
          if (drive->lba + drive->sector_count > drive->io_total_sectors) {
            drive->status = 0x41;
            drive->error = 0x04;
            
            return;
          }
          
          drive->status = 0x08;
          drive->io_offset = 0;
          
          return;
        
        default:
          return;
      }
    
    case 14:
      drive->device_control = (data & 255);
      return;
  }
}

uint16_t pata_read(pata_t *drive, uint8_t offset) {
  // First all offsets with CS0 active, then all offsets with CS1 active:
  
  switch (offset & 15) {
    case 0:
      if (drive->io_offset >= 256 * drive->sector_count) {
        return 0;
      } else if (drive->io_offset == 256 * drive->sector_count - 1) {
        drive->status = 0x40;
      }
      
      return drive->io_buffer[drive->io_offset++];
    
    case 1:
      return drive->error;
    
    case 2:
      return drive->sector_count;
    
    case 3:
      return (drive->lba >> 0) & 255;
    
    case 4:
      return (drive->lba >> 8) & 255;
    
    case 5:
      return (drive->lba >> 16) & 255;
    
    case 6:
      return drive->device_head;
    
    case 7: case 14:
      return drive->status;
    
    default:
      return 0;
  }
}
