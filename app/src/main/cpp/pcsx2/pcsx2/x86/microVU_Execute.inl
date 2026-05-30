// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Config.h"
#include "arm64/OaknutHelpers.h"
#include "cpuinfo.h"

//------------------------------------------------------------------
// Dispatcher Functions
//------------------------------------------------------------------
static bool mvuNeedsFPCRUpdate(mV)
{
	// always update on the vu1 thread
	if (isVU1 && THREAD_VU1)
		return true;

	// otherwise only emit when it's different to the EE
	return EmuConfig.Cpu.FPUFPCR.bitmask != (isVU0 ? EmuConfig.Cpu.VU0FPCR.bitmask : EmuConfig.Cpu.VU1FPCR.bitmask);
}

static __fi OakMemOperand mVUExecuteOakCpuMem(s64 offset)
{
	return {oak::util::X27, offset};
}

static __fi OakMemOperand mVUExecuteOakMvuMem(s64 offset)
{
	return {oak::util::X28, offset};
}

static __fi void mVUExecuteOakLoadRuntimeBases()
{
	oakMoveAddressToReg(oak::util::X28, &g_vuRegistersPack);
	oakMoveAddressToReg(oak::util::X27, &g_cpuRegistersPack);
}

static __fi void mVUExecuteOakLoadFpcr(OakMemOperand mem)
{
	oakLoad64(OAK_XSCRATCH, mem);
	oakAsm->MSR(oak::SystemReg::FPCR, OAK_XSCRATCH);
}

static __fi void mVUExecuteOakPshufd(oak::QReg dst, oak::QReg src, int pIndex)
{
	oakLoad128(OAK_QSCRATCH3, mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, shuffle.data[pIndex][0]))));
	oakAsm->TBL(dst.B16(), oak::List(src.B16()), OAK_QSCRATCH3.B16());
}

static __fi void mVUExecuteOakShufps(oak::QReg dst, oak::QReg src, int pIndex)
{
	oakLoad128(OAK_QSCRATCH3, mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, shuffle.data[pIndex][1]))));
	oakAsm->MOV(OAK_QSCRATCH.B16(), dst.B16());
	oakAsm->MOV(OAK_QSCRATCH2.B16(), src.B16());
	oakAsm->TBX(dst.B16(), oak::List(OAK_QSCRATCH.B16(), OAK_QSCRATCH2.B16()), OAK_QSCRATCH3.B16());
}

static void mVUExecuteOakBeginStackFrame(bool save_fpr = true)
{
	using namespace oak::util;

	oakAsm->SUB(SP, SP, save_fpr ? 160 : 96);
	oakAsm->STP(X19, X20, SP, oak::SOffset<10, 3>(0));
	oakAsm->STP(X21, X22, SP, oak::SOffset<10, 3>(16));
	oakAsm->STP(X23, X24, SP, oak::SOffset<10, 3>(32));
	oakAsm->STP(X25, X26, SP, oak::SOffset<10, 3>(48));
	oakAsm->STP(X27, X28, SP, oak::SOffset<10, 3>(64));
	oakAsm->STP(X29, X30, SP, oak::SOffset<10, 3>(80));
	if (save_fpr)
	{
		oakAsm->STP(oakDRegister(8), oakDRegister(9), SP, oak::SOffset<10, 3>(96));
		oakAsm->STP(oakDRegister(10), oakDRegister(11), SP, oak::SOffset<10, 3>(112));
		oakAsm->STP(oakDRegister(12), oakDRegister(13), SP, oak::SOffset<10, 3>(128));
		oakAsm->STP(oakDRegister(14), oakDRegister(15), SP, oak::SOffset<10, 3>(144));
	}
}

static void mVUExecuteOakEndStackFrame(bool save_fpr = true)
{
	using namespace oak::util;

	if (save_fpr)
	{
		oakAsm->LDP(oakDRegister(14), oakDRegister(15), SP, oak::SOffset<10, 3>(144));
		oakAsm->LDP(oakDRegister(12), oakDRegister(13), SP, oak::SOffset<10, 3>(128));
		oakAsm->LDP(oakDRegister(10), oakDRegister(11), SP, oak::SOffset<10, 3>(112));
		oakAsm->LDP(oakDRegister(8), oakDRegister(9), SP, oak::SOffset<10, 3>(96));
	}
	oakAsm->LDP(X29, X30, SP, oak::SOffset<10, 3>(80));
	oakAsm->LDP(X27, X28, SP, oak::SOffset<10, 3>(64));
	oakAsm->LDP(X25, X26, SP, oak::SOffset<10, 3>(48));
	oakAsm->LDP(X23, X24, SP, oak::SOffset<10, 3>(32));
	oakAsm->LDP(X21, X22, SP, oak::SOffset<10, 3>(16));
	oakAsm->LDP(X19, X20, SP, oak::SOffset<10, 3>(0));
	oakAsm->ADD(SP, SP, save_fpr ? 160 : 96);
}

