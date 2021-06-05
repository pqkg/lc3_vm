/* Few questions/confusions I have (just documenting):
 * Why is the LC-3 not byte addressable?
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
/* includes for unix platform specific portion of code */
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>


/* 8 general purpose register ([R0, R7])
 * 1 program counter (PC)
 * 1 condition flag register (COND)
 */
enum
{
    R0,
    R1,
    R2,
    R3,
    R4,
    R5,
    R6,
    R7,
    PC, /* program counter */
    IR, /* instruction register */
    R_COND, /* condition flag */
    R_COUNT
};

/* memory mapped registers */
enum {
        KBSR = 0xFE00, /* keyboard status */
        KBDR = 0xFE02 /* keyboard data */
};

/* opcodes */
enum {
        OP_BR = 0, /* branch */
        OP_ADD,    /* add  */
        OP_LD,     /* load */
        OP_ST,     /* store */
        OP_JSR,    /* jump register */
        OP_AND,    /* bitwise and */
        OP_LDR,    /* load register */
        OP_STR,    /* store register */
        OP_RTI,    /* unused */
        OP_NOT,    /* bitwise not */
        OP_LDI,    /* load indirect */
        OP_STI,    /* store indirect */
        OP_JMP,    /* jump */
        OP_RES,    /* reserved (unused) */
        OP_LEA,    /* load effective address */
        OP_TRAP    /* execute trap */
};

/* trap codes */
enum {
        TRAP_GETC = 0x20, /* get character from keyboard, not echoed onto the terminal */
        TRAP_OUT = 0x21, /* output a character */
        TRAP_PUTS = 0x22, /* output a word string */
        TRAP_IN = 0x23, /* get character from keyboard, echoed onto the terminal */
        TRAP_PUTSP = 0x24, /* output a byte string */
        TRAP_HALT = 0x25 /* halt the program */
};

/* condition flags */
enum {
        POS = 1 << 0, /* positive, 0b001 */
        ZRO = 1 << 1, /* zero, 0b010 */
        NEG = 1 << 2, /* negative, 0b100 */
};

/* array storing registers */
uint16_t reg[R_COUNT]; /* should overwrite first? */

/* 65536 locations
 * 16-bit word addressable
 */
/* make byte addressable perhaps? */
uint16_t memory[UINT16_MAX]; /* sufficiently small to not have to use malloc */


/* sign extends to 16 bits */
uint16_t sign_ext(uint16_t x, int bit_count)
{
        if (x >> (bit_count - 1)) { /* checks whether number is negative */
                x |= (0xFFFF << bit_count);
        }
        return x;
}

/* sets condition flag */
void update_flags(uint16_t r)
{
        if (reg[r] == 0) {
                reg[R_COND] = ZRO;
        /* checks whether most significant bit is 1 or 0
         * as that dictates whether the entire number is positive or negative */
        } else if (reg[r] >> 15) {
                reg[R_COND] = NEG;
        } else {
                reg[R_COND] = POS;
        }
}

/* swaps to opposite endian format (big or little) */
uint16_t swap16(uint16_t x)
{
        return (x << 8) | (x >> 8);
}

/* reads image file into memory */
void read_image_file(FILE* file)
{
        /* origin tells us where in memory to place the image */
        uint16_t origin;
        fread(&origin, sizeof(origin), 1, file);
        origin = swap16(origin); /* converts to little endian */

        /* maximum file size known */
        uint16_t max_read = UINT16_MAX - origin;
        uint16_t *p = &(memory[origin]);
        size_t read = fread(p, sizeof(uint16_t), max_read, file);

        /* convert to little endian as file was big endian most likely */
        while (read-- > 0) {
                *p = swap16(*p);
                ++p; /* works because is pointer to 16 bit object */
        }
}

/* convenience function for read_image_file */
int read_image(const char *image_path)
{
        FILE *file = fopen(image_path, "rb");
        if (!file)
                return 0;
        
        read_image_file(file);
        fclose(file);
        return 1;
}

/* unix platform specific (as mentioned above is copy pasted) */
uint16_t check_key()
{
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        return select(1, &readfds, NULL, NULL, &timeout) != 0;
}
/* unix platform specific end */

