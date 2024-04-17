#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "timer.h"

// Using this guide by Tobias V. Langhoff as a feature reference
// https://tobiasvl.github.io/blog/write-a-chip-8-emulator/

const unsigned int LOGICAL_WIDTH = 64;
const unsigned int LOGICAL_HEIGHT = 32;

const unsigned int SCREEN_WIDTH = 10*LOGICAL_WIDTH;
const unsigned int SCREEN_HEIGHT = 10*LOGICAL_HEIGHT;

const unsigned int STACK_SIZE = 48;
const unsigned int MEMORY_SIZE = 4096;

const unsigned int FONT_OFFSET = 50;
const unsigned int LOAD_OFFSET = 512;

SDL_Renderer* renderer = NULL;
SDL_Texture* texture = NULL;

const uint32_t WHITE = 0xFFFFFFFF;
const uint32_t BLACK = 0x000000FF;

// 64x32 display
uint32_t display[LOGICAL_WIDTH*LOGICAL_HEIGHT];

struct {
    uint16_t PC;
    uint16_t I;
    uint8_t V[16];
} reg;

uint8_t memory[MEMORY_SIZE];

timer_60hz_t delay_timer = { .counter = 0x0 };
timer_60hz_t sound_timer = { .counter = 0x0 };

struct {
    int top;
    uint16_t addr[STACK_SIZE]; // todo: review stack size
} stack = { .top = 0 };

void push_address(const uint16_t addr) {
    if (stack.top == STACK_SIZE) {
        printf("Stack overflow\n");
        exit(EXIT_FAILURE);
    }
    stack.addr[stack.top++] = addr;
}

uint16_t pop_address() {
    if (stack.top == 0) {
        printf("Stack underflow\n");
        exit(EXIT_FAILURE);
    }
    return stack.addr[--stack.top];
}

uint16_t fetch() {
    // CHIP-8 is big endian
    uint8_t msb = memory[reg.PC++];
    uint8_t lsb = memory[reg.PC++];
    return msb << 8 | lsb;
}

void clear_display() {
    (void) SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Set color to black
    (void) SDL_RenderClear(renderer);
    (void) SDL_RenderPresent(renderer);
    (void) memset(display, 0, sizeof(display));
}

