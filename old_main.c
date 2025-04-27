// main.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "z80.h"
#include <SDL2/SDL.h>

#define ROM_SIZE        0x4000        // 16 KB de ROM
#define SCREEN_W        256
#define SCREEN_H        192
#define CYCLES_PER_FRAME (3500000/50) // ~3.5 MHz / 50 Hz ≈ 70 000 T-states

// 64 KB de memória: ROM + RAM
static uint8_t memory[65536];
// Cor da border (3 bits)
static uint8_t border_color = 0;

// Paleta Spectrum (0–7 normales, 8–15 bright)
static const uint32_t palette[16] = {
    0xFF000000, // 0: preto
    0xFF0000D7, // 1: azul
    0xFFD70000, // 2: vermelho
    0xFFD700D7, // 3: magenta
    0xFF00D700, // 4: verde
    0xFF00D7D7, // 5: ciano
    0xFFD7D700, // 6: amarelo
    0xFFD7D7D7, // 7: branco
    0xFF000000, // 8: preto bright (igual)
    0xFF0000FF, // 9: azul bright
    0xFFFF0000, // 10: vermelho bright
    0xFFFF00FF, // 11: magenta bright
    0xFF00FF00, // 12: verde bright
    0xFF00FFFF, // 13: ciano bright
    0xFFFFFF00, // 14: amarelo bright
    0xFFFFFFFF  // 15: branco bright
};

// Memória e I/O para o Z80
static uint8_t read_byte(void* u, uint16_t addr) {
    return memory[addr];
}
static void write_byte(void* u, uint16_t addr, uint8_t val) {
    if (addr >= ROM_SIZE)
        memory[addr] = val;
}
static uint8_t port_in(z80* cpu, uint8_t port) {
    // Sem teclado: devolve sempre 0xFF
    return 0xFF;
}
static void port_out(z80* cpu, uint8_t port, uint8_t val) {
    // Porta 0xFE (LSB = 0): bits 1–3 definem border
    if ((port & 1) == 0)
        border_color = (val >> 1) & 0x07;
}

// Carrega a ROM de 16 KB em memory[0..0x3FFF]
static void load_rom(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) { perror("load_rom"); exit(1); }
    if (fread(memory, 1, ROM_SIZE, f) != ROM_SIZE) {
        fprintf(stderr, "ROM incompleta\n");
        exit(1);
    }
    fclose(f);
    // RAM limpa a zero
    memset(memory + ROM_SIZE, 0, 65536 - ROM_SIZE);
}

int main(int argc, char** argv) {
    // 1) Carregar ROM
    load_rom("48.rom");

    // 2) Inicializar CPU Z80
    z80 cpu;
    z80_init(&cpu);
    cpu.read_byte  = read_byte;
    cpu.write_byte = write_byte;
    cpu.port_in    = port_in;
    cpu.port_out   = port_out;
    cpu.userdata   = NULL;
    cpu.pc         = 0x0000; // Início da ROM

    // 3) Inicializar SDL2
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window*   win = SDL_CreateWindow(
        "ZX Spectrum 48K",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W * 2, SCREEN_H * 2,
        SDL_WINDOW_SHOWN);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture*  tex = SDL_CreateTexture(
        ren, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_W, SCREEN_H);

    uint32_t framebuf[SCREEN_W * SCREEN_H];

    // 4) Loop principal
    bool running = true;
    SDL_Event ev;
    while (running) {
        // 4a) Eventos SDL
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
            }
        }

        // 4b) Emular um frame (≈70 000 T-states)
        unsigned long start = cpu.cyc;
        while (cpu.cyc - start < CYCLES_PER_FRAME) {
            z80_step(&cpu);
        }

        // 4c) Desenhar border (fundo)
        uint32_t bc = palette[border_color];
        for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
            framebuf[i] = bc;

        // 4d) Desenhar frame pixel a pixel com atributos
        for (int y = 0; y < SCREEN_H; y++) {
            // Cálculo rápido do offset de bitmap
            int y0 = (y & 0xC0) << 5  // linhas 0-63,64-127,128-191
                   | (y & 0x07) << 8  // dentro de um grupo de 8
                   | (y & 0x38) << 2; // grupos de 8
            for (int x = 0; x < SCREEN_W; x++) {
                // Endereço do byte de bitmap
                uint16_t vat = 0x4000 + y0 + (x >> 3);
                uint8_t bit = memory[vat] & (0x80 >> (x & 7));

                // Atributo correspondente (8×8 de células)
                uint16_t attr = 0x5800 + (y / 8) * 32 + (x / 8);
                uint8_t A = memory[attr];
                bool bright = (A & 0x40) != 0;
                uint8_t ink   =  A & 0x07;
                uint8_t paper = (A >> 3) & 0x07;
                uint8_t color = (bit ? ink : paper) + (bright ? 8 : 0);

                framebuf[y * SCREEN_W + x] = palette[color];
            }
        }

        // 4e) Atualizar textura e apresentar
        SDL_UpdateTexture(tex, NULL, framebuf, SCREEN_W * sizeof(uint32_t));
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }

    // 5) Cleanup
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
