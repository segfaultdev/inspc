  .cr scmp
  .tf test.bin, bin
  .or 0x0000

m_buffer      = 0x1000 ; MC6847 video buffer
m_cursor      = 0x1C00 ; Memory address to write the next character to
m_mode        = 0x1C02 ; Bit 0: Enable cursor
m_cursor_tick = 0x1C03 ; Cursor is inverted every 32 frames
m_stack       = 0x1000 ; Actually goes from 0x2000 downwards, but page alignment does funky stuff

; First PS/2 port (keyboard)
m_ps2_buffer_0     = 0x1D00 ; 256-byte circular buffer
m_ps2_read_head_0  = 0x1C04 ; Buffer read head
m_ps2_write_head_0 = 0x1C05 ; Buffer write head

; Second PS/2 port (mouse)
m_ps2_buffer_1     = 0x1E00 ; 256-byte circular buffer
m_ps2_read_head_1  = 0x1C06 ; Buffer read head
m_ps2_write_head_1 = 0x1C07 ; Buffer write head

; Upon interrupt:
; - Check Sense B -> If high, we're dealing with a WD65C51S interrupt
; - Otherwise, check PS/2 shift registers (they're cleared when read), as it could be a PS/2 interrupt
; - As a fallback, increment the tick counter, assuming it's an MC6847 interrupt (don't do otherwise, as it could desync)

  .db 0x00
start:
  ; Load the stack
  ldi /m_stack
  xpah p2
  ldi #m_stack
  xpal p2
  ; Initialize the screen
  js p1,f_init_screen
  ; Clear the screen
  ldi 0x00
  xae
  js p1,f_clear_screen
  ; Do text stuff
  ldi /test_string
  st @-1(p2)
  ldi #test_string
  st @-1(p2)
  js p1,f_print_string
  ld @2(p2)
end:
  ; Infinite loop (halt)
  jmp end

test_string:
  .db "INSPC R01, 57344 BYTES FREE", 0x0A, "?", 0x00


f_init_screen:
  ; Save P1 onto stack, and set to m_cursor
  ldi /m_cursor
  xpah p1
  st @-1(p2)
  ldi #m_cursor
  xpal p1
  st @-1(p2)
  ; Clear m_cursor (set to m_buffer)
  ldi /m_buffer
  st 1(p1)
  ldi #m_buffer
  st 0(p1)
  ; Clear m_mode and m_cursor_tick (set to 1)
  ldi 1
  st 2(p1)
  st 3(p1)
  ; Load the interrupt handler onto P3
  ldi /i_handle_p3
  xpah p3
  ldi #i_handle_p3
  xpal p3
  ; Setup text mode, and enable interrupts
  ldi 0b00001001
  cas
  ; Restore P1
  ld @1(p2)
  xpal p1
  ld @1(p2)
  xpah p1
  ret p1

  ; E -> Byte to fill screen with
f_clear_screen:
  ; Save P1 onto stack, and set to m_buffer
  ldi /m_buffer
  xpah p1
  st @-1(p2)
  ldi #m_buffer
  xpal p1
  st @-1(p2)
  ; Loop through all 512 bytes of text-mode buffer
  ldi 255
.loop:
  xae
  st @1(p1)
  st @1(p1)
  xae
  jz .end
  ccl
  adi -1
  jmp .loop
.end:
  ; Save memory by using f_init_screen to move back the cursor
  js p1,f_init_screen
  ; Restore P1
  ld @1(p2)
  xpal p1
  ld @1(p2)
  xpah p1
  ret p1

f_check_invert:
  ; TODO
  ret p1

f_check_scroll:
  ; Save E and P1 onto stack, load m_cursor
  xae
  st @-1(p2)
  ldi /m_cursor
  xpah p1
  st @-1(p2)
  ldi #m_cursor
  xpal p1
  st @-1(p2)
  ; Test if high byte is equal to 0x22, skip to end otherwise
  ld 1(p1)
  ccl
  adi -0x22
  jnz .end
  ; Set m_cursor to 0x21E0, 32 bytes before the end of the screen
  ldi 0x21
  st 1(p1)
  ldi 0xE0
  st 0(p1)
  ; Start scroll loop
  ldi /m_buffer
  xpah p1
  ldi #m_buffer
  xpal p1
  ldi 240
  xae