void draw(uint8_t X, uint8_t Y, uint8_t N) {
    X %= LOGICAL_WIDTH;
    Y %= LOGICAL_HEIGHT;

    reg.V[0xF] = 0U;

    for (int n = 0; (n < N) && (Y+n < LOGICAL_HEIGHT); n++) {
        const uint8_t sprite = memory[reg.I + n];

        for (uint8_t b = 0; (b < 8) && (X+b < LOGICAL_WIDTH); b++) {
            const uint32_t sprite_pixel = sprite & (1U << (7U-b)) ? WHITE : BLACK;

            // If both pixels are on, set VF to 1
            if ( (sprite_pixel == WHITE) && (display[(Y+n)*LOGICAL_WIDTH + (X+b)] == WHITE) ) {
                reg.V[0xF] = 1U;
            }

            display[(Y+n)*LOGICAL_WIDTH + (X+b)] = sprite_pixel;
        }
    }

    SDL_UpdateTexture(texture, NULL, display, LOGICAL_WIDTH * sizeof(uint32_t));
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

// Decode implementation based on CHIP-8 opcode table from Wikipedia
// https://en.wikipedia.org/wiki/CHIP-8#Opcode_table
void decode_and_execute(const uint16_t opcode) {
    const uint8_t  MSN = (opcode & 0xF000) >> 12;
    const uint8_t  X   = (opcode & 0x0F00) >> 8;
    const uint8_t  Y   = (opcode & 0x00F0) >> 4;
    const uint8_t  N   = (opcode & 0x000F);
    const uint8_t  NN  = (opcode & 0x00FF);
    const uint16_t NNN = (opcode & 0x0FFF);

    switch (MSN) {
    case 0x0:
        switch (NN) {
        case 0xE0: // Clear display
            clear_display();
            break;
        case 0xEE: // Return from subroutine
            reg.PC = pop_address();
            break;
        default:
            goto bad_opcode;
        }
        break;
    case 0x1: // Jump to NNN
        reg.PC = NNN;
        break;
    case 0x2: // Call subroutine at NNN
        push_address(reg.PC);
        reg.PC = NNN;
        break;
    case 0x3: // Skip next instr if (Vx == NN)
        if (reg.V[X] == NN) {
            reg.PC += sizeof(opcode);
        }
        break;
    case 0x4: // Skip next instr if (Vx != NN)
        if (reg.V[X] != NN) {
            reg.PC += sizeof(opcode);
        }
        break;
    case 0x5: // Skip next instr if (Vx == Vy)
        if (reg.V[X] != reg.V[Y]) {
            reg.PC += sizeof(opcode);
        }
        break;
    case 0x6: // Set Vx = NN
        reg.V[X] = NN;
        break;
    case 0x7: // Set V[X] += NN
        reg.V[X] += NN;
        break;
    case 0x8:
        switch (N) {
        case 0x0: // Set Vx = Vy
            reg.V[X] = reg.V[Y];
            break;
        case 0x1: // Set Vx |= Vy
            reg.V[X] |= reg.V[Y];
            break;
        case 0x2: // Set Vx &= Vy
            reg.V[X] &= reg.V[Y];
            break;
        case 0x3: // Set Vx ^= Vy
            reg.V[X] ^= reg.V[Y];
            break;
        case 0x4: // Set Vx += Vy
            reg.V[X] += reg.V[Y];
            break;
        case 0x5: // Set Vx -= Vy
            reg.V[X] -= reg.V[Y];
            break;
        case 0x6: // Set V[X] >>= 1
            reg.V[X] >>= 1U;
            break;
        case 0x7: // Set Vx = Vy - Vx
            reg.V[X] = reg.V[Y] - reg.V[X];
            break;
        case 0xE: // Set Vx <<= 1
            reg.V[X] <<= 1U;
            break;
        default:
            goto bad_opcode;
        }
        break;
    case 0x9: // Skip next instr if (Vx != Vy)
        if (reg.V[X] != reg.V[Y]) {
            reg.PC += sizeof(opcode);
        }
        break;
    case 0xA: // Set I = NNN
        reg.I = NNN;
        break;
    case 0xB: // Set PC = V0 + NNN
        reg.PC = reg.V[0x0] + NNN;
        break;
    case 0xC: // Set Vx = rand() & NN
        reg.V[X] = (rand() % 256U) & NN;
        break;
    case 0xD: // Draw sprite to display
        draw(reg.V[X], reg.V[Y], N);
        break;
    case 0xE:
        switch (NN) {
        case 0x9E:
            printf("Skip next instr if (key() == Vx)"); // if (key() == V[X]) PC+=2
            break;
        case 0xA1:
            printf("Skip next instr if (key() != Vx)"); // if (key() != V[X]) PC+=2
            break;
        default:
            goto bad_opcode;
        }
        break;
    case 0xF:
        switch (NN) {
        case 0x07: // Set Vx to value of delay timer counter
            reg.V[X] = delay_timer.counter;
            break;
        case 0x0A:
            printf("Set Vx = get_key()");
            break;
        case 0x15: // Set delay timer counter to Vx
            timer_60hz_set(&delay_timer, reg.V[X]);
            break;
        case 0x18: // Set sound timer counter to Vx
            timer_60hz_set(&sound_timer, reg.V[X]);
            break;
        case 0x1E: // Set I += Vx
            reg.I += reg.V[X];
            break;
        case 0x29: // Set I to sprite for hex value at Vx 
            reg.I = FONT_OFFSET + reg.V[X];
            break;
        case 0x33: // Store BCD representation of Vx at I->I+2
            memory[reg.I + 0U] = (reg.V[X]/100)%10;
            memory[reg.I + 1U] = (reg.V[X]/10)%10;
            memory[reg.I + 2U] = (reg.V[X])%10;
            break;
        case 0x55: // Store the values of registers V0->VF in mem at addr I
            (void) memcpy(memory + reg.I, reg.V, sizeof(reg.V));
            break;
        case 0x65: // Loads the values of registers V0->VF from mem at addr I
            (void) memcpy(reg.V, memory + reg.I, sizeof(reg.V));
            break;
        default:
            goto bad_opcode;
        }
        break;
    default:
        goto bad_opcode;
    }

    return;

bad_opcode:
    printf("Bad opcode: %04X\n", opcode);
    printf("PC: %i\n", reg.PC);
    sleep(1);
    exit(EXIT_FAILURE);
}

// Simple font for printing hex characters
const uint8_t font[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

long load(const char* file_name) {
    FILE* file = fopen(file_name, "rb");

    (void) fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    (void) fseek(file, 0, SEEK_SET);

    if (MEMORY_SIZE - LOAD_OFFSET < file_size) {
        printf("ROM is too large");
        exit(EXIT_FAILURE);
    }

    // Copy ROM into memory
    (void) fread(memory + LOAD_OFFSET, sizeof(uint8_t), file_size, file);

    (void) fclose(file);

    reg.PC = LOAD_OFFSET;

    return file_size;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    srand(time(0));

    // load font into memory
    (void) memcpy(memory + FONT_OFFSET, font, sizeof(font));

    // Zero out registers
    (void) memset(&reg, 0, sizeof(reg));

    // load ROM into memory
    const long file_size = load(argv[1]);

    (void) SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = NULL;

    window = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, LOGICAL_WIDTH, LOGICAL_HEIGHT);

    (void) SDL_RenderSetLogicalSize(renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT);

    clear_display();

    SDL_Event e;
    while (1) {
        (void) SDL_PollEvent(&e);
        if (e.type == SDL_QUIT) {
            break;
        }

        if (reg.PC < LOAD_OFFSET || reg.PC > LOAD_OFFSET + file_size) {
            printf("PC out of range\n");
            break;
        }

        decode_and_execute(fetch());

        timer_60hz_decrement(&delay_timer);
        timer_60hz_decrement(&sound_timer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}