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
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/

#ifndef ARITHMETICIMM_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_FUNC_DEL(ADDI, _Rt_);
REC_FUNC_DEL(ADDIU, _Rt_);
REC_FUNC_DEL(DADDI, _Rt_);
REC_FUNC_DEL(DADDIU, _Rt_);
REC_FUNC_DEL(ANDI, _Rt_);
REC_FUNC_DEL(ORI, _Rt_);
REC_FUNC_DEL(XORI, _Rt_);

REC_FUNC_DEL(SLTI, _Rt_);
REC_FUNC_DEL(SLTIU, _Rt_);

#else

//// ADDI
static void recADDI_const(void)
{
	g_cpuConstRegs[_Rt_].SD[0] = s64(s32(g_cpuConstRegs[_Rs_].UL[0] + u32(s32(_Imm_))));
}

static void recADDI_emit_oaknut(int info)
{
	using namespace oak::util;

	recBeginOaknutEmit();

	const oak::WReg regt_w = oakWRegister(EEREC_T);
	const oak::XReg regt_x = oakXRegister(EEREC_T);
	if (info & PROCESS_EE_S)
		oakAsm->MOV(regt_w, oakWRegister(EEREC_S));
	else
		oakLoad32(regt_w, {X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UL[0]))});

	oakAsm->MOV(W16, static_cast<u32>(static_cast<s32>(_Imm_)));
	oakAsm->ADD(regt_w, regt_w, W16);
	oakAsm->SXTW(regt_x, regt_w);

	recEndOaknutEmit();
}

static void recADDI_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recADDI_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC1, ADDI, XMMINFO_WRITET | XMMINFO_READS);

////////////////////////////////////////////////////
static void recADDIU_const(void)
{
	g_cpuConstRegs[_Rt_].SD[0] = s64(s32(g_cpuConstRegs[_Rs_].UL[0] + u32(s32(_Imm_))));
}

static void recADDIU_emit_oaknut(int info)
{
	using namespace oak::util;

	recBeginOaknutEmit();

	const oak::WReg regt_w = oakWRegister(EEREC_T);
	const oak::XReg regt_x = oakXRegister(EEREC_T);
	if (info & PROCESS_EE_S)
		oakAsm->MOV(regt_w, oakWRegister(EEREC_S));
	else
		oakLoad32(regt_w, {X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UL[0]))});

	oakAsm->MOV(W16, static_cast<u32>(static_cast<s32>(_Imm_)));
	oakAsm->ADD(regt_w, regt_w, W16);
	oakAsm->SXTW(regt_x, regt_w);

	recEndOaknutEmit();
}

static void recADDIU_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recADDIU_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC1, ADDIU, XMMINFO_WRITET | XMMINFO_READS);

////////////////////////////////////////////////////
static void recDADDI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] + u64(s64(_Imm_));
}

static void recDADDI_emit_oaknut(int info)
{
	using namespace oak::util;

	recBeginOaknutEmit();

	const oak::XReg regt_x = oakXRegister(EEREC_T);
	if (info & PROCESS_EE_S)
		oakAsm->MOV(regt_x, oakXRegister(EEREC_S));
	else
		oakLoad64(regt_x, {X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});

	oakAsm->MOV(X16, static_cast<u64>(static_cast<s64>(_Imm_)));
	oakAsm->ADD(regt_x, regt_x, X16);

	recEndOaknutEmit();
}

static void recDADDI_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recDADDI_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC1, DADDI, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

//// DADDIU
static void recDADDIU_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] + u64(s64(_Imm_));
}

static void recDADDIU_emit_oaknut(int info)
{
	using namespace oak::util;

	recBeginOaknutEmit();

	const oak::XReg regt_x = oakXRegister(EEREC_T);
	if (info & PROCESS_EE_S)
		oakAsm->MOV(regt_x, oakXRegister(EEREC_S));
	else
		oakLoad64(regt_x, {X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});

	oakAsm->MOV(X16, static_cast<u64>(static_cast<s64>(_Imm_)));
	oakAsm->ADD(regt_x, regt_x, X16);

	recEndOaknutEmit();
}

static void recDADDIU_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recDADDIU_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC1, DADDIU, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

//// SLTIU
static void recSLTIU_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] < (u64)(_Imm_);
}

static void recSLTIU_emit_oaknut(int info, int regt)
{
	using namespace oak::util;

	recBeginOaknutEmit();

	const oak::XReg src = OAK_XSCRATCH2;
	if (info & PROCESS_EE_S)
		oakAsm->MOV(src, oakXRegister(EEREC_S));
	else
		oakLoad64(src, {X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});

	oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(static_cast<s64>(_Imm_)));
	oakAsm->CMP(src, OAK_XSCRATCH);
	oakAsm->CSET(oakWRegister(regt), CC);

	recEndOaknutEmit();
}

