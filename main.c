// --- [ Standard C Libraries ] ---
#include <stdio.h>    // File operations (fopen, fread, etc.)
#include <stdlib.h>   // General utilities (exit, memory)
#include <stdint.h>   // Fixed-width integer types (uint8_t, uint16_t)
#include <string.h>   // String/memory functions
#include <stdbool.h>  // Boolean type support (true/false)

// --- [ Z80 Emulator Library ] ---
#include "z80.h" // External Z80 CPU emulation library

// --- [ SDL2 for Graphics and Sound ] ---
#define SDL_MAIN_HANDLED  // Prevent SDL from overriding main()
#include <SDL2/SDL.h>     // SDL2 library for window, rendering, audio

// --- [ Constants for the ZX Spectrum 48K System ] ---
#define ROM_SIZE           0x4000        // 16KB ROM size (16384 bytes)
#define SCREEN_W           256           // Screen width in pixels
#define SCREEN_H           192           // Screen height in pixels
#define CYCLES_PER_FRAME   (3500000/50)  // ZX Spectrum CPU is 3.5 MHz, 50 frames per second
#define SCALE              2             // Scale screen 2x (otherwise it's very small)

#define WIN_W              (SCREEN_W * SCALE)  // Window width in pixels
#define WIN_H              (SCREEN_H * SCALE)  // Window height in pixels

// --- [ Global Variables for Emulation State ] ---
static uint8_t memory[65536];  // Full 64 KB addressable memory
static uint8_t key_matrix[8];  // Keyboard matrix (8 rows for keys)

// Flash attribute (flashing colors in games, handled by the Spectrum video chip)
static int flash_counter = 0;   
static bool flash_state = false;   

// --- [ Beeper Sound Variables ] ---
static bool speaker_on = false;
static bool last_speaker_state = false;
static float speaker_freq = 440.0f;  // Frequency of beeper (440 Hz ~ musical A4 note)
static float phase = 0.0f;
static SDL_AudioDeviceID audio_dev;

// --- [ Audio Callback for Generating Beeper Sound ] ---
void audio_callback(void* userdata, Uint8* stream, int len) {
    Sint16* buf = (Sint16*)stream;   // Buffer for 16-bit audio samples
    int samples = len / 2;           // Number of samples (2 bytes per sample)

    for (int i = 0; i < samples; i++) {
        if (speaker_on) {
            // Output a simple square wave
            buf[i] = (Sint16)((phase < 0.5f) ? 1500 : -1500); // Toggle between +1500 and -1500
            phase += speaker_freq / 44100.0f; // Advance phase based on frequency
            if (phase >= 1.0f) phase -= 1.0f; // Wrap around after completing a cycle
        } else {
            buf[i] = 0; // Silence if speaker is OFF
        }
    }
}

// --- [ Keyboard Initialization (Set All Keys as Released) ] ---
static void init_keyboard(void) {
    for (int i = 0; i < 8; i++)
        key_matrix[i] = 0x1F;  // 5 active bits, all set to '1' = unpressed
}

// --- [ Update a Key's State in the Matrix ] ---
static void update_key(int row, int bit, bool pressed) {
    if (pressed)
        key_matrix[row] &= ~(1 << bit); // Clear bit to mark as pressed
    else
        key_matrix[row] |= (1 << bit);  // Set bit to mark as released
}

