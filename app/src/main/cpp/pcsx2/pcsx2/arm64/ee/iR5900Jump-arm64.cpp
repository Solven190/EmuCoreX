// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "arm64/OaknutHelpers-arm64.h"
#include "R5900OpcodeTables.h"
#include "arm64/ee/iR5900-arm64.h"

#if !defined(__ANDROID__)
using namespace x86Emitter;
#endif

namespace R5900::Dynarec::OpcodeImpl
{

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
#ifndef JUMP_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_SYS(J);
REC_SYS_DEL(JAL, 31);
REC_SYS(JR);
REC_SYS_DEL(JALR, _Rd_);

#else

static void recMoveGPRLowToOakW(oak::WReg to, int fromgpr, bool allow_preload = true)
{
	if (fromgpr == 0)
	{
		recBeginOaknutEmit();
		oakAsm->EOR(to, to, to);
		recEndOaknutEmit();
		return;
	}

	if (GPR_IS_CONST1(fromgpr))
	{
		recBeginOaknutEmit();
		oakAsm->MOV(to, g_cpuConstRegs[fromgpr].UL[0]);
		recEndOaknutEmit();
		return;
	}

	int x86reg = _checkX86reg(X86TYPE_GPR, fromgpr, MODE_READ);
	int xmmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ);

	if (allow_preload && x86reg < 0 && xmmreg < 0)
	{
		if (EEINST_XMM_USEDTEST(fromgpr))
			xmmreg = _allocGPRtoXMMreg(fromgpr, MODE_READ);
		else if (EEINST_USEDTEST(fromgpr))
			x86reg = _allocX86reg(X86TYPE_GPR, fromgpr, MODE_READ);
	}

	recBeginOaknutEmit();
	if (x86reg >= 0)
		oakAsm->MOV(to, oakWRegister(x86reg));
	else if (xmmreg >= 0)
		oakAsm->FMOV(to, oakSRegister(xmmreg));
	else
		oakLoad32(to, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[fromgpr].UL[0]))});
	recEndOaknutEmit();
}

static void recStoreGPRLowToPcWriteback_emit_oaknut(int fromgpr)
{
	recMoveGPRLowToOakW(OAK_WSCRATCH, fromgpr);
	recBeginOaknutEmit();
	oakStore32(OAK_WSCRATCH, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.pcWriteback))});
	recEndOaknutEmit();
}

static int recMoveGPRLowToPcWritebackReg(int fromgpr)
{
	const int wbreg = _allocX86reg(X86TYPE_PCWRITEBACK, 0, MODE_WRITE | MODE_CALLEESAVED);
	recMoveGPRLowToOakW(oakWRegister(wbreg), fromgpr);
	return wbreg;
}

static void recMovePcWritebackToRAXAfterDelaySlot(int wbreg)
{
	recBeginOaknutEmit();
	if (wbreg >= 0 && x86regs[wbreg].inuse && x86regs[wbreg].type == X86TYPE_PCWRITEBACK)
	{
		oakAsm->MOV(oakWRegister(EE_HOST_RAX), oakWRegister(wbreg));
		x86regs[wbreg].inuse = 0;
	}
	else
	{
		oakLoad32(oakWRegister(EE_HOST_RAX), {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.pcWriteback))});
	}
	recEndOaknutEmit();
}

static void recStoreJumpLink_emit_oaknut(int gpr, u32 link)
{
	recBeginOaknutEmit();
	oakAsm->MOV(OAK_XSCRATCH, link);
	oakStore64(OAK_XSCRATCH, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + gpr * sizeof(GPR_reg))});
	recEndOaknutEmit();
}

////////////////////////////////////////////////////
static void recJ_emit_oaknut()
{
	// SET_FPUSTATE;
	u32 newpc = (_InstrucTarget_ << 2) + (pc & 0xf0000000);
	recompileNextInstruction(true, false);
	if (EmuConfig.Gamefixes.GoemonTlbHack)
		SetBranchImm(vtlb_V2P(newpc));
	else
		SetBranchImm(newpc);
}

void recJ()
{
	EE::Profiler.EmitOp(eeOpcode::J);
	recJ_emit_oaknut();
}

////////////////////////////////////////////////////
static void recJAL_emit_oaknut()
{
	u32 newpc = (_InstrucTarget_ << 2) + (pc & 0xf0000000);
	_deleteEEreg(31, 0);
	if (EE_CONST_PROP)
	{
		GPR_SET_CONST(31);
		g_cpuConstRegs[31].UL[0] = pc + 4;
		g_cpuConstRegs[31].UL[1] = 0;
	}
	else
	{
		recStoreJumpLink_emit_oaknut(31, pc + 4);
	}

	recompileNextInstruction(true, false);
	if (EmuConfig.Gamefixes.GoemonTlbHack)
		SetBranchImm(vtlb_V2P(newpc));
	else
		SetBranchImm(newpc);
}

