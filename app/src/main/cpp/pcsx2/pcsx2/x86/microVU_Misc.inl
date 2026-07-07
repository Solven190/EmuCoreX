// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include <bitset>
#include <vector>

#include "arm64/OaknutHelpers.h"

//------------------------------------------------------------------
// Micro VU - Misc Functions
//------------------------------------------------------------------

struct mVURegSaveLayout {
    struct GprPair {
        int r1;
        int r2; // -1 if none (e.g. paired with XZR)
        int offset;
    };
    struct XmmPair {
        int r1;
        int r2; // -1 if none
        int offset;
    };

    std::vector<GprPair> gpr_saves;
    std::vector<XmmPair> xmm_saves;
    int stack_size = 0;
};

static inline mVURegSaveLayout mVUGetRegSaveLayout(microVU& mVU, bool onlyNeeded)
{
    mVURegSaveLayout layout;
    std::vector<int> gprs_to_save;
    for (int i = 0; i < static_cast<int>(iREGCNT_GPR); ++i)
    {
        if (!oakIsCallerSaved(i) || i == 4)
            continue;

        if (!onlyNeeded || mVU.regAlloc->checkCachedGPR(i)) {
            gprs_to_save.push_back(i);
        }
    }

    std::vector<int> xmms_to_save;
    for (int i = 0; i < mVU.regAlloc->getXmmCount(); ++i)
    {
        if (!oakIsCallerSavedXmm(i))
            continue;

        if (!onlyNeeded || mVU.regAlloc->checkCachedReg(i) || VU_HOST_XMMPQ == i) {
            xmms_to_save.push_back(i);
        }
    }

    int current_offset = 0;

    // Lay out GPRs (8 bytes each, stored in pairs)
    for (size_t idx = 0; idx < gprs_to_save.size(); idx += 2) {
        if (idx + 1 < gprs_to_save.size()) {
            layout.gpr_saves.push_back({gprs_to_save[idx], gprs_to_save[idx + 1], current_offset});
            current_offset += 16;
        } else {
            layout.gpr_saves.push_back({gprs_to_save[idx], -1, current_offset});
            current_offset += 16;
        }
    }

    // Lay out XMMs (16 bytes each, stored in pairs)
    for (size_t idx = 0; idx < xmms_to_save.size(); idx += 2) {
        if (idx + 1 < xmms_to_save.size()) {
            layout.xmm_saves.push_back({xmms_to_save[idx], xmms_to_save[idx + 1], current_offset});
            current_offset += 32;
        } else {
            layout.xmm_saves.push_back({xmms_to_save[idx], -1, current_offset});
            current_offset += 16;
        }
    }

    layout.stack_size = current_offset;
    return layout;
}

static inline mVURegSaveLayout mVUGetWaitMTVULayout(microVU& mVU)
{
    mVURegSaveLayout layout;
    std::vector<int> gprs_to_save;
    for (int i = 0; i < static_cast<int>(iREGCNT_GPR); ++i)
    {
        if (!oakIsCallerSaved(i) || i == 4)
            continue;
        if (i == VU_HOST_T2)
            continue;
        gprs_to_save.push_back(i);
    }
    // Also save X30 (LR)
    gprs_to_save.push_back(30);

    std::vector<int> xmms_to_save;
    for (int i = 0; i < mVU.regAlloc->getXmmCount(); ++i)
    {
        if (!oakIsCallerSavedXmm(i))
            continue;
        xmms_to_save.push_back(i);
    }

    int current_offset = 0;

    // Lay out GPRs (8 bytes each, stored in pairs)
    for (size_t idx = 0; idx < gprs_to_save.size(); idx += 2) {
        if (idx + 1 < gprs_to_save.size()) {
            layout.gpr_saves.push_back({gprs_to_save[idx], gprs_to_save[idx + 1], current_offset});
            current_offset += 16;
        } else {
            layout.gpr_saves.push_back({gprs_to_save[idx], -1, current_offset});
            current_offset += 16;
        }
    }

    // Lay out XMMs (16 bytes each, stored in pairs)
    for (size_t idx = 0; idx < xmms_to_save.size(); idx += 2) {
        if (idx + 1 < xmms_to_save.size()) {
            layout.xmm_saves.push_back({xmms_to_save[idx], xmms_to_save[idx + 1], current_offset});
            current_offset += 32;
        } else {
            layout.xmm_saves.push_back({xmms_to_save[idx], -1, current_offset});
            current_offset += 16;
        }
    }

    layout.stack_size = current_offset;
    return layout;
}

