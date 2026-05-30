// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include <bitset>

#include "arm64/OaknutHelpers.h"

//------------------------------------------------------------------
// Micro VU - Misc Functions
//------------------------------------------------------------------

// Backup Volatile Regs (EAX, ECX, EDX, MM0~7, XMM0~7, are all volatile according to 32bit Win/Linux ABI)
__fi void mVUbackupRegs(microVU& mVU, bool toMemory = false, bool onlyNeeded = false)
{
    if (toMemory)
    {
        recBeginOaknutEmit();

        int i, e = iREGCNT_GPR;
        for (i = 0; i < e; ++i)
        {
            if (!oakIsCallerSaved(i) || i == 4)
                continue;

            if (!onlyNeeded || mVU.regAlloc->checkCachedGPR(i)) {
                oakAsm->STP(oakXRegister(i), oak::util::XZR, oak::util::SP, oak::PreIndexed{}, oak::SOffset<10, 3>(-16));
            }
        }

        ////

        e = iREGCNT_XMM;
        for (i = 0; i < e; ++i)
        {
            if (!oakIsCallerSavedXmm(i))
                continue;

            if (!onlyNeeded || mVU.regAlloc->checkCachedReg(i) || VU_HOST_XMMPQ == i) {
                oakAsm->STR(oakQRegister(i), oak::util::SP, oak::PreIndexed{}, oak::SOffset<9, 0>(-16));
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

        int i, e = iREGCNT_XMM - 1;
        for (i = e; i >= 0; --i)
        {
            if (!oakIsCallerSavedXmm(i))
                continue;

            if (!onlyNeeded || mVU.regAlloc->checkCachedReg(i) || VU_HOST_XMMPQ == i) {
                oakAsm->LDR(oakQRegister(i), oak::util::SP, oak::PostIndexed{}, oak::SOffset<9, 0>(16));
            }
        }

        ////

        e = iREGCNT_GPR - 1;
        for (i = e; i >= 0; --i)
        {
            if (!oakIsCallerSaved(i)  || i == 4)
                continue;

            if (!onlyNeeded || mVU.regAlloc->checkCachedGPR(i)) {
                oakAsm->LDP(oakXRegister(i), oak::util::XZR, oak::util::SP, oak::PostIndexed{}, oak::SOffset<10, 3>(16));
            }
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

static __fi void mVUClamp1ScalarBits_oaknut(int reg)
{
	oak::Label check_infinity;
	oak::Label done;

	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(reg));
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH, 0x7f800000u);
	oakAsm->CBNZ(OAK_WSCRATCH2, check_infinity);

	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000u);
	oakAsm->FMOV(oakSRegister(reg), OAK_WSCRATCH);
	oakAsm->B(done);

	oakAsm->l(check_infinity);
	oakAsm->MOV(OAK_WSCRATCH, 0x7f800000u);
	oakAsm->CMP(OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->B(oak::util::NE, done);
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(reg));
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, 0x80000000u);
	oakAsm->MOV(OAK_WSCRATCH, 0x7f7fffffu);
	oakAsm->ORR(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->FMOV(oakSRegister(reg), OAK_WSCRATCH2);

	oakAsm->l(done);
}

static __fi void mVUClamp1VectorBits_oaknut(int reg)
{
	const oak::QReg reg_q = oakQRegister(reg);

	oakLoad128(OAK_QSCRATCH3, mVUClampOakGlobMem(offsetof(mVU_Globals, exponent)));
	oakAsm->AND(OAK_QSCRATCH.B16(), reg_q.B16(), OAK_QSCRATCH3.B16());

	oakAsm->EOR(OAK_QSCRATCH2.B16(), OAK_QSCRATCH2.B16(), OAK_QSCRATCH2.B16());
	oakAsm->CMEQ(OAK_QSCRATCH2.S4(), OAK_QSCRATCH.S4(), OAK_QSCRATCH2.S4());
	oakLoad128(OAK_QSCRATCH3, mVUClampOakGlobMem(offsetof(mVU_Globals, signbit)));
	oakAsm->AND(OAK_QSCRATCH3.B16(), reg_q.B16(), OAK_QSCRATCH3.B16());
	oakAsm->BSL(OAK_QSCRATCH2.B16(), OAK_QSCRATCH3.B16(), reg_q.B16());
	oakAsm->MOV(reg_q.B16(), OAK_QSCRATCH2.B16());

	oakLoad128(OAK_QSCRATCH2, mVUClampOakGlobMem(offsetof(mVU_Globals, exponent)));
	oakAsm->CMEQ(OAK_QSCRATCH.S4(), OAK_QSCRATCH.S4(), OAK_QSCRATCH2.S4());
	oakLoad128(OAK_QSCRATCH3, mVUClampOakGlobMem(offsetof(mVU_Globals, signbit)));
	oakAsm->AND(OAK_QSCRATCH3.B16(), reg_q.B16(), OAK_QSCRATCH3.B16());
	oakLoad128(OAK_QSCRATCH2, mVUClampOakGlobMem(offsetof(mVU_Globals, maxvals)));
	oakAsm->ORR(OAK_QSCRATCH3.B16(), OAK_QSCRATCH3.B16(), OAK_QSCRATCH2.B16());
	oakAsm->BSL(OAK_QSCRATCH.B16(), OAK_QSCRATCH3.B16(), reg_q.B16());
	oakAsm->MOV(reg_q.B16(), OAK_QSCRATCH.B16());
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