// --- [ Handle SDL Keyboard Events and Update Matrix ] ---
// This function translates PC keyboard events (from SDL) into
// the format expected by the ZX Spectrum's internal keyboard matrix.
// Each Spectrum key is represented by a specific (row, bit) combination.
static void handle_sdl_key(SDL_Scancode sc, bool pressed) {
    switch (sc) {
        // --- [ Mapping SDL keys to ZX Spectrum keys ] ---

        // [Shift and Ctrl] map to Spectrum CAPS SHIFT key (Row 7, Bit 1)
        case SDL_SCANCODE_RSHIFT:
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
            update_key(7, 1, pressed);
            break;

        // [Spacebar] maps to Spectrum SPACE key (Row 7, Bit 0)
        case SDL_SCANCODE_SPACE:
            update_key(7, 0, pressed);
            break;

        // [M key] maps to 'M' (Row 7, Bit 2)
        case SDL_SCANCODE_M:
            update_key(7, 2, pressed);
            break;

        // [N key] maps to 'N' (Row 7, Bit 3)
        case SDL_SCANCODE_N:
            update_key(7, 3, pressed);
            break;

        // [B key] maps to 'B' (Row 7, Bit 4)
        case SDL_SCANCODE_B:
            update_key(7, 4, pressed);
            break;

        // [Enter key] maps to Spectrum ENTER (Row 6, Bit 0)
        case SDL_SCANCODE_RETURN:
            update_key(6, 0, pressed);
            break;

        // [L key] maps to 'L' (Row 6, Bit 1)
        case SDL_SCANCODE_L:
            update_key(6, 1, pressed);
            break;

        // [K key] maps to 'K' (Row 6, Bit 2)
        case SDL_SCANCODE_K:
            update_key(6, 2, pressed);
            break;

        // [J key] maps to 'J' (Row 6, Bit 3)
        case SDL_SCANCODE_J:
            update_key(6, 3, pressed);
            break;

        // [H key] maps to 'H' (Row 6, Bit 4)
        case SDL_SCANCODE_H:
            update_key(6, 4, pressed);
            break;

        // --- [ Top rows: P, O, I, U, Y ] ---

        case SDL_SCANCODE_P:
            update_key(5, 0, pressed);
            break;
        case SDL_SCANCODE_O:
            update_key(5, 1, pressed);
            break;
        case SDL_SCANCODE_I:
            update_key(5, 2, pressed);
            break;
        case SDL_SCANCODE_U:
            update_key(5, 3, pressed);
            break;
        case SDL_SCANCODE_Y:
            update_key(5, 4, pressed);
            break;

        // --- [ Number keys (0-9) ] ---

        case SDL_SCANCODE_0:
            update_key(4, 0, pressed);
            break;
        case SDL_SCANCODE_9:
            update_key(4, 1, pressed);
            break;
        case SDL_SCANCODE_8:
            update_key(4, 2, pressed);
            break;
        case SDL_SCANCODE_7:
            update_key(4, 3, pressed);
            break;
        case SDL_SCANCODE_6:
            update_key(4, 4, pressed);
            break;

        // --- [ Number keys (1-5) ] ---

        case SDL_SCANCODE_1:
            update_key(3, 0, pressed);
            break;
        case SDL_SCANCODE_2:
            update_key(3, 1, pressed);
            break;
        case SDL_SCANCODE_3:
            update_key(3, 2, pressed);
            break;
        case SDL_SCANCODE_4:
            update_key(3, 3, pressed);
            break;
        case SDL_SCANCODE_5:
            update_key(3, 4, pressed);
            break;

        // --- [ Top alphabet keys (Q-W-E-R-T) ] ---

        case SDL_SCANCODE_Q:
            update_key(2, 0, pressed);
            break;
        case SDL_SCANCODE_W:
            update_key(2, 1, pressed);
            break;
        case SDL_SCANCODE_E:
            update_key(2, 2, pressed);
            break;
        case SDL_SCANCODE_R:
            update_key(2, 3, pressed);
            break;
        case SDL_SCANCODE_T:
            update_key(2, 4, pressed);
            break;

        // --- [ Middle alphabet keys (A-S-D-F-G) ] ---

        case SDL_SCANCODE_A:
            update_key(1, 0, pressed);
            break;
        case SDL_SCANCODE_S:
            update_key(1, 1, pressed);
            break;
        case SDL_SCANCODE_D:
            update_key(1, 2, pressed);
            break;
        case SDL_SCANCODE_F:
            update_key(1, 3, pressed);
            break;
        case SDL_SCANCODE_G:
            update_key(1, 4, pressed);
            break;

        // --- [ Bottom alphabet keys (Shift-Z-X-C-V) ] ---

        case SDL_SCANCODE_LSHIFT:
            update_key(0, 0, pressed);
            break;
        case SDL_SCANCODE_Z:
            update_key(0, 1, pressed);
            break;
        case SDL_SCANCODE_X:
            update_key(0, 2, pressed);
            break;
        case SDL_SCANCODE_C:
            update_key(0, 3, pressed);
            break;
        case SDL_SCANCODE_V:
            update_key(0, 4, pressed);
            break;

        // --- [ Default: ignore any other keys ] ---
        default:
            break;
    }
}


// --- [ Memory Read Function for CPU ] ---
static uint8_t read_byte(void* _, uint16_t addr) {
    return memory[addr];
}

// --- [ Memory Write Function for CPU ] ---
static void write_byte(void* _, uint16_t addr, uint8_t val) {
    if (addr >= ROM_SIZE) // Protect ROM area from writes
        memory[addr] = val;
}

// --- [ Port Input: Handle Keyboard Reading ] ---
static uint8_t port_in(z80* cpu, uint8_t port_lo) {
    if (port_lo & 1) return 0xFF;  // Only even ports are valid
    uint8_t sel = ~cpu->b;         // Selection mask from B register
    uint8_t res = 0xFF;            // Default: all keys unpressed
    for (int r = 0; r < 8; r++)
        if (sel & (1 << r)) res &= key_matrix[r]; // Merge rows
    return res | 0xE0; // Top bits are always high
}