// Backup Volatile Regs (EAX, ECX, EDX, MM0~7, XMM0~7, are all volatile according to 32bit Win/Linux ABI)
__fi void mVUbackupRegs(microVU& mVU, bool toMemory = false, bool onlyNeeded = false)
{
    if (toMemory)
    {
        recBeginOaknutEmit();

        mVURegSaveLayout layout = mVUGetRegSaveLayout(mVU, onlyNeeded);
        if (layout.stack_size > 0)
        {
            oakAsm->SUB(oak::util::SP, oak::util::SP, layout.stack_size);

            for (const auto& save : layout.gpr_saves) {
                if (save.r2 != -1) {
                    oakAsm->STP(oakXRegister(save.r1), oakXRegister(save.r2), oak::util::SP, oak::SOffset<10, 3>(save.offset));
                } else {
                    oakAsm->STP(oakXRegister(save.r1), oak::util::XZR, oak::util::SP, oak::SOffset<10, 3>(save.offset));
                }
            }

            for (const auto& save : layout.xmm_saves) {
                if (save.r2 != -1) {
                    oakAsm->STP(oakQRegister(save.r1), oakQRegister(save.r2), oak::util::SP, oak::SOffset<11, 4>(save.offset));
                } else {
                    oakAsm->STR(oakQRegister(save.r1), oak::util::SP, oak::POffset<16, 4>(save.offset));
                }
            }
        }

        recEndOaknutEmit();
    }
    else
	{
		// TODO(Stenzek): get rid of xmmbackup
		mVU.regAlloc->flushAll(); // Flush Regalloc
		recBeginOaknutEmit();
		oakStore128(oakQRegister(VU_HOST_XMMPQ),
			{oak::util::X28, static_cast<s64>(offsetof(vuRegistersPack, microVU[mVU.index].xmmBackup[VU_HOST_XMMPQ][0]))});
        recEndOaknutEmit();
    }
}

// Restore Volatile Regs
__fi void mVUrestoreRegs(microVU& mVU, bool fromMemory = false, bool onlyNeeded = false)
{
    if (fromMemory)
    {
        recBeginOaknutEmit();

        mVURegSaveLayout layout = mVUGetRegSaveLayout(mVU, onlyNeeded);
        if (layout.stack_size > 0)
        {
            for (const auto& save : layout.xmm_saves) {
                if (save.r2 != -1) {
                    oakAsm->LDP(oakQRegister(save.r1), oakQRegister(save.r2), oak::util::SP, oak::SOffset<11, 4>(save.offset));
                } else {
                    oakAsm->LDR(oakQRegister(save.r1), oak::util::SP, oak::POffset<16, 4>(save.offset));
                }
            }

            for (const auto& save : layout.gpr_saves) {
                if (save.r2 != -1) {
                    oakAsm->LDP(oakXRegister(save.r1), oakXRegister(save.r2), oak::util::SP, oak::SOffset<10, 3>(save.offset));
                } else {
                    oakAsm->LDP(oakXRegister(save.r1), oak::util::XZR, oak::util::SP, oak::SOffset<10, 3>(save.offset));
                }
            }

            oakAsm->ADD(oak::util::SP, oak::util::SP, layout.stack_size);
        }

        recEndOaknutEmit();
	}
	else
	{
		recBeginOaknutEmit();
		oakLoad128(oakQRegister(VU_HOST_XMMPQ),
			{oak::util::X28, static_cast<s64>(offsetof(vuRegistersPack, microVU[mVU.index].xmmBackup[VU_HOST_XMMPQ][0]))});
        recEndOaknutEmit();
    }
}

static __fi OakMemOperand mVUClampOakGlobMem(s64 offset)
{
	return mVUIrOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, mVUglob)) + offset);
}

static __fi void mVUEmitVectorConstant32_oaknut(const oak::QReg& dst, u32 value)
{
	oakAsm->MOV(OAK_WSCRATCH, value);
	oakAsm->DUP(dst.S4(), OAK_WSCRATCH);
}

static __fi void mVUEmitExponentVector_oaknut(const oak::QReg& dst)
{
	mVUEmitVectorConstant32_oaknut(dst, 0x7f800000u);
}

static __fi void mVUEmitSignbitVector_oaknut(const oak::QReg& dst)
{
	mVUEmitVectorConstant32_oaknut(dst, 0x80000000u);
}

static __fi void mVUEmitMaxvalsVector_oaknut(const oak::QReg& dst)
{
	mVUEmitVectorConstant32_oaknut(dst, 0x7f7fffffu);
}

static __fi void mVUEmitAbsclipVector_oaknut(const oak::QReg& dst)
{
	mVUEmitVectorConstant32_oaknut(dst, 0x7fffffffu);
}

static __fi void mVUEmitI32MaxFVector_oaknut(const oak::QReg& dst)
{
	mVUEmitVectorConstant32_oaknut(dst, 0x4effffffu);
}

static __fi void mVUClampDenormalScalarBits_oaknut(int reg)
{
	oak::Label done;

	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(reg));
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH, 0x7f800000u);
	oakAsm->CBNZ(OAK_WSCRATCH2, done);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000u);
	oakAsm->MOV(OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->INS(oakQRegister(reg).Selem()[0], OAK_WSCRATCH2);
	oakAsm->l(done);
}

