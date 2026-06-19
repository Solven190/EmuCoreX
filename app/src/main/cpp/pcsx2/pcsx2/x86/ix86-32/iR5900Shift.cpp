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
#ifndef SHIFT_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_FUNC_DEL(SLL, _Rd_);
REC_FUNC_DEL(SRL, _Rd_);
REC_FUNC_DEL(SRA, _Rd_);
REC_FUNC_DEL(DSLL, _Rd_);
REC_FUNC_DEL(DSRL, _Rd_);
REC_FUNC_DEL(DSRA, _Rd_);
REC_FUNC_DEL(DSLL32, _Rd_);
REC_FUNC_DEL(DSRL32, _Rd_);
REC_FUNC_DEL(DSRA32, _Rd_);

REC_FUNC_DEL(SLLV, _Rd_);
REC_FUNC_DEL(SRLV, _Rd_);
REC_FUNC_DEL(SRAV, _Rd_);
REC_FUNC_DEL(DSLLV, _Rd_);
REC_FUNC_DEL(DSRLV, _Rd_);
REC_FUNC_DEL(DSRAV, _Rd_);

#else

namespace
{
enum class ShiftOp
{
	LSL,
	LSR,
	ASR
};
} // namespace

static void emitShiftW_oaknut(oak::WReg dst, oak::WReg amount, ShiftOp op)
{
	switch (op)
	{
		case ShiftOp::LSL:
			oakAsm->LSL(dst, dst, amount);
			break;
		case ShiftOp::LSR:
			oakAsm->LSR(dst, dst, amount);
			break;
		case ShiftOp::ASR:
			oakAsm->ASR(dst, dst, amount);
			break;
	}
}

static void emitShiftW_oaknut(oak::WReg dst, int amount, ShiftOp op)
{
	if (amount == 0)
		return;

	switch (op)
	{
		case ShiftOp::LSL:
			oakAsm->LSL(dst, dst, amount);
			break;
		case ShiftOp::LSR:
			oakAsm->LSR(dst, dst, amount);
			break;
		case ShiftOp::ASR:
			oakAsm->ASR(dst, dst, amount);
			break;
	}
}

static void emitShiftX_oaknut(oak::XReg dst, oak::XReg amount, ShiftOp op)
{
	switch (op)
	{
		case ShiftOp::LSL:
			oakAsm->LSL(dst, dst, amount);
			break;
		case ShiftOp::LSR:
			oakAsm->LSR(dst, dst, amount);
			break;
		case ShiftOp::ASR:
			oakAsm->ASR(dst, dst, amount);
			break;
	}
}

static void emitShiftX_oaknut(oak::XReg dst, int amount, ShiftOp op)
{
	if (amount == 0)
		return;

	switch (op)
	{
		case ShiftOp::LSL:
			oakAsm->LSL(dst, dst, amount);
			break;
		case ShiftOp::LSR:
			oakAsm->LSR(dst, dst, amount);
			break;
		case ShiftOp::ASR:
			oakAsm->ASR(dst, dst, amount);
			break;
	}
}

static void loadShiftAmount_oaknut(int info)
{
	if (info & PROCESS_EE_S)
		oakAsm->MOV(oak::util::X17, oakXRegister(EEREC_S));
	else
		oakLoad64(oak::util::X17, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});
}

static void recShift32Imm_emit_oaknut(int info, int amount, ShiftOp op)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recBeginOaknutEmit();

	const oak::WReg regd_w = oakWRegister(EEREC_D);
	if (info & PROCESS_EE_T)
		oakAsm->MOV(regd_w, oakWRegister(EEREC_T));
	else
		oakLoad32(regd_w, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UL[0]))});
	emitShiftW_oaknut(regd_w, amount, op);
	oakAsm->SXTW(oakXRegister(EEREC_D), regd_w);

	recEndOaknutEmit();
}