// Generates the code for entering/exit recompiled blocks
void mVUdispatcherAB(mV)
{
    mVU.startFunct = recBeginOaknutEmit();
	{
        mVUExecuteOakBeginStackFrame();

        // From memory to registry
        mVUExecuteOakLoadRuntimeBases();

		// = The caller has already put the needed parameters in ecx/edx:
        if (!isVU1) {
            oakEmitCall(reinterpret_cast<void*>(mVUexecuteVU0));
        }
        else        {
            oakEmitCall(reinterpret_cast<void*>(mVUexecuteVU1));
        }

		// Load VU's MXCSR state
		if (mvuNeedsFPCRUpdate(mVU)) {
            const s64 vu_fpcr_offset = static_cast<s64>(isVU0 ? offsetof(cpuRegistersPack, Cpu.VU0FPCR.bitmask) : offsetof(cpuRegistersPack, Cpu.VU1FPCR.bitmask));
            mVUExecuteOakLoadFpcr(mVUExecuteOakCpuMem(vu_fpcr_offset));
        }

        // Load Regs
        oakLoad128(oakQRegister(VU_HOST_XMMT1), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].VI[REG_P].UL))));
        oakLoad128(oakQRegister(VU_HOST_XMMPQ), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].VI[REG_Q].UL))));
        oakAsm->MOVI(oakQRegister(VU_HOST_XMMT2).B16(), 0);
        oakLoad32(OAK_WSCRATCH, mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].pending_q))));
        oakAsm->MOV(oakQRegister(VU_HOST_XMMT2).Selem()[0], OAK_WSCRATCH);
        mVUExecuteOakShufps(oakQRegister(VU_HOST_XMMPQ), oakQRegister(VU_HOST_XMMT1), 0);
        //Load in other Q instance
        mVUExecuteOakPshufd(oakQRegister(VU_HOST_XMMPQ), oakQRegister(VU_HOST_XMMPQ), 0xe1);
        oakAsm->MOV(oakQRegister(VU_HOST_XMMPQ).Selem()[0], oakQRegister(VU_HOST_XMMT2).Selem()[0]);
        mVUExecuteOakPshufd(oakQRegister(VU_HOST_XMMPQ), oakQRegister(VU_HOST_XMMPQ), 0xe1);

        if (isVU1)
        {
            //Load in other P instance
            oakAsm->MOVI(oakQRegister(VU_HOST_XMMT2).B16(), 0);
            oakLoad32(OAK_WSCRATCH, mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].pending_p))));
            oakAsm->MOV(oakQRegister(VU_HOST_XMMT2).Selem()[0], OAK_WSCRATCH);
            mVUExecuteOakPshufd(oakQRegister(VU_HOST_XMMPQ), oakQRegister(VU_HOST_XMMPQ), 0x1B);
            oakAsm->MOV(oakQRegister(VU_HOST_XMMPQ).Selem()[0], oakQRegister(VU_HOST_XMMT2).Selem()[0]);
            mVUExecuteOakPshufd(oakQRegister(VU_HOST_XMMPQ), oakQRegister(VU_HOST_XMMPQ), 0x1B);
        }

        oakLoad128(oakQRegister(VU_HOST_XMMT1), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].micro_macflags))));
        oakStore128(oakQRegister(VU_HOST_XMMT1), mVUExecuteOakMvuMem(static_cast<s64>(offsetof(vuRegistersPack, microVU[mVU.index].macFlag))));

        oakLoad128(oakQRegister(VU_HOST_XMMT1), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].micro_clipflags))));
        oakStore128(oakQRegister(VU_HOST_XMMT1), mVUExecuteOakMvuMem(static_cast<s64>(offsetof(vuRegistersPack, microVU[mVU.index].clipFlag))));

        oakLoad32(oakWRegister(VU_HOST_F0), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].micro_statusflags[0]))));
        oakLoad32(oakWRegister(VU_HOST_F1), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].micro_statusflags[1]))));
        oakLoad32(oakWRegister(VU_HOST_F2), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].micro_statusflags[2]))));
        oakLoad32(oakWRegister(VU_HOST_F3), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].micro_statusflags[3]))));

		// Jump to Recompiled Code Block
        oakAsm->BR(oak::util::X0);

		mVU.exitFunct = oakGetCurrentCodePointer();

		// Load EE's MXCSR state
		if (mvuNeedsFPCRUpdate(mVU)) {
            mVUExecuteOakLoadFpcr(mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, Cpu.FPUFPCR.bitmask))));
        }

		// = The first two DWORD or smaller arguments are passed in ECX and EDX registers;
		//              all other arguments are passed right to left.
        if (!isVU1) {
            oakEmitCall(reinterpret_cast<void*>(mVUcleanUpVU0));
        }
        else        {
            oakEmitCall(reinterpret_cast<void*>(mVUcleanUpVU1));
        }

        mVUExecuteOakEndStackFrame();
	}

    oakAsm->RET();
    recEndOaknutEmit();

	Perf::any.Register(mVU.startFunct, static_cast<u32>(mVU.prog.x86start - mVU.startFunct),
		mVU.index ? "VU1StartFunc" : "VU0StartFunc");
}

