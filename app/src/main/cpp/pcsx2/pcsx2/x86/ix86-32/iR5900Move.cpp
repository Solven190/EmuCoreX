// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "arm64/OaknutHelpers.h"
#include "x86/iR5900.h"

#if !defined(__ANDROID__)
using namespace x86Emitter;
#endif

namespace R5900::Dynarec::OpcodeImpl
{

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
#ifndef MOVE_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_FUNC_DEL(LUI, _Rt_);
REC_FUNC_DEL(MFLO, _Rd_);
REC_FUNC_DEL(MFHI, _Rd_);
REC_FUNC(MTLO);
REC_FUNC(MTHI);

REC_FUNC_DEL(MFLO1, _Rd_);
REC_FUNC_DEL(MFHI1, _Rd_);
REC_FUNC(MTHI1);
REC_FUNC(MTLO1);

REC_FUNC_DEL(MOVZ, _Rd_);
REC_FUNC_DEL(MOVN, _Rd_);

#else

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/

//// LUI
static void recLUI_emit_oaknut(int regt, s64 value)
{
	recBeginOaknutEmit();
	oakAsm->MOV(oakXRegister(regt), static_cast<u64>(value));
	recEndOaknutEmit();
}

void recLUI()
{
	if (!_Rt_)
		return;

	// need to flush the upper 64 bits for xmm
	GPR_DEL_CONST(_Rt_);
	_deleteGPRtoX86reg(_Rt_, DELETE_REG_FREE_NO_WRITEBACK);
	_deleteGPRtoXMMreg(_Rt_, DELETE_REG_FLUSH_AND_FREE);

	if (EE_CONST_PROP)
	{
		GPR_SET_CONST(_Rt_);
		g_cpuConstRegs[_Rt_].UD[0] = (s32)(cpuRegs.code << 16);
	}
	else
	{
		const int regt = _allocX86reg(X86TYPE_GPR, _Rt_, MODE_WRITE);
		recLUI_emit_oaknut(regt, (s64)(s32)(cpuRegs.code << 16));
	}

	EE::Profiler.EmitOp(eeOpcode::LUI);
}

static void recMoveGPRtoOakX(oak::XReg to, int fromgpr)
{
	if (fromgpr == 0)
	{
		oakAsm->MOV(to, 0);
		return;
	}

	if (GPR_IS_CONST1(fromgpr))
	{
		oakAsm->MOV(to, g_cpuConstRegs[fromgpr].UD[0]);
		return;
	}

	const int x86reg = _checkX86reg(X86TYPE_GPR, fromgpr, MODE_READ);
	if (x86reg >= 0)
	{
		oakAsm->MOV(to, oakXRegister(x86reg));
		return;
	}

	const int xmmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ);
	if (xmmreg >= 0)
	{
		oakAsm->FMOV(to, oakDRegister(xmmreg));
		return;
	}

	oakLoad64(to, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[fromgpr].UD[0]))});
}

