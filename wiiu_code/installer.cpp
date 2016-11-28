
#include "cafe.h"

#include "code.h"
#define INSTALL_ADDR 0x011DD000
#define MAIN_JMP_ADDR 0x0101C56C

#define KERN_ADDRESS_TBL 0xFFEAB7A0

#define Acquire(module) \
	int module; \
	OSDynLoad_Acquire(#module ".rpl", &module);

#define Import(module, function) \
    function ##_t function; \
	OSDynLoad_FindExport(module, 0, #function, &function);

void kern_write(u32 addr, u32 value) {
	asm(
		"stwu 1, -0x20(1)\n"
		"mflr 0\n"
		"stw 0, 0x24(1)\n"
		"stw 29, 0x14(1)\n"
		"stw 30, 0x18(1)\n"
		"stw 31, 0x1C(1)\n"
		"mr 31, 1\n"
		"stw 3, 8(31)\n"
		"stw 4, 0xC(31)\n"
		"lwz 30, 8(31)\n"
		"lwz 29, 0xC(31)\n"
		"li 3, 1\n"
		"li 4, 0\n"
		"mr 5, 29\n"
		"li 6, 0\n"
		"li 7, 0\n"
		"lis 8, 1\n"
		"mr 9, 30\n"
		"mr 29, 1\n"
		"li 0, 0x3500\n"
		"sc\n"
		"nop\n"
		"mr 1, 29\n"
		"addi 11, 31, 0x20\n"
		"lwz 0, 4(11)\n"
		"mtlr 0\n"
		"lwz 29, -0xC(11)\n"
		"lwz 30, -8(11)\n"
		"lwz 31, -4(11)\n"
		"mr 1, 11\n"
	);
}

void memcpy(void* dst, const void* src, u32 size) {
	for (u32 i = 0; i < size; i++) {
		*((u8 *)dst + i) = *((const u8 *)src + i);
	}
}

u32 doBL(u32 dst, u32 src) {
	u32 instr = (dst - src);
	instr &= 0x03FFFFFC;
	instr |= 0x48000001;
	return instr;
}

u32 doB(u32 dst, u32 src) {
	u32 instr = (dst - src);
	instr &= 0x03FFFFFC;
	instr |= 0x48000000;
	return instr;
}

void insertBranchL(u32 addr, u32 target, DCFlushRange_t DCFlushRange, ICInvalidateRange_t ICInvalidateRange) {
	u32 *loc = (u32 *)(0xA0000000 + addr);
	*loc = doBL(target, addr);
	DCFlushRange(loc, 4);
	ICInvalidateRange(loc, 4);
}

void insertBranch(u32 addr, u32 target, DCFlushRange_t DCFlushRange, ICInvalidateRange_t ICInvalidateRange) {
	u32 *loc = (u32 *)(0xA0000000 + addr);
	*loc = doB(target, addr);
	DCFlushRange(loc, 4);
	ICInvalidateRange(loc, 4);
}

void _main() {
    Acquire(coreinit)

    Import(coreinit, DCFlushRange)
    Import(coreinit, ICInvalidateRange)
    Import(coreinit, OSEffectiveToPhysical)
    Import(coreinit, _Exit);

    if (OSEffectiveToPhysical((void *)0xA0000000) != 0) {
        u32 codeAddr = 0xA0000000 + INSTALL_ADDR;
        memcpy((void *)codeAddr, code_bin, code_length);
        DCFlushRange((void *)codeAddr, code_length);
        ICInvalidateRange((void *)codeAddr, code_length);

        insertBranchL(MAIN_JMP_ADDR, INSTALL_ADDR, DCFlushRange, ICInvalidateRange);

        //Disable OSSetExceptionCallback because DKC: TF will
        //overwrite our own exception handlers otherwise
        //Problem: Splatoon uses OSSetExceptionCallbackEx
        Import(coreinit, OSSetExceptionCallback)
        void *addr = (u8 *)OSSetExceptionCallback + 0xA0000000;
        *(u32 *)addr = 0x4E800020; //blr
        DCFlushRange(addr, 4);
        ICInvalidateRange(addr, 4);

        kern_write(KERN_ADDRESS_TBL + 0x48, 0x80000000);
        kern_write(KERN_ADDRESS_TBL + 0x4C, 0x28305800);

        //Clear globals pointer
        *(u32 *)0x10000000 = 0;
    }
    _Exit();
}