// Generates the code for resuming/exit xgkick
void mVUdispatcherCD(mV)
{
    mVU.startFunctXG = recBeginOaknutEmit();
	{
        mVUExecuteOakBeginStackFrame();
		mVUExecuteOakLoadRuntimeBases();

        // Load VU's MXCSR state
        if (mvuNeedsFPCRUpdate(mVU)) {
            const s64 vu_fpcr_offset = static_cast<s64>(isVU0 ? offsetof(cpuRegistersPack, Cpu.VU0FPCR.bitmask) : offsetof(cpuRegistersPack, Cpu.VU1FPCR.bitmask));
            mVUExecuteOakLoadFpcr(mVUExecuteOakCpuMem(vu_fpcr_offset));
        }

        mVUrestoreRegs(mVU);
        oakLoad32(oakWRegister(VU_HOST_F0), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].micro_statusflags[0]))));
        oakLoad32(oakWRegister(VU_HOST_F1), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].micro_statusflags[1]))));
        oakLoad32(oakWRegister(VU_HOST_F2), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].micro_statusflags[2]))));
        oakLoad32(oakWRegister(VU_HOST_F3), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].micro_statusflags[3]))));

        // Jump to Recompiled Code Block
        oakEmitJmp(&mVU.resumePtrXG);

        mVU.exitFunctXG = oakGetCurrentCodePointer();

        // Backup Status Flag (other regs were backed up on xgkick)
        oakStore32(oakWRegister(VU_HOST_F0), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].micro_statusflags[0]))));
        oakStore32(oakWRegister(VU_HOST_F1), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].micro_statusflags[1]))));
        oakStore32(oakWRegister(VU_HOST_F2), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].micro_statusflags[2]))));
        oakStore32(oakWRegister(VU_HOST_F3), mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].micro_statusflags[3]))));

        // Load EE's MXCSR state
        if (mvuNeedsFPCRUpdate(mVU)) {
            mVUExecuteOakLoadFpcr(mVUExecuteOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, Cpu.FPUFPCR.bitmask))));
        }

        mVUExecuteOakEndStackFrame();
	}

    oakAsm->RET();
    recEndOaknutEmit();

	Perf::any.Register(mVU.startFunctXG, static_cast<u32>(mVU.prog.x86start - mVU.startFunctXG),
		mVU.index ? "VU1StartFuncXG" : "VU0StartFuncXG");
}