void recJAL()
{
	EE::Profiler.EmitOp(eeOpcode::JAL);
	recJAL_emit_oaknut();
}

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/

////////////////////////////////////////////////////
static void recJR_emit_oaknut()
{
	const bool swap = EmuConfig.Gamefixes.GoemonTlbHack ? false : TrySwapDelaySlot(_Rs_, 0, 0, true);
	int wbreg = -1;
	if (!swap)
	{
		if (EmuConfig.Gamefixes.GoemonTlbHack)
			recStoreGPRLowToPcWriteback_emit_oaknut(_Rs_);
		else
			wbreg = recMoveGPRLowToPcWritebackReg(_Rs_);

		if (EmuConfig.Gamefixes.GoemonTlbHack)
		{
			recBeginOaknutEmit();
			oakLoad32(oakWRegister(EE_HOST_RCX), {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.pcWriteback))});
			recEndOaknutEmit();
			vtlb_DynV2P();
			recBeginOaknutEmit();
			oakStore32(oakWRegister(EE_HOST_RAX), {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.pcWriteback))});
			recEndOaknutEmit();
		}

		recompileNextInstruction(true, false);
		recMovePcWritebackToRAXAfterDelaySlot(wbreg);
	}
	else
	{
		if (GPR_IS_DIRTY_CONST(_Rs_) || _hasX86reg(X86TYPE_GPR, _Rs_, 0))
		{
			const int x86reg = _allocX86reg(X86TYPE_GPR, _Rs_, MODE_READ);
			recBeginOaknutEmit();
			oakAsm->MOV(oakWRegister(EE_HOST_RAX), oakWRegister(x86reg));
			recEndOaknutEmit();
		}
		else
		{
			recMoveGPRLowToOakW(oakWRegister(EE_HOST_RAX), _Rs_);
		}
	}

	SetBranchReg();
}

void recJR()
{
	EE::Profiler.EmitOp(eeOpcode::JR);
	recJR_emit_oaknut();
}

////////////////////////////////////////////////////
static void recJALR_emit_oaknut()
{
	const u32 newpc = pc + 4;
	const bool swap = (EmuConfig.Gamefixes.GoemonTlbHack || _Rd_ == _Rs_) ? false : TrySwapDelaySlot(_Rs_, 0, _Rd_, true);
	int wbreg = -1;

	// uncomment when there are NO instructions that need to call interpreter
	//	int mmreg;
	//	if (GPR_IS_CONST1(_Rs_))
	//	else
	//	{
	//		int mmreg;
	//
	//		if ((mmreg = _checkXMMreg(XMMTYPE_GPRREG, _Rs_, MODE_READ)) >= 0)
	//		{
	//		}
	//		else {
	//		}
	//	}

	if (!swap)
	{
		if (EmuConfig.Gamefixes.GoemonTlbHack)
			recStoreGPRLowToPcWriteback_emit_oaknut(_Rs_);
		else
			wbreg = recMoveGPRLowToPcWritebackReg(_Rs_);

		if (EmuConfig.Gamefixes.GoemonTlbHack)
		{
			recBeginOaknutEmit();
			oakLoad32(oakWRegister(EE_HOST_RCX), {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.pcWriteback))});
			recEndOaknutEmit();
			vtlb_DynV2P();
			recBeginOaknutEmit();
			oakStore32(oakWRegister(EE_HOST_RAX), {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.pcWriteback))});
			recEndOaknutEmit();
		}
	}

	if (_Rd_)
	{
		_deleteEEreg(_Rd_, 0);
		if (EE_CONST_PROP)
		{
			GPR_SET_CONST(_Rd_);
			g_cpuConstRegs[_Rd_].UD[0] = newpc;
		}
		else
		{
			recStoreJumpLink_emit_oaknut(_Rd_, newpc);
		}
	}

	if (!swap)
	{
		recompileNextInstruction(true, false);
		recMovePcWritebackToRAXAfterDelaySlot(wbreg);
	}
	else
	{
		if (GPR_IS_DIRTY_CONST(_Rs_) || _hasX86reg(X86TYPE_GPR, _Rs_, 0))
		{
			const int x86reg = _allocX86reg(X86TYPE_GPR, _Rs_, MODE_READ);
			recBeginOaknutEmit();
			oakAsm->MOV(oakWRegister(EE_HOST_RAX), oakWRegister(x86reg));
			recEndOaknutEmit();
		}
		else
		{
			recMoveGPRLowToOakW(oakWRegister(EE_HOST_RAX), _Rs_);
		}
	}

	SetBranchReg();
}

void recJALR()
{
	EE::Profiler.EmitOp(eeOpcode::JALR);
	recJALR_emit_oaknut();
}

#endif

} // namespace R5900::Dynarec::OpcodeImpl
