#include <SDL2/SDL.h>
#include <palette.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <font.h>
#include <pata.h>
#include <scmp.h>
#include <time.h>

#define SCALE_X 4
#define SCALE_Y 3

#define MHZ 3.579545f

uint8_t ram[65536]; // Also includes VRAM, the ROM and all the I/O space (though the latter is unused, of course :p).

pata_t pata_drives[3];

/*
mc6847_t video_device;
sn76489_t sound_device;
mos6551_t uart_device;

ps2_t ps2_ports[2];
*/

// Memory map:
// 0x0000 -> 0x0EFF: INSPC BIOS
// 0x0F00 -> 0x0FFF: I/O bus
// 0x1000 -> 0x1BFF: Video RAM (for MC6847)
// 0x1C00 -> 0xFFFF: 57 KiB RAM free (first kibibyte is actually used by the BIOS)

// I/O bus:
// - Up to 16 devices, each with 16 bytes of addressable space

// I/O map:
// 0x00 - 0x0F: IDE/PATA drive 0
// 0x10 - 0x1F: IDE/PATA drive 1
// 0x20 - 0x2F: IDE/PATA drive 2
// 0x30 - 0x3F: IDE/PATA drive 3
// 0x40 - 0x4F: PS/2 FIFO 0 (repeated 16x)
// 0x50 - 0x5F: PS/2 FIFO 1 (repeated 16x)
// 0x60 - 0x6F: SN76489 (repeated 16x)
// 0x70 - 0xFF: Unused (was going to add a WD65C51S, but making an interrupt system sounds like too much work)

void mem_write(scmp_t *scmp, uint16_t offset, uint8_t data) {
  if (offset < 0x0F00) {
    // ROM is read-only
    return;
  } else if (offset >= 0x0F00 && offset < 0x1000) {
    // TODO: I/O
    return;
  }
  
  ram[offset] = data;
}

uint8_t mem_read(scmp_t *scmp, uint16_t offset) {
  if (offset >= 0x0F00 && offset < 0x1000) {
    
  }
  
  return ram[offset];
}