static void mVUGenerateWaitMTVU(mV)
{
    mVU.waitMTVU = recBeginOaknutEmit();

    int i;
    for (i = 0; i < static_cast<int>(iREGCNT_GPR); ++i)
    {
        if (!oakIsCallerSaved(i) || i == 4)
            continue;

        // T1 often contains the address we're loading when waiting for VU1.
        // T2 isn't used until afterwards, so don't bother saving it.
        if (i == VU_HOST_T2)
            continue;

        oakAsm->STP(oakXRegister(i), oak::util::XZR, oak::util::SP, oak::PreIndexed{}, oak::SOffset<10, 3>(-16));
    }
    //
    for (i = 0; i < static_cast<int>(iREGCNT_XMM); ++i)
    {
        if (!oakIsCallerSavedXmm(i))
            continue;

        oakAsm->STR(oakQRegister(i), oak::util::SP, oak::PreIndexed{}, oak::SOffset<9, 0>(-16));
    }

    ////
    oakAsm->STP(oak::util::X30, oak::util::XZR, oak::util::SP, oak::PreIndexed{}, oak::SOffset<10, 3>(-16));
    oakEmitCall((void*)mVUwaitMTVU);
    oakAsm->LDP(oak::util::X30, oak::util::XZR, oak::util::SP, oak::PostIndexed{}, oak::SOffset<10, 3>(16));
    ////

    for (i = static_cast<int>(iREGCNT_XMM - 1); i >= 0; --i)
    {
        if (!oakIsCallerSavedXmm(i))
            continue;

        oakAsm->LDR(oakQRegister(i), oak::util::SP, oak::PostIndexed{}, oak::SOffset<9, 0>(16));
    }
    //
    for (i = static_cast<int>(iREGCNT_GPR - 1); i >= 0; --i)
    {
        if (!oakIsCallerSaved(i) || i == 4)
            continue;

        if (i == VU_HOST_T2)
            continue;

        oakAsm->LDP(oakXRegister(i), oak::util::XZR, oak::util::SP, oak::PostIndexed{}, oak::SOffset<10, 3>(16));
    }

    oakAsm->RET();
    recEndOaknutEmit();

	Perf::any.Register(mVU.waitMTVU, static_cast<u32>(mVU.prog.x86start - mVU.waitMTVU),
		mVU.index ? "VU1WaitMTVU" : "VU0WaitMTVU");
}

static void mVUCompareMaskPs_emit_oaknut(oak::WReg dst, oak::QReg src)
{
	oakAsm->MOV(dst, 0);
	for (int lane = 0; lane < 4; lane++)
	{
		oakAsm->MOV(OAK_WSCRATCH, src.Selem()[lane]);
		oakAsm->LSR(OAK_WSCRATCH, OAK_WSCRATCH, 31);
		oakAsm->ORR(dst, dst, OAK_WSCRATCH, oak::LogShift::LSL, lane);
	}
}

static void mVUGenerateCopyPipelineState(mV)
{
    mVU.copyPLState = recBeginOaknutEmit();

	constexpr int state_size = 6;
	const s64 dst_base = static_cast<s64>(offsetof(vuRegistersPack, microVU[mVU.index].prog.lpState));
	for (int i = 0; i < state_size; i++)
	{
		oakLoad128(oakQRegister(i), {oak::util::X0, i * 16});
		oakStore128(oakQRegister(i), {oak::util::X28, dst_base + i * 16});
	}

	oakAsm->RET();
	recEndOaknutEmit();

	Perf::any.Register(mVU.copyPLState, static_cast<u32>(mVU.prog.x86start - mVU.copyPLState),
		mVU.index ? "VU1CopyPLState" : "VU0CopyPLState");
}

//------------------------------------------------------------------
// Micro VU - Custom Quick Search
//------------------------------------------------------------------