static void recShift64Imm_emit_oaknut(int info, int amount, ShiftOp op)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recBeginOaknutEmit();

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (info & PROCESS_EE_T)
		oakAsm->MOV(regd_x, oakXRegister(EEREC_T));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UD[0]))});
	emitShiftX_oaknut(regd_x, amount, op);

	recEndOaknutEmit();
}

static void recShift32VarConstT_emit_oaknut(int info, ShiftOp op)
{
	pxAssert(_Rs_ != 0);
	recBeginOaknutEmit();

	loadShiftAmount_oaknut(info);
	const oak::WReg regd_w = oakWRegister(EEREC_D);
	oakAsm->MOV(regd_w, g_cpuConstRegs[_Rt_].UL[0]);
	emitShiftW_oaknut(regd_w, oak::util::W17, op);
	oakAsm->SXTW(oakXRegister(EEREC_D), regd_w);

	recEndOaknutEmit();
}

static void recShift32Var_emit_oaknut(int info, ShiftOp op)
{
	pxAssert(_Rs_ != 0);
	recBeginOaknutEmit();

	loadShiftAmount_oaknut(info);
	const oak::WReg regd_w = oakWRegister(EEREC_D);
	if (info & PROCESS_EE_T)
		oakAsm->MOV(regd_w, oakWRegister(EEREC_T));
	else
		oakLoad32(regd_w, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UL[0]))});
	emitShiftW_oaknut(regd_w, oak::util::W17, op);
	oakAsm->SXTW(oakXRegister(EEREC_D), regd_w);

	recEndOaknutEmit();
}

static void recShift64VarConstT_emit_oaknut(int info, ShiftOp op)
{
	pxAssert(_Rs_ != 0);
	recBeginOaknutEmit();

	loadShiftAmount_oaknut(info);
	const oak::XReg regd_x = oakXRegister(EEREC_D);
	oakAsm->MOV(regd_x, g_cpuConstRegs[_Rt_].UD[0]);
	emitShiftX_oaknut(regd_x, oak::util::X17, op);

	recEndOaknutEmit();
}

static void recShift64Var_emit_oaknut(int info, ShiftOp op)
{
	pxAssert(_Rs_ != 0);
	recBeginOaknutEmit();

	loadShiftAmount_oaknut(info);
	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (info & PROCESS_EE_T)
		oakAsm->MOV(regd_x, oakXRegister(EEREC_T));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UD[0]))});
	emitShiftX_oaknut(regd_x, oak::util::X17, op);

	recEndOaknutEmit();
}

//// SLL
static void recSLL_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] << _Sa_);
}

static void recSLL_emit_oaknut(int info)
{
	recShift32Imm_emit_oaknut(info, _Sa_, ShiftOp::LSL);
}

static void recSLL_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recSLL_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, SLL, XMMINFO_WRITED | XMMINFO_READT);

//// SRL
static void recSRL_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] >> _Sa_);
}

static void recSRL_emit_oaknut(int info)
{
	recShift32Imm_emit_oaknut(info, _Sa_, ShiftOp::LSR);
}

static void recSRL_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recSRL_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, SRL, XMMINFO_WRITED | XMMINFO_READT);

//// SRA
static void recSRA_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].SL[0] >> _Sa_);
}

static void recSRA_emit_oaknut(int info)
{
	recShift32Imm_emit_oaknut(info, _Sa_, ShiftOp::ASR);
}

static void recSRA_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recSRA_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, SRA, XMMINFO_WRITED | XMMINFO_READT);

////////////////////////////////////////////////////
static void recDSLL_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] << _Sa_);
}

static void recDSLL_emit_oaknut(int info)
{
	recShift64Imm_emit_oaknut(info, _Sa_, ShiftOp::LSL);
}

static void recDSLL_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recDSLL_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, DSLL, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

////////////////////////////////////////////////////
static void recDSRL_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] >> _Sa_);
}