static __fi void mVUClampDenormalVectorBits_oaknut(int reg)
{
	const oak::QReg reg_q = oakQRegister(reg);

	mVUEmitExponentVector_oaknut(OAK_QSCRATCH3);
	mVUEmitSignbitVector_oaknut(OAK_QSCRATCH2);

	oakAsm->AND(OAK_QSCRATCH.B16(), reg_q.B16(), OAK_QSCRATCH3.B16());
	oakAsm->AND(OAK_QSCRATCH2.B16(), reg_q.B16(), OAK_QSCRATCH2.B16());

	oakAsm->CMEQ(OAK_QSCRATCH.S4(), OAK_QSCRATCH.S4(), 0);
	oakAsm->BSL(OAK_QSCRATCH.B16(), OAK_QSCRATCH2.B16(), reg_q.B16());
	oakAsm->MOV(reg_q.B16(), OAK_QSCRATCH.B16());
}


static __fi void mVUClamp1ScalarBits_oaknut(int reg)
{
	oakAsm->MOV(OAK_QSCRATCH2.B16(), oakQRegister(reg).B16());
	oakLoad128(OAK_QSCRATCH3, mVUClampOakGlobMem(offsetof(mVU_Globals, maxvals)));
	oakAsm->SMIN(OAK_QSCRATCH2.S4(), OAK_QSCRATCH2.S4(), OAK_QSCRATCH3.S4());
	oakLoad128(OAK_QSCRATCH3, mVUClampOakGlobMem(offsetof(mVU_Globals, minvals)));
	oakAsm->UMIN(OAK_QSCRATCH2.S4(), OAK_QSCRATCH2.S4(), OAK_QSCRATCH3.S4());
	oakAsm->MOV(oakQRegister(reg).Selem()[0], OAK_QSCRATCH2.Selem()[0]);
}

static __fi void mVUClamp1ScalarFast_oaknut(int reg)
{
	oakLoad128(OAK_QSCRATCH3, mVUClampOakGlobMem(offsetof(mVU_Globals, maxvals)));
	oakAsm->FMINNM(OAK_SSCRATCH2, oakSRegister(reg), OAK_SSCRATCH3);
	oakLoad128(OAK_QSCRATCH3, mVUClampOakGlobMem(offsetof(mVU_Globals, minvals)));
	oakAsm->FMAXNM(OAK_SSCRATCH2, OAK_SSCRATCH2, OAK_SSCRATCH3);
	oakAsm->MOV(oakQRegister(reg).Selem()[0], OAK_QSCRATCH2.Selem()[0]);
}

static __fi void mVUClamp1VectorFast_oaknut(int reg)
{
	const oak::QReg reg_q = oakQRegister(reg);

	oakLoad128(OAK_QSCRATCH3, mVUClampOakGlobMem(offsetof(mVU_Globals, maxvals)));
	oakAsm->FMINNM(reg_q.S4(), reg_q.S4(), OAK_QSCRATCH3.S4());
	oakLoad128(OAK_QSCRATCH3, mVUClampOakGlobMem(offsetof(mVU_Globals, minvals)));
	oakAsm->FMAXNM(reg_q.S4(), reg_q.S4(), OAK_QSCRATCH3.S4());
}

static __fi void mVUClamp1VectorBits_oaknut(int reg)
{
	const oak::QReg reg_q = oakQRegister(reg);

	oakLoad128(OAK_QSCRATCH3, mVUClampOakGlobMem(offsetof(mVU_Globals, maxvals)));
	oakAsm->SMIN(reg_q.S4(), reg_q.S4(), OAK_QSCRATCH3.S4());
	oakLoad128(OAK_QSCRATCH3, mVUClampOakGlobMem(offsetof(mVU_Globals, minvals)));
	oakAsm->UMIN(reg_q.S4(), reg_q.S4(), OAK_QSCRATCH3.S4());
}

static void mVUTBit()
{
	u32 old = vu1Thread.mtvuInterrupts.fetch_or(VU_Thread::InterruptFlagVUTBit, std::memory_order_release);
	if (old & VU_Thread::InterruptFlagVUTBit)
		DevCon.Warning("Old TBit not registered");
}

static void mVUEBit()
{
	vu1Thread.mtvuInterrupts.fetch_or(VU_Thread::InterruptFlagVUEBit, std::memory_order_release);
}
static inline u32 branchAddr(const mV)
{
	pxAssumeMsg(islowerOP, "MicroVU: Expected Lower OP code for valid branch addr.");
    // return ((((iPC + 2) + (_Imm11_ * 2)) & mVU.progMemMask) * 4)
	return ((((iPC + 2) + (_Imm11_ << 1)) & mVU.progMemMask) << 2);
}

static void mVUwaitMTVU()
{
	vu1Thread.WaitVU();
}

