#include "sys.h"

#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdint.h>
#include "mem.h"
#include "cpu.h"

uint8_t sys_carttype;
uint8_t sys_mbc1_s;
uint8_t sys_romsize;
uint8_t sys_extmem_en;
uint8_t sys_rombank;
uint8_t sys_rambank;

int16_t sys_div_cycles;

int16_t sys_timer_cycles;
int16_t sys_timer_interval;

uint8_t sys_buttons_all;

uint16_t sys_dma_source;
uint8_t sys_dma_counter;
uint8_t sys_dma_busy;

int16_t sys_timer_interval_list[4] = {
    SYS_TIMER_CYCLES_4096HZ,
    SYS_TIMER_CYCLES_262144HZ,
    SYS_TIMER_CYCLES_65536HZ,
    SYS_TIMER_CYCLES_16384HZ,
};

void sys_init() {

    sys_carttype = CT_ROMONLY;
    sys_mbc1_s = MBC1_2048_8;
    sys_romsize = 0;
    sys_extmem_en = 0;
    sys_rombank = 0;
    sys_rambank = 0;

    sys_div_cycles = SYS_DIV_INTERVAL;
    sys_timer_cycles = SYS_TIMER_CYCLES_4096HZ;

    sys_timer_interval = SYS_TIMER_CYCLES_4096HZ;

    sys_buttons_all = 0;

    sys_dma_source = 0;
    sys_dma_counter = 0;
    sys_dma_busy = 0;

}

void sys_dma_cycles(int cycles) {

    // Process dma cycles, one byte per cycle
    if (sys_dma_busy) {

#ifdef SYS_VERBOSE
        printf(">>>> DMA COPYING %x BYTES FROM %x\n", cycles, sys_dma_source + sys_dma_counter);
#endif
        for (int i = 0; i < cycles; i++) {
            oam[sys_dma_counter] = cpu_read8_force(sys_dma_source + sys_dma_counter);
            if (++sys_dma_counter == SYS_DMA_LENGTH)
                break;
        }

        if (sys_dma_counter == SYS_DMA_LENGTH) {
            // DMA has ended
            sys_dma_source = 0;
            sys_dma_counter = 0;
            sys_dma_busy = 0;
        }
    }

}

void sys_cycles(int cycles) {

    // Handle DIV counter which is always active

    sys_div_cycles -= cycles;

    if (sys_div_cycles <= 0) {
        // Increment Divider register
        SYS_DIV++;
        sys_div_cycles = SYS_DIV_INTERVAL + sys_div_cycles;
    }

    if (SYS_TIMER_CFG & SYS_TIMER_ENABLED)  {

        // Do timer shenanigans

        sys_timer_cycles -= cycles;

#ifdef SYS_VERBOSE
        printf("sys_timer_interval = %i, sys_timer_cycles = %i, sys_timer = %i, sys_timer_mod = %i\n", sys_timer_interval, sys_timer_cycles, SYS_TIMER, SYS_TIMER_MOD);
#endif

        if (sys_timer_cycles <= 0) {
            // Increment timer when the amount of cycles per "tick" have been reached

            sys_timer_cycles = sys_timer_interval + sys_timer_cycles; // Reset amount of cycles

            SYS_TIMER++;

            if (SYS_TIMER == 0) {
                // Timer overflowed
                SYS_TIMER = SYS_TIMER_MOD;
                sys_interrupt_req(INT_TIMER);
            }

        }


    }

}

uint8_t sys_read_joypad() {

    uint8_t result = 0x0F;

    // Handle DPAD if enabled
    if (SYS_JOYPAD & JOY_DPAD) {
        result &= (sys_buttons_all >> 4);
    }

    // Handle buttons if enabled
    if (SYS_JOYPAD & JOY_BUTTONS) {
        result &= (sys_buttons_all & 0x0F);
    }

#ifdef SYS_VERBOSE
    printf("Joypad status %02x, joy_int %02x, mem_ie %02x, buttons %02x\n", result, SYS_IF & INT_JOYPAD, ram_ie, sys_buttons_all);
#endif

    return result;
}

void sys_handle_joypads() {

    const uint8_t *state = SDL_GetKeyboardState(NULL);

    // 0xFF00
    /*
    Bit 7 - Not used
    Bit 6 - Not used
    Bit 5 - P15 Select Button Keys      (0=Select)
    Bit 4 - P14 Select Direction Keys   (0=Select)
    Bit 3 - P13 Input Down  or Start    (0=Pressed) (Read Only)
    Bit 2 - P12 Input Up    or Select   (0=Pressed) (Read Only)
    Bit 1 - P11 Input Left  or Button B (0=Pressed) (Read Only)
    Bit 0 - P10 Input Right or Button A (0=Pressed) (Read Only)
    */

    uint8_t sys_buttons_old = sys_buttons_all; // old joypad state for later

    sys_buttons_all = 0
            | ((~state[SDL_SCANCODE_DOWN] & 0x01)    << 7)
            | ((~state[SDL_SCANCODE_UP] & 0x01)      << 6)
            | ((~state[SDL_SCANCODE_LEFT] & 0x01)    << 5)
            | ((~state[SDL_SCANCODE_RIGHT] & 0x01)   << 4)
            | ((~state[SDL_SCANCODE_RETURN] & 0x01)  << 3)
            | ((~state[SDL_SCANCODE_SPACE] & 0x01)   << 2)
            | ((~state[SDL_SCANCODE_S] & 0x01)       << 1)
            | ((~state[SDL_SCANCODE_A] & 0x01)       << 0)
            ;

    // Interrupt on high-low transitions

    for (int i = 0; i < 8; i++) {
        if (((sys_buttons_old >> i) & 0x01) && !(((sys_buttons_all >> i) & 0x01))) {
            // Req a joypad interupt
            sys_interrupt_req(INT_JOYPAD);
            break;
        }
    }

}


void sys_interrupt_req(uint8_t index) {
    // Requests an interrupt
    SYS_IF |= index;
}

void sys_interrupt_clear(uint8_t index) {
    // Clears an interrupt
    SYS_IF &= ~index;
}
