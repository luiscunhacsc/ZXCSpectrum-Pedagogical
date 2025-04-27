#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "z80.h"

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#define ROM_SIZE           0x4000        // 16 KB ROM
#define SCREEN_W           256
#define SCREEN_H           192
#define CYCLES_PER_FRAME   (3500000/50)  // ≈3.5 MHz / 50 Hz
#define SCALE              2

#define WIN_W              (SCREEN_W * SCALE)
#define WIN_H              (SCREEN_H * SCALE)

static uint8_t memory[65536];
static uint8_t key_matrix[8];

static int flash_counter = 0;
static bool flash_state = false;

// --- Áudio (beep melhorado)
static bool speaker_on = false;
static bool last_speaker_state = false;
static float speaker_freq = 440.0f;  // Corrigido: frequência mais realista
static float phase = 0.0f;
static SDL_AudioDeviceID audio_dev;

void audio_callback(void* userdata, Uint8* stream, int len) {
    Sint16* buf = (Sint16*)stream;
    int samples = len / 2;
    for (int i = 0; i < samples; i++) {
        if (speaker_on) {
            buf[i] = (Sint16)((phase < 0.5f) ? 1500 : -1500); // Amplitude mais baixa
            phase += speaker_freq / 44100.0f;
            if (phase >= 1.0f) phase -= 1.0f;
        } else {
            buf[i] = 0;
        }
    }
}
// --- Fim áudio

static void init_keyboard(void) {
    for (int i = 0; i < 8; i++)
        key_matrix[i] = 0x1F;
}

static void update_key(int row, int bit, bool pressed) {
    if (pressed)
        key_matrix[row] &= ~(1 << bit);
    else
        key_matrix[row] |= (1 << bit);
}

static void handle_sdl_key(SDL_Scancode sc, bool pressed) {
    switch (sc) {
      case SDL_SCANCODE_RSHIFT:
      case SDL_SCANCODE_LCTRL:
      case SDL_SCANCODE_RCTRL:
        update_key(7,1,pressed); break;
      case SDL_SCANCODE_SPACE:  update_key(7,0,pressed); break;
      case SDL_SCANCODE_M:      update_key(7,2,pressed); break;
      case SDL_SCANCODE_N:      update_key(7,3,pressed); break;
      case SDL_SCANCODE_B:      update_key(7,4,pressed); break;
      case SDL_SCANCODE_RETURN: update_key(6,0,pressed); break;
      case SDL_SCANCODE_L:      update_key(6,1,pressed); break;
      case SDL_SCANCODE_K:      update_key(6,2,pressed); break;
      case SDL_SCANCODE_J:      update_key(6,3,pressed); break;
      case SDL_SCANCODE_H:      update_key(6,4,pressed); break;
      case SDL_SCANCODE_P: update_key(5,0,pressed); break;
      case SDL_SCANCODE_O: update_key(5,1,pressed); break;
      case SDL_SCANCODE_I: update_key(5,2,pressed); break;
      case SDL_SCANCODE_U: update_key(5,3,pressed); break;
      case SDL_SCANCODE_Y: update_key(5,4,pressed); break;
      case SDL_SCANCODE_0: update_key(4,0,pressed); break;
      case SDL_SCANCODE_9: update_key(4,1,pressed); break;
      case SDL_SCANCODE_8: update_key(4,2,pressed); break;
      case SDL_SCANCODE_7: update_key(4,3,pressed); break;
      case SDL_SCANCODE_6: update_key(4,4,pressed); break;
      case SDL_SCANCODE_1: update_key(3,0,pressed); break;
      case SDL_SCANCODE_2: update_key(3,1,pressed); break;
      case SDL_SCANCODE_3: update_key(3,2,pressed); break;
      case SDL_SCANCODE_4: update_key(3,3,pressed); break;
      case SDL_SCANCODE_5: update_key(3,4,pressed); break;
      case SDL_SCANCODE_Q: update_key(2,0,pressed); break;
      case SDL_SCANCODE_W: update_key(2,1,pressed); break;
      case SDL_SCANCODE_E: update_key(2,2,pressed); break;
      case SDL_SCANCODE_R: update_key(2,3,pressed); break;
      case SDL_SCANCODE_T: update_key(2,4,pressed); break;
      case SDL_SCANCODE_A: update_key(1,0,pressed); break;
      case SDL_SCANCODE_S: update_key(1,1,pressed); break;
      case SDL_SCANCODE_D: update_key(1,2,pressed); break;
      case SDL_SCANCODE_F: update_key(1,3,pressed); break;
      case SDL_SCANCODE_G: update_key(1,4,pressed); break;
      case SDL_SCANCODE_LSHIFT: update_key(0,0,pressed); break;
      case SDL_SCANCODE_Z:      update_key(0,1,pressed); break;
      case SDL_SCANCODE_X:      update_key(0,2,pressed); break;
      case SDL_SCANCODE_C:      update_key(0,3,pressed); break;
      case SDL_SCANCODE_V:      update_key(0,4,pressed); break;
      default: break;
    }
}