// --- [ Port Output: Control Beeper ] ---
static void port_out(z80* cpu, uint8_t port_lo, uint8_t val) {
    if ((port_lo & 1) == 0) { // Only even ports are valid
        bool new_state = (val & 0x10) != 0;  // Bit 4 = speaker control
        if (new_state != last_speaker_state) {
            phase = 0.0f; // Reset audio phase on toggle
            last_speaker_state = new_state;
        }
        speaker_on = new_state;
    }
}

// --- [ Load ROM File into Memory ] ---
// This function loads the 16 KB ROM file into the beginning of our memory array.
// It also clears the rest of the memory (RAM) to zeros.
static void load_rom(const char* path) {
    // 1. Open the ROM file for reading in binary mode ("rb" = read binary)
    FILE* f = fopen(path, "rb");
    
    // 2. Check if the file opened successfully
    if (!f) {
        perror(path);  // Print the system error message with the filename
        exit(1);       // Exit the program immediately with error code 1
    }

    // 3. Read ROM_SIZE bytes (16 KB) from the file into memory starting at address 0
    if (fread(memory, 1, ROM_SIZE, f) != ROM_SIZE) {
        // If fread returns fewer bytes than expected, the ROM is invalid or corrupted
        fprintf(stderr, "Invalid ROM\n");  // Print a custom error message
        exit(1);  // Exit the program immediately
    }

    // 4. Close the ROM file (important to free system resources)
    fclose(f);

    // 5. Clear the rest of the memory (RAM area) by setting it to zero
    // memory + ROM_SIZE points right after the ROM area (address 0x4000)
    // 65536 - ROM_SIZE = size of the remaining memory (RAM)
    memset(memory + ROM_SIZE, 0, 65536 - ROM_SIZE);
    // Why clear RAM?
    // --> In the real ZX Spectrum, RAM starts blank or with random data.
    //     We initialize it to 0 for simplicity and to avoid unpredictable behavior.
}