uint16_t mem_read(uint16_t address)
{
        /* unix platform specific (as mentioned above is copy pasted) */
        if (address == KBSR) {
                if (check_key()) {
                        memory[KBSR] = 0x1 << 15;
                        memory[KBDR] = getchar();
                } else {
                        memory[KBSR] = 0;
                }
        }
        /* unix platform specific end */
        return memory[address];
}

/* unix platform specific (as mentioned above is copy pasted) */
struct termios original_tio;

void disable_input_buffering()
{
        tcgetattr(STDIN_FILENO, &original_tio);
        struct termios new_tio = original_tio;
        new_tio.c_lflag &= ~ICANON & ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering()
{
        tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

void handle_interrupt(int signal)
{
        restore_input_buffering();
        printf("\n");
        exit(-2);
}
/* unix platform specific end */


/* main loop */
int main(int argc, const char *argv[])
{
        /* checks for valid input */
        if (argc < 2) {
                printf("lc3 <image_file1> ...\n");
                exit(1);
        }

        for (int i = 1; i < argc; ++i) {
                if (!read_image(argv[i])) {
                        printf("Failed to load image \"%s\"\n", argv[i]);
                        exit(2);
                }
        }


        /* unix platform specific (as mentioned above is copy pasted) */
        signal(SIGINT, handle_interrupt);
        disable_input_buffering();
        /* unix platform specific end */


        /* default PC starting position is 0x3000 */
        reg[PC] = 0x3000;

        int running = 1;
        while (running) {
                /* fetch */
                reg[IR] = mem_read(reg[PC]++); /* loads current instruction into IR */
                uint16_t op = reg[IR] >> 12; /* gets opcode */

                switch (op) {
                case OP_ADD:
                        {
                                /* destination register (DR) */
                                uint16_t dr = (reg[IR] >> 9) & 0x7; /* 0x7 = 0b111, width of DR */
                                /* first operand (SR1) */
                                uint16_t sr1 = (reg[IR] >> 6) & 0x7;
                                /* mode */
                                uint16_t mode = (reg[IR] >> 5) & 0x1;

                                if (mode) {
                                        uint16_t imm5 = sign_ext(reg[IR] & 0x1F, 5);
                                        reg[dr] = reg[sr1] + imm5;
                                } else {
                                        /* second operand (SR2) */
                                        uint16_t sr2 = reg[IR] & 0x7;
                                        reg[dr] = reg[sr1] + reg[sr2];
                                }

                                update_flags(dr);
                        }
                        break;
                case OP_AND:
                        {
                                uint16_t dr = (reg[IR] >> 9) & 0x7;
                                uint16_t sr1 = (reg[IR] >> 6) & 0x7;
                                uint16_t mode = (reg[IR] >> 5) & 0x1;

                                /* performs operation according to mode */
                                if (mode) {
                                        uint16_t imm5 = sign_ext(reg[IR] & 0x1F, 5);
                                        reg[dr] = reg[sr1] & imm5;
                                } else {
                                        uint16_t sr2 = reg[IR] & 0x7;
                                        reg[dr] = reg[sr1] & reg[sr2];
                                }

                                update_flags(dr);
                        }
                        break;
                case OP_NOT:
                        {
                                uint16_t dr = (reg[IR] >> 9) & 0x7;
                                uint16_t sr = (reg[IR] >> 6) & 0x7;

                                reg[dr] = ~reg[sr];
                                update_flags(dr);
                        }
                        break;
                case OP_BR:
                        {
                                uint16_t cond = (reg[IR] >> 9) & 0x7;

                                if (cond & reg[R_COND]) {
                                        uint16_t pcos = sign_ext(reg[IR] & 0x1FF, 9);
                                        reg[PC] += pcos;
                                }
                        }
                        break;
                case OP_JMP:
                        {
                                uint16_t base = (reg[IR] >> 6) & 0x7;
                                reg[PC] = reg[base];
                        }
                        break;
                case OP_JSR:
                        {
                                reg[R7] = reg[PC];
                                uint16_t mode = (reg[IR] >> 11) & 0x1;

                                if (mode) {
                                        /* JSR */
                                        uint16_t pcos = sign_ext(reg[IR] & 0x7FF, 11);
                                        reg[PC] += pcos;
                                } else {
                                        /* JSRR */
                                        uint16_t base = (reg[IR] >> 6) & 0x7;
                                        reg[PC] = reg[base];
                                }
                        }
                        break;
                case OP_LD:
                        {
                                uint16_t pcos = sign_ext(reg[IR] & 0x1FF, 9);
                                uint16_t dr = (reg[IR] >> 9) & 0x7;

                                reg[dr] = mem_read(reg[PC] + pcos);

                                update_flags(dr);
                        }
                        break;
                case OP_LDI:
                        {
                                /* destination register (DR) */
                                uint16_t dr = (reg[IR] >> 9) & 0x7;
                                /* PCoffset9 */
                                uint16_t pcos = sign_ext(reg[IR] & 0x1FF, 9);
                                /* add pcos to current PC, look at that memory location to to get final address */
                                reg[dr] = mem_read(mem_read(reg[PC] + pcos));
                                
                                update_flags(dr);
                        }
                        break;
                case OP_LDR:
                        {
                                uint16_t dr = (reg[IR] >> 9) & 0x7;
                                uint16_t base = (reg[IR] >> 6) & 0x7;
                                uint16_t os = sign_ext(reg[IR] & 0x3F, 6);

                                reg[dr] = mem_read(reg[base] + os);

                                update_flags(dr);
                        }
                        break;
                case OP_LEA:
                        {
                                uint16_t dr = (reg[IR] >> 9) & 0x7;
                                uint16_t pcos = sign_ext(reg[IR] & 0x1FF, 9);

                                reg[dr] = reg[PC] + pcos;

                                update_flags(dr);
                        }
                        break;
                case OP_ST:
                        {
                                uint16_t sr = (reg[IR] >> 9) & 0x7;
                                uint16_t pcos = sign_ext(reg[IR] & 0x1FF, 9);

                                /* need to cast due to type promotion */
                                memory[(uint16_t) (reg[PC] + pcos)] = reg[sr]; 
                        }
                        break;
                case OP_STI:
                        {
                                uint16_t sr = (reg[IR] >> 9) & 0x7;
                                uint16_t pcos = sign_ext(reg[IR] & 0x1FF, 9);

                                memory[(uint16_t) mem_read(reg[PC] + pcos)] = reg[sr];
                        }
                        break;
                case OP_STR:
                        {
                                uint16_t sr = (reg[IR] >> 9) & 0x7;
                                uint16_t base = (reg[IR] >> 6) & 0x7;
                                uint16_t os = sign_ext(reg[IR] & 0x3F, 6);

                                memory[(uint16_t) (reg[base] + os)] = reg[sr];
                        }
                        break;
                case OP_TRAP:
                        switch (reg[IR] & 0xFF) {
                        case TRAP_GETC:
                                reg[R0] = (uint16_t) getchar();
                                break;
                        case TRAP_OUT:
                                /* truncates not needed bits */
                                putc((char) reg[R0], stdout);
                                fflush(stdout);
                                break;
                        case TRAP_PUTS:
                                {
                                        /* one char per word */
                                        size_t adr = reg[R0];
                                        while (memory[adr]) {
                                                putc((char) memory[adr++], stdout);
                                        }

                                        fflush(stdout);
                                }
                                break;
                        case TRAP_IN:
                                {
                                        printf("Enter a character: ");
                                        char c = getchar();
                                        putc(c, stdout);
                                        reg[R0] = (uint16_t) c;

                                        fflush(stdout);
                                }
                                break;
                        case TRAP_PUTSP:
                                {
                                        /* one char per byte (two bytes per word), need to use big endian */
                                        size_t adr = reg[R0];
                                        while (memory[adr]) {
                                                putc(memory[adr] & 0xFF, stdout);
                                                if (memory[adr] >> 8)
                                                        putc(memory[adr++] >> 8, stdout);
                                        }

                                        fflush(stdout);
                                }
                                break;
                        case TRAP_HALT:
                                puts("HALT");
                                fflush(stdout);
                                running = 0;
                                break;
                        }
                        break;
                case OP_RES:
                case OP_RTI:
                default:
                        abort();
                }
        }
        /* unix platform specific (as mentioned above is copy pasted) */
        restore_input_buffering();
        /* unix platform specific end */
}