static uint8_t read_byte(void* _, uint16_t addr) { return memory[addr]; }
static void write_byte(void* _, uint16_t addr, uint8_t val) {
    if (addr >= ROM_SIZE) memory[addr] = val;
}
static uint8_t port_in(z80* cpu, uint8_t port_lo) {
    if (port_lo & 1) return 0xFF;
    uint8_t sel = ~cpu->b, res = 0xFF;
    for (int r=0; r<8; r++) if (sel & (1<<r)) res &= key_matrix[r];
    return res | 0xE0;
}
static void port_out(z80* cpu, uint8_t port_lo, uint8_t val) {
    if ((port_lo & 1) == 0) {
        bool new_state = (val & 0x10) != 0;
        if (new_state != last_speaker_state) {
            phase = 0.0f; // Resetar fase no momento da mudança
            last_speaker_state = new_state;
        }
        speaker_on = new_state;
    }
}

static void load_rom(const char* path) {
    FILE* f = fopen(path,"rb"); if (!f){perror(path);exit(1);}
    if (fread(memory,1,ROM_SIZE,f)!=ROM_SIZE){fprintf(stderr,"ROM inválida\n");exit(1);}
    fclose(f);
    memset(memory+ROM_SIZE,0,65536-ROM_SIZE);
}

int main(int argc, char* argv[]) {
    load_rom("48.rom");
    init_keyboard();

    z80 cpu; z80_init(&cpu);
    cpu.read_byte=read_byte; cpu.write_byte=write_byte;
    cpu.port_in=port_in; cpu.port_out=port_out;
    cpu.userdata=NULL; cpu.pc=0;

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)!=0){fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());return 1;}

    SDL_Window* win = SDL_CreateWindow("ZX Spectrum 48K",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,WIN_W,WIN_H,0);
    SDL_Renderer* ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED);
    SDL_Texture* tex=SDL_CreateTexture(ren,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,SCREEN_W,SCREEN_H);

    // Áudio
    SDL_AudioSpec want = {0};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = audio_callback;
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    SDL_PauseAudioDevice(audio_dev, 0);

    uint32_t framebuf[SCREEN_W*SCREEN_H];
    static const uint32_t palette[16]={
        0xFF000000,0xFF0000D7,0xFFD70000,0xFFD700D7,
        0xFF00D700,0xFF00D7D7,0xFFD7D700,0xFFD7D7D7,
        0xFF000000,0xFF0000FF,0xFFFF0000,0xFFFF00FF,
        0xFF00FF00,0xFF00FFFF,0xFFFFFF00,0xFFFFFFFF
    };

    bool running=true; SDL_Event ev;
    while(running) {
        if(++flash_counter>=16){flash_counter=0;flash_state=!flash_state;}

        Uint32 t0=SDL_GetTicks();
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT)running=false;
            else if(ev.type==SDL_KEYDOWN) handle_sdl_key(ev.key.keysym.scancode, true);
            else if(ev.type==SDL_KEYUP)   handle_sdl_key(ev.key.keysym.scancode, false);
        }
        unsigned long start=cpu.cyc;
        while(cpu.cyc-start<CYCLES_PER_FRAME) z80_step(&cpu);
        z80_gen_int(&cpu,0);

        for(int y=0;y<SCREEN_H;y++){
            int y0=(y&0xC0)<<5|(y&0x07)<<8|(y&0x38)<<2;
            for(int x=0;x<SCREEN_W;x++){
                uint8_t bit=memory[0x4000+y0+(x>>3)]&(0x80>>(x&7));
                uint8_t A=memory[0x5800+(y/8)*32+(x/8)];
                bool br=A&0x40, fl=A&0x80;
                uint8_t ink=A&0x07, pap=(A>>3)&0x07;
                if(fl&&flash_state){uint8_t t=ink;ink=pap;pap=t;}
                uint8_t col=(bit?ink:pap)+(br?8:0);
                framebuf[y*SCREEN_W+x]=palette[col];
            }
        }

        SDL_UpdateTexture(tex,NULL,framebuf,SCREEN_W*sizeof(uint32_t));
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren,tex,NULL,NULL);
        SDL_RenderPresent(ren);

        Uint32 dt=SDL_GetTicks()-t0; if(dt<20)SDL_Delay(20-dt);
    }

    SDL_CloseAudioDevice(audio_dev);
    SDL_DestroyTexture(tex); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