template <int HiloReg, u8 Lane>
static void recMFHILOExact()
{
	if (!_Rd_)
		return;

	// kill any constants on rd, lower 64 bits get written regardless of upper
	_eeOnWriteReg(_Rd_, 0);

	const int reg = HiloReg;
	constexpr s64 hiloOffset = HiloReg == XMMGPR_HI ? offsetof(cpuRegistersPack, cpuRegs.HI.UD[Lane]) : offsetof(cpuRegistersPack, cpuRegs.LO.UD[Lane]);
	const int xmmd = EEINST_XMMUSEDTEST(_Rd_) ? _allocGPRtoXMMreg(_Rd_, MODE_READ | MODE_WRITE) : _checkXMMreg(XMMTYPE_GPRREG, _Rd_, MODE_READ | MODE_WRITE);
	const int xmmhilo = EEINST_XMMUSEDTEST(reg) ? _allocGPRtoXMMreg(reg, MODE_READ) : _checkXMMreg(XMMTYPE_GPRREG, reg, MODE_READ);
	if (xmmd >= 0)
	{
		const int gprhilo = (xmmhilo < 0 && Lane == 0) ? _allocIfUsedGPRtoX86(reg, MODE_READ) : -1;
		recBeginOaknutEmit();
		if (xmmhilo >= 0)
		{
			oakAsm->MOV(oakQRegister(xmmd).Delem()[0], oakQRegister(xmmhilo).Delem()[Lane]);
		}
		else
		{
			if (gprhilo >= 0)
			{
				oakAsm->INS(oakQRegister(xmmd).Delem()[0], oakXRegister(gprhilo));
			}
			else {
				oakLoad64(OAK_XSCRATCH, {oak::util::X27, hiloOffset});
				oakAsm->INS(oakQRegister(xmmd).Delem()[0], OAK_XSCRATCH);
            }
		}
		recEndOaknutEmit();
	}
	else
	{
		// try rename {hi,lo} -> rd
		const int gprreg = (Lane != 0) ? -1 : _checkX86reg(X86TYPE_GPR, reg, MODE_READ);
		if (gprreg >= 0 && _eeTryRenameReg(_Rd_, reg, gprreg, -1, 0) >= 0)
			return;

		const int gprd = _allocIfUsedGPRtoX86(_Rd_, MODE_WRITE);
		if (gprd >= 0 && xmmhilo >= 0)
		{
			pxAssert(gprreg < 0);
			recBeginOaknutEmit();
			if constexpr (Lane != 0)
				oakAsm->FMOV(oakXRegister(gprd), oakQRegister(xmmhilo).Delem()[1]);
			else
				oakAsm->FMOV(oakXRegister(gprd), oakDRegister(xmmhilo));
			recEndOaknutEmit();
		}
		else if (gprd < 0 && xmmhilo >= 0)
		{
			pxAssert(gprreg < 0);
			recBeginOaknutEmit();
			if constexpr (Lane != 0)
				oakAsm->FMOV(OAK_XSCRATCH, oakQRegister(xmmhilo).Delem()[1]);
			else
				oakAsm->FMOV(OAK_XSCRATCH, oakDRegister(xmmhilo));
			oakStore64(OAK_XSCRATCH, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rd_].UD[0]))});
			recEndOaknutEmit();
		}
		else if (gprd >= 0)
		{
			recBeginOaknutEmit();
			if (gprreg >= 0) {
				oakAsm->MOV(oakXRegister(gprd), oakXRegister(gprreg));
            }
			else {
				oakLoad64(oakXRegister(gprd), {oak::util::X27, hiloOffset});
            }
			recEndOaknutEmit();
		}
		else if (gprreg >= 0)
		{
			recBeginOaknutEmit();
			oakStore64(oakXRegister(gprreg), {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rd_].UD[0]))});
			recEndOaknutEmit();
		}
		else
		{
			recBeginOaknutEmit();
			oakLoad64(OAK_XSCRATCH, {oak::util::X27, hiloOffset});
			oakStore64(OAK_XSCRATCH, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rd_].UD[0]))});
			recEndOaknutEmit();
		}
	}
}