// --- [ Main Program Entry Point ] ---
int main(int argc, char* argv[]) {
    load_rom("48.rom");   // Load the ZX Spectrum 48K ROM file into memory
    init_keyboard();      // Initialize keyboard matrix to all keys released

    // --- [ Initialize the CPU ] ---
    z80 cpu;
    z80_init(&cpu);       // Set all CPU registers to their default values
    cpu.read_byte = read_byte;   // Set memory read handler
    cpu.write_byte = write_byte; // Set memory write handler
    cpu.port_in = port_in;       // Set port input handler (keyboard)
    cpu.port_out = port_out;     // Set port output handler (beeper)
    cpu.userdata = NULL;         // No extra user data needed
    cpu.pc = 0;                  // Program counter starts at 0 (beginning of ROM)

    // --- [ Initialize SDL2 ] ---
    SDL_SetMainReady();  // SDL2 needs this before SDL_Init if SDL_MAIN_HANDLED
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    // --- [ Create SDL2 Window, Renderer, and Texture ] ---
    SDL_Window* win = SDL_CreateWindow(
        "ZX Spectrum 48K",            // Window title
        SDL_WINDOWPOS_CENTERED,       // Centered horizontally
        SDL_WINDOWPOS_CENTERED,       // Centered vertically
        WIN_W, WIN_H,                 // Window size (scaled)
        0                             // No special flags
    );
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* tex = SDL_CreateTexture(
        ren,
        SDL_PIXELFORMAT_ARGB8888,      // 32-bit pixels (Alpha-Red-Green-Blue)
        SDL_TEXTUREACCESS_STREAMING,   // We will update the pixels manually
        SCREEN_W, SCREEN_H             // Internal size (unscaled)
    );

    // --- [ Setup SDL2 Audio Device ] ---
    SDL_AudioSpec want = {0};      // Desired audio format
    want.freq = 44100;             // 44.1 kHz audio (standard)
    want.format = AUDIO_S16SYS;    // 16-bit signed audio samples
    want.channels = 1;             // Mono sound
    want.samples = 1024;           // Buffer size
    want.callback = audio_callback; // Function called to fill the audio buffer
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    SDL_PauseAudioDevice(audio_dev, 0);  // Start playing audio immediately

    // --- [ Prepare a Framebuffer for Drawing the Screen ] ---
    uint32_t framebuf[SCREEN_W * SCREEN_H];  // 32-bit ARGB framebuffer
    static const uint32_t palette[16] = {
        0xFF000000,0xFF0000D7,0xFFD70000,0xFFD700D7,
        0xFF00D700,0xFF00D7D7,0xFFD7D700,0xFFD7D7D7,
        0xFF000000,0xFF0000FF,0xFFFF0000,0xFFFF00FF,
        0xFF00FF00,0xFF00FFFF,0xFFFFFF00,0xFFFFFFFF
    };
    // First 8 entries = normal colors; next 8 = bright versions

    bool running = true;   // Main loop flag
    SDL_Event ev;          // SDL event variable

    while (running) {
        // --- [ Flash effect (for blinking colors) ] ---
        if (++flash_counter >= 16) { // Every 16 frames
            flash_counter = 0;
            flash_state = !flash_state;  // Toggle flash ON/OFF
        }

        // --- [ Handle SDL Events (Keyboard, Window Close) ] ---
        Uint32 t0 = SDL_GetTicks(); // Get current time in milliseconds
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT)
                running = false;   // Window closed
            else if (ev.type == SDL_KEYDOWN)
                handle_sdl_key(ev.key.keysym.scancode, true);   // Key pressed
            else if (ev.type == SDL_KEYUP)
                handle_sdl_key(ev.key.keysym.scancode, false);  // Key released
        }

        // --- [ Emulate CPU for one video frame (~70,000 cycles) ] ---
        unsigned long start = cpu.cyc;
        while (cpu.cyc - start < CYCLES_PER_FRAME)
            z80_step(&cpu);   // Step through CPU instructions

        z80_gen_int(&cpu, 0);  // Generate an interrupt after each frame (Spectrum design)

        // --- [ Video Rendering: Rebuild the Framebuffer ] ---
        for (int y = 0; y < SCREEN_H; y++) {  // For each line on the screen

            // --- [ Calculate Spectrum's weird screen memory address for this y ] ---
            int y0 = (y & 0xC0) << 5      // Top 2 bits of Y (bits 6–7) shifted to 11–12
                   | (y & 0x07) << 8       // Bottom 3 bits of Y (bits 0–2) shifted to 8–10
                   | (y & 0x38) << 2;      // Middle 3 bits of Y (bits 3–5) shifted to 5–7
            // ⚡ Explanation:
            // ZX Spectrum has a strange screen layout:
            //   - 0x4000..0x57FF stores the pixel data (bitmap)
            //   - 192 lines are divided into 3 zones (64 lines each)
            //   - Each 8-pixel block is stored non-linearly (this formula computes that)

            for (int x = 0; x < SCREEN_W; x++) {  // For each pixel in the line

                // --- [ Read the bitmap pixel bit ] ---
                uint8_t bit = memory[0x4000 + y0 + (x >> 3)] & (0x80 >> (x & 7));
                // Explanation:
                // - Each byte stores 8 pixels (1 bit per pixel)
                // - (x >> 3) = byte inside the line (x divided by 8)
                // - (0x80 >> (x & 7)) masks the right bit (from MSB to LSB)

                // --- [ Read the attribute byte (color info) ] ---
                uint8_t A = memory[0x5800 + (y/8) * 32 + (x/8)];
                // - 1 byte per 8x8 pixel block
                // - Attributes start at address 0x5800
                // - (y/8)*32 + (x/8) selects attribute block

                // --- [ Decode attribute: bright and flash bits ] ---
                bool br = A & 0x40; // Bit 6 = brightness flag (0 = normal, 1 = bright)
                bool fl = A & 0x80; // Bit 7 = flash flag (0 = normal, 1 = flash)

                // --- [ Extract INK (foreground) and PAPER (background) colors ] ---
                uint8_t ink = A & 0x07;          // Bits 0–2 = ink (foreground color)
                uint8_t pap = (A >> 3) & 0x07;    // Bits 3–5 = paper (background color)

                // --- [ Handle FLASH attribute (color swap) ] ---
                if (fl && flash_state) {
                    uint8_t t = ink;
                    ink = pap;
                    pap = t;
                }

                // --- [ Choose color: ink or paper depending on pixel bit ] ---
                uint8_t col = (bit ? ink : pap) + (br ? 8 : 0);
                // Add 8 if bright is enabled (palette has bright colors at index 8+)

                // --- [ Write color into framebuffer ] ---
                framebuf[y * SCREEN_W + x] = palette[col];
            }
        }

        // --- [ Update SDL2 Texture and Render Framebuffer ] ---
        SDL_UpdateTexture(tex, NULL, framebuf, SCREEN_W * sizeof(uint32_t));
        SDL_RenderClear(ren);            // Clear previous frame
        SDL_RenderCopy(ren, tex, NULL, NULL); // Copy updated texture
        SDL_RenderPresent(ren);           // Present on the screen

        // --- [ Frame Rate Control: Delay if frame finished too fast ] ---
        Uint32 dt = SDL_GetTicks() - t0;
        if (dt < 20) SDL_Delay(20 - dt);  // Target ~50 FPS (20ms per frame)
    }

    // --- [ Clean Up SDL2 Resources ] ---
    SDL_CloseAudioDevice(audio_dev);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit(); // Quit SDL2

    return 0;  // Program ends successfully
}