static void recDSRL_emit_oaknut(int info)
{
	recShift64Imm_emit_oaknut(info, _Sa_, ShiftOp::LSR);
}

static void recDSRL_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recDSRL_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, DSRL, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

//// DSRA
static void recDSRA_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (u64)(g_cpuConstRegs[_Rt_].SD[0] >> _Sa_);
}

static void recDSRA_emit_oaknut(int info)
{
	recShift64Imm_emit_oaknut(info, _Sa_, ShiftOp::ASR);
}

static void recDSRA_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recDSRA_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, DSRA, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

///// DSLL32
static void recDSLL32_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] << (_Sa_ + 32));
}

static void recDSLL32_emit_oaknut(int info)
{
	recShift64Imm_emit_oaknut(info, _Sa_ + 32, ShiftOp::LSL);
}

static void recDSLL32_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recDSLL32_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, DSLL32, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

//// DSRL32
static void recDSRL32_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] >> (_Sa_ + 32));
}

static void recDSRL32_emit_oaknut(int info)
{
	recShift64Imm_emit_oaknut(info, _Sa_ + 32, ShiftOp::LSR);
}

static void recDSRL32_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recDSRL32_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, DSRL32, XMMINFO_WRITED | XMMINFO_READT);

//// DSRA32
static void recDSRA32_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (u64)(g_cpuConstRegs[_Rt_].SD[0] >> (_Sa_ + 32));
}

static void recDSRA32_emit_oaknut(int info)
{
	recShift64Imm_emit_oaknut(info, _Sa_ + 32, ShiftOp::ASR);
}

static void recDSRA32_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recDSRA32_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC2, DSRA32, XMMINFO_WRITED | XMMINFO_READT | XMMINFO_64BITOP);

/*********************************************************
* Shift arithmetic with variant register shift           *
* Format:  OP rd, rt, rs                                 *
*********************************************************/

//// SLLV
static void recSLLV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] << (g_cpuConstRegs[_Rs_].UL[0] & 0x1f));
}

static void recSLLV_consts_emit_oaknut(int info)
{
	const int sa = g_cpuConstRegs[_Rs_].UL[0] & 0x1f;
	recShift32Imm_emit_oaknut(info, sa, ShiftOp::LSL);
}

static void recSLLV_constt_emit_oaknut(int info)
{
	recShift32VarConstT_emit_oaknut(info, ShiftOp::LSL);
}

static void recSLLV_emit_oaknut(int info)
{
	recShift32Var_emit_oaknut(info, ShiftOp::LSL);
}

static void recSLLV_consts(int info)
{
	recSLLV_consts_emit_oaknut(info);
}

static void recSLLV_constt(int info)
{
	recSLLV_constt_emit_oaknut(info);
}

static void recSLLV_(int info)
{
	recSLLV_emit_oaknut(info);
}

EERECOMPILE_CODERC0(SLLV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// SRLV
static void recSRLV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x1f));
}

static void recSRLV_consts_emit_oaknut(int info)
{
	const int sa = g_cpuConstRegs[_Rs_].UL[0] & 0x1f;
	recShift32Imm_emit_oaknut(info, sa, ShiftOp::LSR);
}

static void recSRLV_constt_emit_oaknut(int info)
{
	recShift32VarConstT_emit_oaknut(info, ShiftOp::LSR);
}

static void recSRLV_emit_oaknut(int info)
{
	recShift32Var_emit_oaknut(info, ShiftOp::LSR);
}

static void recSRLV_consts(int info)
{
	recSRLV_consts_emit_oaknut(info);
}

static void recSRLV_constt(int info)
{
	recSRLV_constt_emit_oaknut(info);
}

static void recSRLV_(int info)
{
	recSRLV_emit_oaknut(info);
}

EERECOMPILE_CODERC0(SRLV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// SRAV
static void recSRAV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].SL[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x1f));
}