static void recSLTIU_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	// TODO(Stenzek): this can be made to suck less by turning Rs into a temp and reallocating Rt.
	const int dreg = (_Rt_ == _Rs_) ? _allocX86reg(X86TYPE_TEMP, 0, 0) : EEREC_T;
	recSLTIU_emit_oaknut(info, dreg);

	if (dreg != EEREC_T)
	{
		std::swap(x86regs[dreg], x86regs[EEREC_T]);
		_freeX86reg(EEREC_T);
	}
}

EERECOMPILE_CODEX(eeRecompileCodeRC1, SLTIU, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP | XMMINFO_NORENAME);

//// SLTI
static void recSLTI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].SD[0] < (s64)(_Imm_);
}

static void recSLTI_emit_oaknut(int info, int regt)
{
	using namespace oak::util;

	recBeginOaknutEmit();

	const oak::XReg src = OAK_XSCRATCH2;
	if (info & PROCESS_EE_S)
		oakAsm->MOV(src, oakXRegister(EEREC_S));
	else
		oakLoad64(src, {X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});

	oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(static_cast<s64>(_Imm_)));
	oakAsm->CMP(src, OAK_XSCRATCH);
	oakAsm->CSET(oakWRegister(regt), LT);

	recEndOaknutEmit();
}

static void recSLTI_(int info)
{
	const int dreg = (_Rt_ == _Rs_) ? _allocX86reg(X86TYPE_TEMP, 0, 0) : EEREC_T;
	recSLTI_emit_oaknut(info, dreg);

	if (dreg != EEREC_T)
	{
		std::swap(x86regs[dreg], x86regs[EEREC_T]);
		_freeX86reg(EEREC_T);
	}
}

EERECOMPILE_CODEX(eeRecompileCodeRC1, SLTI, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP | XMMINFO_NORENAME);

//// ANDI
static void recANDI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] & (u64)_ImmU_; // Zero-extended Immediate
}

static void recANDI_emit_oaknut(int info)
{
	using namespace oak::util;

	recBeginOaknutEmit();

	const oak::XReg regt_x = oakXRegister(EEREC_T);
	if (_ImmU_ == 0)
	{
		oakAsm->EOR(oakWRegister(EEREC_T), oakWRegister(EEREC_T), oakWRegister(EEREC_T));
		recEndOaknutEmit();
		return;
	}

	if (info & PROCESS_EE_S)
		oakAsm->MOV(regt_x, oakXRegister(EEREC_S));
	else
		oakLoad64(regt_x, {X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});

	if (oak::detail::encode_bit_imm(static_cast<u64>(_ImmU_)))
		oakAsm->AND(regt_x, regt_x, oak::BitImm64(static_cast<u64>(_ImmU_)));
	else
	{
		oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(_ImmU_));
		oakAsm->AND(regt_x, regt_x, OAK_XSCRATCH);
	}

	recEndOaknutEmit();
}

static void recANDI_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recANDI_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC1, ANDI, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

////////////////////////////////////////////////////
static void recORI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] | (u64)_ImmU_; // Zero-extended Immediate
}

static void recORI_emit_oaknut(int info)
{
	using namespace oak::util;

	recBeginOaknutEmit();

	const oak::XReg regt_x = oakXRegister(EEREC_T);
	if (info & PROCESS_EE_S)
		oakAsm->MOV(regt_x, oakXRegister(EEREC_S));
	else
		oakLoad64(regt_x, {X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});

	if (_ImmU_ != 0)
	{
		if (oak::detail::encode_bit_imm(static_cast<u64>(_ImmU_)))
			oakAsm->ORR(regt_x, regt_x, oak::BitImm64(static_cast<u64>(_ImmU_)));
		else
		{
			oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(_ImmU_));
			oakAsm->ORR(regt_x, regt_x, OAK_XSCRATCH);
		}
	}

	recEndOaknutEmit();
}

static void recORI_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recORI_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC1, ORI, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

////////////////////////////////////////////////////
static void recXORI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] ^ (u64)_ImmU_; // Zero-extended Immediate
}

static void recXORI_emit_oaknut(int info)
{
	using namespace oak::util;

	recBeginOaknutEmit();

	const oak::XReg regt_x = oakXRegister(EEREC_T);
	if (info & PROCESS_EE_S)
		oakAsm->MOV(regt_x, oakXRegister(EEREC_S));
	else
		oakLoad64(regt_x, {X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});

	if (_ImmU_ != 0)
	{
		if (oak::detail::encode_bit_imm(static_cast<u64>(_ImmU_)))
			oakAsm->EOR(regt_x, regt_x, oak::BitImm64(static_cast<u64>(_ImmU_)));
		else
		{
			oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(_ImmU_));
			oakAsm->EOR(regt_x, regt_x, OAK_XSCRATCH);
		}
	}

	recEndOaknutEmit();
}

static void recXORI_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));
	recXORI_emit_oaknut(info);
}

EERECOMPILE_CODEX(eeRecompileCodeRC1, XORI, XMMINFO_WRITET | XMMINFO_READS | XMMINFO_64BITOP);

#endif

} // namespace R5900::Dynarec::OpcodeImpl