template <int HiloReg, u8 Lane>
static void recMTHILOExact()
{
	const int reg = HiloReg;
	constexpr s64 hiloOffset = HiloReg == XMMGPR_HI ? offsetof(cpuRegistersPack, cpuRegs.HI.UD[Lane]) : offsetof(cpuRegistersPack, cpuRegs.LO.UD[Lane]);
	_eeOnWriteReg(reg, 0);

	const int xmms = EEINST_XMMUSEDTEST(_Rs_) ? _allocGPRtoXMMreg(_Rs_, MODE_READ) : _checkXMMreg(XMMTYPE_GPRREG, _Rs_, MODE_READ);
	const int xmmhilo = EEINST_XMMUSEDTEST(reg) ? _allocGPRtoXMMreg(reg, MODE_READ | MODE_WRITE) : _checkXMMreg(XMMTYPE_GPRREG, reg, MODE_READ | MODE_WRITE);
	if (xmms >= 0)
	{
		const int gprhilo = (xmmhilo < 0 && Lane == 0) ? _allocIfUsedGPRtoX86(reg, MODE_WRITE) : -1;
		recBeginOaknutEmit();
		if (xmmhilo >= 0)
		{
			oakAsm->MOV(oakQRegister(xmmhilo).Delem()[Lane], oakQRegister(xmms).Delem()[0]);
		}
		else
		{
			if (gprhilo >= 0)
			{
				oakAsm->FMOV(oakXRegister(gprhilo), oakDRegister(xmms));
			}
			else {
				oakAsm->FMOV(OAK_XSCRATCH, oakDRegister(xmms));
				oakStore64(OAK_XSCRATCH, {oak::util::X27, hiloOffset});
            }
		}
		recEndOaknutEmit();
	}
	else
	{
		int gprs = _allocIfUsedGPRtoX86(_Rs_, MODE_READ);

		if (xmmhilo >= 0)
		{
			recBeginOaknutEmit();
			if (gprs >= 0)
			{
				oakAsm->INS(oakQRegister(xmmhilo).Delem()[Lane], oakXRegister(gprs));
			}
			else if (GPR_IS_CONST1(_Rs_))
			{
				oakAsm->MOV(OAK_XSCRATCH, g_cpuConstRegs[_Rs_].UD[0]);
				oakAsm->INS(oakQRegister(xmmhilo).Delem()[Lane], OAK_XSCRATCH);
			}
			else
			{
				oakLoad64(OAK_XSCRATCH, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});
				oakAsm->INS(oakQRegister(xmmhilo).Delem()[Lane], OAK_XSCRATCH);
			}
			recEndOaknutEmit();
		}
		else
		{
			// try rename rs -> {hi,lo}
			if (gprs >= 0 && Lane == 0 && _eeTryRenameReg(reg, _Rs_, gprs, -1, 0) >= 0)
				return;

			const int gprreg = (Lane != 0) ? -1 : _allocIfUsedGPRtoX86(reg, MODE_WRITE);
			if (gprreg >= 0)
			{
				recBeginOaknutEmit();
				recMoveGPRtoOakX(oakXRegister(gprreg), _Rs_);
				recEndOaknutEmit();
			}
			else
			{
				// force into a register, since we need to load it to write anyway
				gprs = _allocX86reg(X86TYPE_GPR, _Rs_, MODE_READ);
				recBeginOaknutEmit();
				oakStore64(oakXRegister(gprs), {oak::util::X27, hiloOffset});
				recEndOaknutEmit();
			}
		}
	}
}


static void recMFHI_emit_oaknut()
{
	recMFHILOExact<XMMGPR_HI, 0>();
}

void recMFHI()
{
	recMFHI_emit_oaknut();
	EE::Profiler.EmitOp(eeOpcode::MFHI);
}

static void recMFLO_emit_oaknut()
{
	recMFHILOExact<XMMGPR_LO, 0>();
}

void recMFLO()
{
	recMFLO_emit_oaknut();
	EE::Profiler.EmitOp(eeOpcode::MFLO);
}

static void recMTHI_emit_oaknut()
{
	recMTHILOExact<XMMGPR_HI, 0>();
}

void recMTHI()
{
	recMTHI_emit_oaknut();
	EE::Profiler.EmitOp(eeOpcode::MTHI);
}

static void recMTLO_emit_oaknut()
{
	recMTHILOExact<XMMGPR_LO, 0>();
}

void recMTLO()
{
	recMTLO_emit_oaknut();
	EE::Profiler.EmitOp(eeOpcode::MTLO);
}

static void recMFHI1_emit_oaknut()
{
	recMFHILOExact<XMMGPR_HI, 1>();
}

void recMFHI1()
{
	recMFHI1_emit_oaknut();
	EE::Profiler.EmitOp(eeOpcode::MFHI1);
}

static void recMFLO1_emit_oaknut()
{
	recMFHILOExact<XMMGPR_LO, 1>();
}

void recMFLO1()
{
	recMFLO1_emit_oaknut();
	EE::Profiler.EmitOp(eeOpcode::MFLO1);
}

static void recMTHI1_emit_oaknut()
{
	recMTHILOExact<XMMGPR_HI, 1>();
}

void recMTHI1()
{
	recMTHI1_emit_oaknut();
	EE::Profiler.EmitOp(eeOpcode::MTHI1);
}