void start_inspc(void) {
  uint32_t colors[12];
  
  for (int i = 0; i < 12; i++) {
    palette_to_rgb(palettes[0], 1, i, colors + i);
  }
  
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  
  SDL_Window *window = SDL_CreateWindow("inspc VM r01", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 256 * SCALE_X, 204 * SCALE_Y, SDL_WINDOW_SHOWN);
  SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 204);
  
  scmp_t scmp;
  
  scmp.f_write = mem_write;
  scmp.f_read = mem_read;
  
  scmp.cycles_left = 0;
  
  scmp.pc = 0;
  scmp.sr = 0;
  
  for (int i = 0; i < 65536; i++) {
    ram[i] = rand() & 0xFF;
  }
  
  FILE *rom = fopen("test.bin", "rb");
  
  fseek(rom, 0, SEEK_END);
  size_t size = ftell(rom);
  fseek(rom, 0, SEEK_SET);
  
  fread(ram, 1, size, rom);
  
  fclose(rom);
  
  struct timeval frame_tv;
  gettimeofday(&frame_tv, NULL);
  
  struct timeval last_tv;
  gettimeofday(&last_tv, NULL);
  
  int cycles_per_frame = 1000;
  int do_quit = 0;
  
  while (!do_quit) {
    // cycles_per_frame = 1;
    
    for (int i = 0; i < cycles_per_frame; i++) {
      if (!(scmp.cycles % 14915)) {
        scmp.sr |= 0x10;
      }
      
      scmp_tick(&scmp);
    }
    
    struct timeval curr_tv;
    gettimeofday(&curr_tv, NULL);
    
    size_t curr_us = 1000000ull * curr_tv.tv_sec + curr_tv.tv_usec;
    size_t last_us = 1000000ull * last_tv.tv_sec + last_tv.tv_usec;
    size_t frame_us = 1000000ull * frame_tv.tv_sec + frame_tv.tv_usec;
    
    float mhz = (4.0f * cycles_per_frame) / (curr_us - last_us);
    
    if (rand() % 100 == 0) {
      printf("%.2f FPS, %.2f MHz\n", 1000000.0f / (curr_us - last_us), mhz);
    }
    
    if (curr_us - frame_us >= 16667ull) {
      frame_us += 16667ull;
    }
    
    if (mhz > MHZ) {
      cycles_per_frame--;
    } else if (mhz < MHZ) {
      cycles_per_frame++;
    }
    
    last_tv = curr_tv;
    SDL_Event ev;
    
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) {
        do_quit = 1;
        break;
      }
    }
    
    const char *hex_chars = "0123456789ABCDEF";
    uint8_t debug_text[32] = {' '};
    
    debug_text[0] = ('A' - 32) | 0x40;
    debug_text[1] = ('C' - 32) | 0x40;
    debug_text[2] = hex_chars[(scmp.ac >> 4) & 15] - 32;
    debug_text[3] = hex_chars[(scmp.ac >> 0) & 15] - 32;
    debug_text[5] = ('E' - 32) | 0x40;
    debug_text[6] = hex_chars[(scmp.e >> 4) & 15] - 32;
    debug_text[7] = hex_chars[(scmp.e >> 0) & 15] - 32;
    debug_text[9] = ('0' - 32) | 0x40;
    debug_text[10] = hex_chars[(scmp.pc >> 12) & 15] - 32;
    debug_text[11] = hex_chars[(scmp.pc >> 8) & 15] - 32;
    debug_text[12] = hex_chars[(scmp.pc >> 4) & 15] - 32;
    debug_text[13] = hex_chars[(scmp.pc >> 0) & 15] - 32;
    debug_text[15] = ('1' - 32) | 0x40;
    debug_text[16] = hex_chars[(scmp.p1 >> 12) & 15] - 32;
    debug_text[17] = hex_chars[(scmp.p1 >> 8) & 15] - 32;
    debug_text[18] = hex_chars[(scmp.p1 >> 4) & 15] - 32;
    debug_text[19] = hex_chars[(scmp.p1 >> 0) & 15] - 32;
    debug_text[21] = ('2' - 32) | 0x40;
    debug_text[22] = hex_chars[(scmp.p2 >> 12) & 15] - 32;
    debug_text[23] = hex_chars[(scmp.p2 >> 8) & 15] - 32;
    debug_text[24] = hex_chars[(scmp.p2 >> 4) & 15] - 32;
    debug_text[25] = hex_chars[(scmp.p2 >> 0) & 15] - 32;
    debug_text[27] = ('3' - 32) | 0x40;
    debug_text[28] = hex_chars[(scmp.p3 >> 12) & 15] - 32;
    debug_text[29] = hex_chars[(scmp.p3 >> 8) & 15] - 32;
    debug_text[30] = hex_chars[(scmp.p3 >> 4) & 15] - 32;
    debug_text[31] = hex_chars[(scmp.p3 >> 0) & 15] - 32;
    
    uint32_t pixels[256 * 204];
    
    int f0 = !!(scmp.sr & 1);
    int f1 = !!(scmp.sr & 2);
    
    for (int i = 0; i < 204; i++) {
      if (i >= 192) {
        f0 = 2;
        f1 = 0;
      }
      
      if (f1) {
        for (int j = 0; j < 16; j++) {
          uint8_t mask = ram[0x1000 + j + i * 16];
          
          for (int k = 0; k < 8; k++) {
            uint32_t color;
            
            if (mask & (1 << k)) {
              color = colors[f0 ? 4 : 0];
            } else {
              color = colors[8];
            }
            
            pixels[0 + (7 - k) * 2 + j * 16 + i * 256] = color;
            pixels[1 + (7 - k) * 2 + j * 16 + i * 256] = color;
          }
        }
      } else {
        for (int j = 0; j < 256; j++) {
          uint8_t value = ram[0x1000 + (j / 8) + (i / 12) * 32];
          
          if (f0 == 2) {
            value = debug_text[j / 8];
          }
          
          int x = j & 7;
          int y = i % 12;
          
          if (value & 0x80) {
            int index = (x / 4) + (y / 6) * 2;
            pixels[j + i * 256] = colors[(value & (1 << (3 - index))) ? ((value >> 4) & 7) : 8];
          } else {
            int bit = 0;
            
            if (y >= 2 && y < 10) {
              bit = ((font[(y - 2) + ((value & 63) << 3)] >> x) & 1);
            }
            
            if (value & 0x40) {
              bit = !bit;
            }
            
            uint32_t color;
            
            if (f0 == 0) {
              color = colors[bit ? 0 : 9];
            } else if (f0 == 1) {
              color = colors[bit ? 11 : 10];
            } else if (f0 == 2) {
              color = colors[bit ? 4 : 8];
            }
            
            pixels[j + i * 256] = color;
          }
        }
      }
    }
    
    SDL_UpdateTexture(texture, NULL, pixels, 256 * 4);
    
    SDL_Rect dest_rect = {0, 0, 256 * SCALE_X, 204 * SCALE_Y};
    SDL_RenderCopy(renderer, texture, NULL, &dest_rect);
    
    SDL_RenderPresent(renderer);
  }
  
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  
  SDL_Quit();
}

int main(int argc, const char **argv) {
  srand(time(NULL));
  start_inspc();
  
  return 0;
}
