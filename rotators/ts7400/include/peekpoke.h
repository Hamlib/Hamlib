// We can't expect that a dereference of an unsigned short * always
// produces a ldrh or strh since the compiler may choose to use
// a byte write instead.  Hence, we emit the peeks and pokes using
// inline assembler.  --JO
static inline unsigned short PEEK16(unsigned long addr) {
        unsigned short ret;

        asm volatile (
                "ldrh %0, [ %1 ]\n"
                : "=r" (ret)
                : "r" (addr)
                : "memory"
        );
        return ret;
}

static inline void POKE16(unsigned long addr, unsigned short dat) {
        asm volatile (
                "strh %1, [ %0 ]\n"
                :
                : "r" (addr), "r" (dat)
                : "memory"
        );
}

static inline unsigned long PEEK32(unsigned long addr) {
        unsigned long ret;

        asm volatile (
                "ldr %0, [ %1 ]\n"
                : "=r" (ret)
                : "r" (addr)
                : "memory"
        );
        return ret;
}

static inline void POKE32(unsigned long addr, unsigned long dat) {
        asm volatile (
                "str %1, [ %0 ]\n"
                :
                : "r" (addr), "r" (dat)
                : "memory"
        );
}


static inline unsigned char PEEK8(unsigned long addr) {
        unsigned char ret;

        asm volatile (
                "ldrb %0, [ %1 ]\n"
                : "=r" (ret)
                : "r" (addr)
                : "memory"
        );
        return ret;
}

static inline void POKE8(unsigned long addr, unsigned char dat) {
        asm volatile (
                "strb %1, [ %0 ]\n"
                :
                : "r" (addr), "r" (dat)
                : "memory"
        );
}

#define TRUE  0x01
#define FALSE 0x00