static void recMTLO1_emit_oaknut()
{
	recMTHILOExact<XMMGPR_LO, 1>();
}

void recMTLO1()
{
	recMTLO1_emit_oaknut();
	EE::Profiler.EmitOp(eeOpcode::MTLO1);
}

//// MOVZ
// if (rt == 0) then rd <- rs
static void recMOVcc_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0];
}

static void recMOVcc_consts_emit_oaknut(int info, oak::Cond cond)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	recBeginOaknutEmit();

	if (info & PROCESS_EE_T)
		oakAsm->TST(oakXRegister(EEREC_T), oakXRegister(EEREC_T));
	else
	{
		oakLoad64(OAK_XSCRATCH, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UD[0]))});
		oakAsm->CMP(OAK_XSCRATCH, 0);
	}

	oakAsm->MOV(OAK_XSCRATCH2, g_cpuConstRegs[_Rs_].UD[0]);
	oakAsm->CSEL(oakXRegister(EEREC_D), OAK_XSCRATCH2, oakXRegister(EEREC_D), cond);

	recEndOaknutEmit();
}

static void recMOVcc_constt_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	recBeginOaknutEmit();

	if (info & PROCESS_EE_S)
		oakAsm->MOV(oakXRegister(EEREC_D), oakXRegister(EEREC_S));
	else
		oakLoad64(oakXRegister(EEREC_D), {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});

	recEndOaknutEmit();
}

static void recMOVcc_emit_oaknut(int info, oak::Cond cond)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	recBeginOaknutEmit();

	if (info & PROCESS_EE_T)
		oakAsm->TST(oakXRegister(EEREC_T), oakXRegister(EEREC_T));
	else
	{
		oakLoad64(OAK_XSCRATCH, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UD[0]))});
		oakAsm->CMP(OAK_XSCRATCH, 0);
	}

	if (info & PROCESS_EE_S)
		oakAsm->CSEL(oakXRegister(EEREC_D), oakXRegister(EEREC_S), oakXRegister(EEREC_D), cond);
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});
		oakAsm->CSEL(oakXRegister(EEREC_D), OAK_XSCRATCH2, oakXRegister(EEREC_D), cond);
	}

	recEndOaknutEmit();
}

static void recMOVZ_consts(int info)
{
	recMOVcc_consts_emit_oaknut(info, oak::Cond::EQ);
}

static void recMOVZ_constt(int info)
{
	recMOVcc_constt_emit_oaknut(info);
}

static void recMOVZ_(int info)
{
	recMOVcc_emit_oaknut(info, oak::Cond::EQ);
}

void recMOVZ()
{
	if (_Rs_ == _Rd_)
		return;

	if (GPR_IS_CONST1(_Rt_) && g_cpuConstRegs[_Rt_].UD[0] != 0)
		return;

	EE::Profiler.EmitOp(eeOpcode::MOVZtemp);
	// Specify READD here, because we might not write to it, and want to preserve the value.
	eeRecompileCodeRC0(recMOVcc_const, recMOVZ_consts, recMOVZ_constt, recMOVZ_, XMMINFO_READS | XMMINFO_READT | XMMINFO_READD | XMMINFO_WRITED | XMMINFO_NORENAME);
}

//// MOVN
static void recMOVN_consts(int info)
{
	recMOVcc_consts_emit_oaknut(info, oak::Cond::NE);
}

static void recMOVN_constt(int info)
{
	recMOVcc_constt_emit_oaknut(info);
}

static void recMOVN_(int info)
{
	recMOVcc_emit_oaknut(info, oak::Cond::NE);
}

void recMOVN()
{
	if (_Rs_ == _Rd_)
		return;

	if (GPR_IS_CONST1(_Rt_) && g_cpuConstRegs[_Rt_].UD[0] == 0)
		return;

	EE::Profiler.EmitOp(eeOpcode::MOVNtemp);
	eeRecompileCodeRC0(recMOVcc_const, recMOVN_consts, recMOVN_constt, recMOVN_, XMMINFO_READS | XMMINFO_READT | XMMINFO_READD | XMMINFO_WRITED | XMMINFO_NORENAME);
}

#endif

} // namespace R5900::Dynarec::OpcodeImpl