// Generates a custom optimized block-search function
// Note: Structs must be 16-byte aligned! (GCC doesn't guarantee this)
static void mVUGenerateCompareState(mV)
{
    mVU.compareStateF = recBeginOaknutEmit();

	oak::Label exitPoint;

	oakLoad128(oak::util::Q0, {oak::util::X1, 0x00});
	oakLoad128(oak::util::Q1, {oak::util::X2, 0x00});
	oakAsm->CMEQ(oak::util::Q0.S4(), oak::util::Q0.S4(), oak::util::Q1.S4());
	oakLoad128(oak::util::Q1, {oak::util::X1, 0x10});
	oakLoad128(oak::util::Q2, {oak::util::X2, 0x10});
	oakAsm->CMEQ(oak::util::Q1.S4(), oak::util::Q1.S4(), oak::util::Q2.S4());
	oakAsm->AND(oak::util::Q0.B16(), oak::util::Q0.B16(), oak::util::Q1.B16());
	mVUCompareMaskPs_emit_oaknut(oak::util::W0, oak::util::Q0);
	oakAsm->EOR(oak::util::W0, oak::util::W0, 0xf);
	oakAsm->CBNZ(oak::util::W0, exitPoint);

	oakLoad128(oak::util::Q0, {oak::util::X1, 0x20});
	oakLoad128(oak::util::Q1, {oak::util::X2, 0x20});
	oakAsm->CMEQ(oak::util::Q0.S4(), oak::util::Q0.S4(), oak::util::Q1.S4());
	oakLoad128(oak::util::Q1, {oak::util::X1, 0x30});
	oakLoad128(oak::util::Q2, {oak::util::X2, 0x30});
	oakAsm->CMEQ(oak::util::Q1.S4(), oak::util::Q1.S4(), oak::util::Q2.S4());
	oakAsm->AND(oak::util::Q0.B16(), oak::util::Q0.B16(), oak::util::Q1.B16());

	oakLoad128(oak::util::Q1, {oak::util::X1, 0x40});
	oakLoad128(oak::util::Q2, {oak::util::X2, 0x40});
	oakAsm->CMEQ(oak::util::Q1.S4(), oak::util::Q1.S4(), oak::util::Q2.S4());
	oakLoad128(oak::util::Q2, {oak::util::X1, 0x50});
	oakLoad128(oak::util::Q3, {oak::util::X2, 0x50});
	oakAsm->CMEQ(oak::util::Q2.S4(), oak::util::Q2.S4(), oak::util::Q3.S4());
	oakAsm->AND(oak::util::Q1.B16(), oak::util::Q1.B16(), oak::util::Q2.B16());
	oakAsm->AND(oak::util::Q0.B16(), oak::util::Q0.B16(), oak::util::Q1.B16());
	mVUCompareMaskPs_emit_oaknut(oak::util::W0, oak::util::Q0);
	oakAsm->EOR(oak::util::W0, oak::util::W0, 0xf);

	oakAsm->l(exitPoint);
	oakAsm->RET();
	recEndOaknutEmit();
}


//------------------------------------------------------------------
// Execution Functions
//------------------------------------------------------------------

// Executes for number of cycles
_mVUt void* mVUexecute(u32 startPC, u32 cycles)
{
	microVU& mVU = mVUx;
	u32 vuLimit = vuIndex ? 0x3ff8 : 0xff8;
	if (startPC > vuLimit + 7)
	{
		DevCon.Warning("microVU%x Warning: startPC = 0x%x, cycles = 0x%x", vuIndex, startPC, cycles);
	}

	mVU.cycles = cycles;
	mVU.totalCycles = cycles;


	return mVUsearchProg<vuIndex>(startPC & vuLimit, (uptr)&mVU.prog.lpState); // Find and set correct program
}

//------------------------------------------------------------------
// Cleanup Functions
//------------------------------------------------------------------

_mVUt void mVUcleanUp()
{
	microVU& mVU = mVUx;

//	mVU.prog.x86ptr = x86Ptr;

	if ((mVU.prog.x86ptr < mVU.prog.x86start) || (mVU.prog.x86ptr >= mVU.prog.x86end))
	{
		mVUreset(mVU, false);
	}

	mVU.cycles = mVU.totalCycles - std::max(0, mVU.cycles);
	mVU.regs().cycle += mVU.cycles;

	if (!vuIndex || !THREAD_VU1)
	{
		u32 cycles_passed = std::min(mVU.cycles, 3000) * EmuConfig.Speedhacks.EECycleSkip;
		if (cycles_passed > 0)
		{
			s32 vu0_offset = VU0.cycle - cpuRegs.cycle;
			cpuRegs.cycle += cycles_passed;

			// VU0 needs to stay in sync with the CPU otherwise things get messy
			// So we need to adjust when VU1 skips cycles also
			if (!vuIndex)
				VU0.cycle = cpuRegs.cycle + vu0_offset;
			else
				VU0.cycle += cycles_passed;
		}
	}
	mVU.profiler.Print();
}

//------------------------------------------------------------------
// Caller Functions
//------------------------------------------------------------------

void* mVUexecuteVU0(u32 startPC, u32 cycles) { return mVUexecute<0>(startPC, cycles); }
void* mVUexecuteVU1(u32 startPC, u32 cycles) { return mVUexecute<1>(startPC, cycles); }
void mVUcleanUpVU0() { mVUcleanUp<0>(); }
void mVUcleanUpVU1() { mVUcleanUp<1>(); }

