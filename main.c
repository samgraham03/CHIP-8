#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "timer.h"

// Using this guide by Tobias V. Langhoff as a feature reference
// https://tobiasvl.github.io/blog/write-a-chip-8-emulator/

struct {
    uint16_t PC;
    uint16_t I;
    uint8_t V[16];
} reg;

uint8_t memory[4096];

timer_60hz_t delay_timer = { .counter = 0x0 };
timer_60hz_t sound_timer = { .counter = 0x0 };

struct {
    int top;
    uint16_t addr[16]; // todo: review stack size
} stack = { .top = 0 };

void push_address(const uint16_t addr) {
    if (stack.top == 16) {
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
        case 0xE0:
            printf("clear display");
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
        reg.V[X] = (rand() % (0xFFU + 0x1U)) & NN;
        break;
    case 0xD:
        printf("draw(Vx, Vy, N)");
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
        case 0x29:
            printf("Set I = sprite_addr[Vx]");
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
    printf("Bad opcode: %0X\n", opcode);
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

int main() {
    srand(time(0));
    // load font into memory at offset 50
    (void) memcpy(memory + 50, font, sizeof(font));

    // todo

    return 0;
}