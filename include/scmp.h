#ifndef __SCMP_H__
#define __SCMP_H__

#include <stdint.h>

typedef struct scmp_t scmp_t;

struct scmp_t {
  // Note: In official documentation, p0 is called pc; p1,
  //       p2 and p3 were never given names and ex is
  //       referred as just e.
  
  union {
    uint16_t px[4];
    
    struct {
      uint16_t pc, p1, p2, p3;
    };
  };
  
  uint8_t ac, e, sr;
  
  uint8_t s_in: 1;
  uint8_t s_out: 1;
  
  uint_least32_t cycles;
  uint_least32_t cycles_left;
  
  void *f_data;
  
  void    (*f_write)(scmp_t *scmp, uint16_t address, uint8_t value);
  uint8_t (*f_read)(scmp_t *scmp, uint16_t address);
};

void scmp_tick(scmp_t *scmp);

#endif