.loop_1:
  ld 32(p1)
  st @1(p1)
  ld 32(p1)
  st @1(p1)
  xae
  ccl
  adi -1
  xae
  lde
  jnz .loop_1
  ; Start clear loop
  ldi 32
  xae
.loop_2:
  ldi 0
  st @1(p1)
  xae
  ccl
  adi -1
  xae
  lde
  jnz .loop_2
.end:
  ; Restore P1 and E
  ld @1(p2)
  xpal p1
  ld @1(p2)
  xpah p1
  ld @1(p2)
  xae
  ret p1

  ; E -> Character to print
f_print_char:
  ; Save P1 onto stack
  xpah p1
  st @-1(p2)
  xpal p1
  st @-1(p2)
  ; Check if cursor is inverted
  js p1,f_check_invert
  ; Check if scrolling is needed
  js p1,f_check_scroll
  ; Load m_cursor to P1
  ldi /m_cursor
  xpah p1
  ldi #m_cursor
  xpal p1
  ; Jump to newline if character is 0x0A, decrease by 32 otherwise
  xae
  ccl
  adi -0x0A
  jz .newline
  ccl
  adi -0x16
  xae
  ; Save the pointer at m_cursor to the stack, increment it and save it back
  ld 1(p1)
  st @-1(p2)
  ld 0(p1)
  st @-1(p2)
  ccl
  adi 1
  st 0(p1)
  ld 1(p1)
  adi 0
  st 1(p1)
  ; Load the pointer to P1, and write the character
  ld @1(p2)
  xpal p1
  ld @1(p2)
  xpah p1
  xae
  st 0(p1)
  ; Skip to end of subroutine
  jmp .end
.newline:
  ld 0(p1)
  ccl
  ani 0b11100000
  adi 0b00100000
  st 0(p1)
  ld 1(p1)
  adi 0
  st 1(p1)
  ; Check if scrolling is needed
  js p1,f_check_scroll
.end:
  ; Restore P1
  ld @1(p2)
  xpal p1
  ld @1(p2)
  xpah p1
  ret p1

  ; 0(P2) -> String to print
f_print_string:
  ; Save P1 onto stack
  xpah p1
  st @-1(p2)
  xpal p1
  st @-1(p2)
  ; Push argument already in the stack again, not to override the original argument
  ld 3(p2)
  st @-1(p2)
  ld 3(p2)
  st @-1(p2)
.loop:
  ld 5(p2)
  xpah p1
  ld 4(p2)
  xpal p1
  ld @1(p1)
  jz .end
  xae
  xpah p1
  st 5(p2)
  xpal p1
  st 4(p2)
  js p1,f_print_char
  jmp .loop
.end:
  ; Decrement stack pointer
  ld @2(p2)
  ; Restore P1
  ld @1(p2)
  xpal p1
  ld @1(p2)
  xpah p1
  ret p1

i_handle_load:
  ; Interrupt handler shenanigans
  ien
  xppc p3
i_handle:
  ; Save all registers (with all I mean AC, E, SR and P1)
  st @-1(p2)
  lde
  st @-1(p2)
  csa
  st @-1(p2)
  xpah p1
  st @-1(p2)
  xpal p1
  st @-1(p2)
  ; Load m_cursor (and thus, m_mode and all that crap) onto P1
  ldi /m_cursor
  xpah p1
  ldi #m_cursor
  xpal p1
  ; Check m_mode, exit handler if cursor disabled
  ld 2(p1)
  ani 0b00000001
  jz .end
  ; Load m_cursor_tick and increment it
  ld 3(p1)
  ccl
  adi 1
  st 3(p1)
  ; If bottom 5 bits are clear, invert current character
  ani 0b00011111
  jnz .end
  ld 0(p1)
  xae
  ld 1(p1)
  xpah p1
  lde
  xpal p1
  ld 0(p1)
  xri 0b01000000
  st 0(p1)
.end:
  ; Restore all registers and exit the interrupt handler
  ld @1(p2)
  xpal p1
  ld @1(p2)
  xpah p1
  ld @1(p2)
  cas
  ld @1(p2)
  xae
  ld @1(p2)
  jmp i_handle_load

; Address to set P3 to (minus 1, as PC is incremented *before* fetching)
i_handle_p3 = i_handle-1