static void recSRAV_consts_emit_oaknut(int info)
{
	const int sa = g_cpuConstRegs[_Rs_].UL[0] & 0x1f;
	recShift32Imm_emit_oaknut(info, sa, ShiftOp::ASR);
}

static void recSRAV_constt_emit_oaknut(int info)
{
	recShift32VarConstT_emit_oaknut(info, ShiftOp::ASR);
}

static void recSRAV_emit_oaknut(int info)
{
	recShift32Var_emit_oaknut(info, ShiftOp::ASR);
}

static void recSRAV_consts(int info)
{
	recSRAV_consts_emit_oaknut(info);
}

static void recSRAV_constt(int info)
{
	recSRAV_constt_emit_oaknut(info);
}

static void recSRAV_(int info)
{
	recSRAV_emit_oaknut(info);
}

EERECOMPILE_CODERC0(SRAV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// DSLLV
static void recDSLLV_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] << (g_cpuConstRegs[_Rs_].UL[0] & 0x3f));
}

static void recDSLLV_consts_emit_oaknut(int info)
{
	const int sa = g_cpuConstRegs[_Rs_].UL[0] & 0x3f;
	recShift64Imm_emit_oaknut(info, sa, ShiftOp::LSL);
}

static void recDSLLV_constt_emit_oaknut(int info)
{
	recShift64VarConstT_emit_oaknut(info, ShiftOp::LSL);
}

static void recDSLLV_emit_oaknut(int info)
{
	recShift64Var_emit_oaknut(info, ShiftOp::LSL);
}

static void recDSLLV_consts(int info)
{
	recDSLLV_consts_emit_oaknut(info);
}

static void recDSLLV_constt(int info)
{
	recDSLLV_constt_emit_oaknut(info);
}

static void recDSLLV_(int info)
{
	recDSLLV_emit_oaknut(info);
}

EERECOMPILE_CODERC0(DSLLV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

//// DSRLV
static void recDSRLV_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x3f));
}

static void recDSRLV_consts_emit_oaknut(int info)
{
	const int sa = g_cpuConstRegs[_Rs_].UL[0] & 0x3f;
	recShift64Imm_emit_oaknut(info, sa, ShiftOp::LSR);
}

static void recDSRLV_constt_emit_oaknut(int info)
{
	recShift64VarConstT_emit_oaknut(info, ShiftOp::LSR);
}

static void recDSRLV_emit_oaknut(int info)
{
	recShift64Var_emit_oaknut(info, ShiftOp::LSR);
}

static void recDSRLV_consts(int info)
{
	recDSRLV_consts_emit_oaknut(info);
}

static void recDSRLV_constt(int info)
{
	recDSRLV_constt_emit_oaknut(info);
}

static void recDSRLV_(int info)
{
	recDSRLV_emit_oaknut(info);
}

EERECOMPILE_CODERC0(DSRLV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

//// DSRAV
static void recDSRAV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s64)(g_cpuConstRegs[_Rt_].SD[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x3f));
}

static void recDSRAV_consts_emit_oaknut(int info)
{
	const int sa = g_cpuConstRegs[_Rs_].UL[0] & 0x3f;
	recShift64Imm_emit_oaknut(info, sa, ShiftOp::ASR);
}

static void recDSRAV_constt_emit_oaknut(int info)
{
	recShift64VarConstT_emit_oaknut(info, ShiftOp::ASR);
}

static void recDSRAV_emit_oaknut(int info)
{
	recShift64Var_emit_oaknut(info, ShiftOp::ASR);
}

static void recDSRAV_consts(int info)
{
	recDSRAV_consts_emit_oaknut(info);
}

static void recDSRAV_constt(int info)
{
	recDSRAV_constt_emit_oaknut(info);
}

static void recDSRAV_(int info)
{
	recDSRAV_emit_oaknut(info);
}

EERECOMPILE_CODERC0(DSRAV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

#endif

} // namespace R5900::Dynarec::OpcodeImpl
