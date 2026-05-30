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
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/

// TODO: overflow checks

#ifndef ARITHMETIC_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_FUNC_DEL(ADD, _Rd_);
REC_FUNC_DEL(ADDU, _Rd_);
REC_FUNC_DEL(DADD, _Rd_);
REC_FUNC_DEL(DADDU, _Rd_);
REC_FUNC_DEL(SUB, _Rd_);
REC_FUNC_DEL(SUBU, _Rd_);
REC_FUNC_DEL(DSUB, _Rd_);
REC_FUNC_DEL(DSUBU, _Rd_);
REC_FUNC_DEL(AND, _Rd_);
REC_FUNC_DEL(OR, _Rd_);
REC_FUNC_DEL(XOR, _Rd_);
REC_FUNC_DEL(NOR, _Rd_);
REC_FUNC_DEL(SLT, _Rd_);
REC_FUNC_DEL(SLTU, _Rd_);

#else

//// ADD
static void recADD_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = s64(s32(g_cpuConstRegs[_Rs_].UL[0] + g_cpuConstRegs[_Rt_].UL[0]));
}

// s is constant
static void recADD_consts_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s32 cval = g_cpuConstRegs[_Rs_].SL[0];
	recBeginOaknutEmit();

	const oak::WReg regd_w = oakWRegister(EEREC_D);
	if (info & PROCESS_EE_T)
		oakAsm->MOV(regd_w, oakWRegister(EEREC_T));
	else
		oakLoad32(regd_w, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UL[0]))});

	if (cval != 0)
	{
		oakAsm->MOV(OAK_WSCRATCH, static_cast<u32>(cval));
		oakAsm->ADD(regd_w, regd_w, OAK_WSCRATCH);
	}
	oakAsm->SXTW(oakXRegister(EEREC_D), regd_w);

	recEndOaknutEmit();
}

static void recADD_consts(int info)
{
	recADD_consts_emit_oaknut(info);
}

// t is constant
static void recADD_constt_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s32 cval = g_cpuConstRegs[_Rt_].SL[0];
	recBeginOaknutEmit();

	const oak::WReg regd_w = oakWRegister(EEREC_D);
	if (info & PROCESS_EE_S)
		oakAsm->MOV(regd_w, oakWRegister(EEREC_S));
	else
		oakLoad32(regd_w, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UL[0]))});

	if (cval != 0)
	{
		oakAsm->MOV(OAK_WSCRATCH, static_cast<u32>(cval));
		oakAsm->ADD(regd_w, regd_w, OAK_WSCRATCH);
	}
	oakAsm->SXTW(oakXRegister(EEREC_D), regd_w);

	recEndOaknutEmit();
}

static void recADD_constt(int info)
{
	recADD_constt_emit_oaknut(info);
}

// nothing is constant
static void recADD_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	u32 rs = _Rs_, rt = _Rt_;
	int regs = (info & PROCESS_EE_S) ? EEREC_S : -1;
	int regt = (info & PROCESS_EE_T) ? EEREC_T : -1;
	if (_Rd_ == _Rt_)
	{
		std::swap(rs, rt);
		std::swap(regs, regt);
	}

	recBeginOaknutEmit();

	const oak::WReg regd_w = oakWRegister(EEREC_D);
	if (regs >= 0)
		oakAsm->MOV(regd_w, oakWRegister(regs));
	else
		oakLoad32(regd_w, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UL[0]) + rs * sizeof(GPR_reg))});

	if (regt >= 0)
		oakAsm->ADD(regd_w, regd_w, oakWRegister(regt));
	else
	{
		oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UL[0]) + rt * sizeof(GPR_reg))});
		oakAsm->ADD(regd_w, regd_w, OAK_WSCRATCH2);
	}
	oakAsm->SXTW(oakXRegister(EEREC_D), regd_w);

	recEndOaknutEmit();
}

static void recADD_(int info)
{
	recADD_emit_oaknut(info);
}

EERECOMPILE_CODERC0(ADD, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

//// ADDU
static void recADDU_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = s64(s32(g_cpuConstRegs[_Rs_].UL[0] + g_cpuConstRegs[_Rt_].UL[0]));
}

static void recADDU_consts_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s32 cval = g_cpuConstRegs[_Rs_].SL[0];
	recBeginOaknutEmit();

	const oak::WReg regd_w = oakWRegister(EEREC_D);
	if (info & PROCESS_EE_T)
		oakAsm->MOV(regd_w, oakWRegister(EEREC_T));
	else
		oakLoad32(regd_w, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UL[0]))});

	if (cval != 0)
	{
		oakAsm->MOV(OAK_WSCRATCH, static_cast<u32>(cval));
		oakAsm->ADD(regd_w, regd_w, OAK_WSCRATCH);
	}
	oakAsm->SXTW(oakXRegister(EEREC_D), regd_w);

	recEndOaknutEmit();
}

static void recADDU_consts(int info)
{
	recADDU_consts_emit_oaknut(info);
}

static void recADDU_constt_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s32 cval = g_cpuConstRegs[_Rt_].SL[0];
	recBeginOaknutEmit();

	const oak::WReg regd_w = oakWRegister(EEREC_D);
	if (info & PROCESS_EE_S)
		oakAsm->MOV(regd_w, oakWRegister(EEREC_S));
	else
		oakLoad32(regd_w, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UL[0]))});

	if (cval != 0)
	{
		oakAsm->MOV(OAK_WSCRATCH, static_cast<u32>(cval));
		oakAsm->ADD(regd_w, regd_w, OAK_WSCRATCH);
	}
	oakAsm->SXTW(oakXRegister(EEREC_D), regd_w);

	recEndOaknutEmit();
}

static void recADDU_constt(int info)
{
	recADDU_constt_emit_oaknut(info);
}

static void recADDU_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	u32 rs = _Rs_, rt = _Rt_;
	int regs = (info & PROCESS_EE_S) ? EEREC_S : -1;
	int regt = (info & PROCESS_EE_T) ? EEREC_T : -1;
	if (_Rd_ == _Rt_)
	{
		std::swap(rs, rt);
		std::swap(regs, regt);
	}

	recBeginOaknutEmit();

	const oak::WReg regd_w = oakWRegister(EEREC_D);
	if (regs >= 0)
		oakAsm->MOV(regd_w, oakWRegister(regs));
	else
		oakLoad32(regd_w, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UL[0]) + rs * sizeof(GPR_reg))});

	if (regt >= 0)
		oakAsm->ADD(regd_w, regd_w, oakWRegister(regt));
	else
	{
		oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UL[0]) + rt * sizeof(GPR_reg))});
		oakAsm->ADD(regd_w, regd_w, OAK_WSCRATCH2);
	}
	oakAsm->SXTW(oakXRegister(EEREC_D), regd_w);

	recEndOaknutEmit();
}

static void recADDU_(int info)
{
	recADDU_emit_oaknut(info);
}

EERECOMPILE_CODERC0(ADDU, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

//// DADD
void recDADD_const(void)
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] + g_cpuConstRegs[_Rt_].UD[0];
}

// s is constant
static void recDADD_consts_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 cval = g_cpuConstRegs[_Rs_].SD[0];
	recBeginOaknutEmit();

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (info & PROCESS_EE_T)
		oakAsm->MOV(regd_x, oakXRegister(EEREC_T));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UD[0]))});

	if (cval != 0)
	{
		oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(cval));
		oakAsm->ADD(regd_x, regd_x, OAK_XSCRATCH);
	}

	recEndOaknutEmit();
}

static void recDADD_consts(int info)
{
	recDADD_consts_emit_oaknut(info);
}

// t is constant
static void recDADD_constt_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 cval = g_cpuConstRegs[_Rt_].SD[0];
	recBeginOaknutEmit();

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (info & PROCESS_EE_S)
		oakAsm->MOV(regd_x, oakXRegister(EEREC_S));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});

	if (cval != 0)
	{
		oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(cval));
		oakAsm->ADD(regd_x, regd_x, OAK_XSCRATCH);
	}

	recEndOaknutEmit();
}

static void recDADD_constt(int info)
{
	recDADD_constt_emit_oaknut(info);
}

// nothing is constant
static void recDADD_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	u32 rs = _Rs_, rt = _Rt_;
	int regs = (info & PROCESS_EE_S) ? EEREC_S : -1;
	int regt = (info & PROCESS_EE_T) ? EEREC_T : -1;
	if (_Rd_ == _Rt_)
	{
		std::swap(rs, rt);
		std::swap(regs, regt);
	}

	recBeginOaknutEmit();

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (regs >= 0)
		oakAsm->MOV(regd_x, oakXRegister(regs));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + rs * sizeof(GPR_reg))});

	if (regt >= 0)
		oakAsm->ADD(regd_x, regd_x, oakXRegister(regt));
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + rt * sizeof(GPR_reg))});
		oakAsm->ADD(regd_x, regd_x, OAK_XSCRATCH2);
	}

	recEndOaknutEmit();
}

static void recDADD_(int info)
{
	recDADD_emit_oaknut(info);
}

EERECOMPILE_CODERC0(DADD, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

//// DADDU
void recDADDU_const(void)
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] + g_cpuConstRegs[_Rt_].UD[0];
}

static void recDADDU_consts_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 cval = g_cpuConstRegs[_Rs_].SD[0];
	recBeginOaknutEmit();

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (info & PROCESS_EE_T)
		oakAsm->MOV(regd_x, oakXRegister(EEREC_T));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UD[0]))});

	if (cval != 0)
	{
		oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(cval));
		oakAsm->ADD(regd_x, regd_x, OAK_XSCRATCH);
	}

	recEndOaknutEmit();
}

static void recDADDU_consts(int info)
{
	recDADDU_consts_emit_oaknut(info);
}

static void recDADDU_constt_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 cval = g_cpuConstRegs[_Rt_].SD[0];
	recBeginOaknutEmit();

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (info & PROCESS_EE_S)
		oakAsm->MOV(regd_x, oakXRegister(EEREC_S));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});

	if (cval != 0)
	{
		oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(cval));
		oakAsm->ADD(regd_x, regd_x, OAK_XSCRATCH);
	}

	recEndOaknutEmit();
}

static void recDADDU_constt(int info)
{
	recDADDU_constt_emit_oaknut(info);
}

static void recDADDU_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	u32 rs = _Rs_, rt = _Rt_;
	int regs = (info & PROCESS_EE_S) ? EEREC_S : -1;
	int regt = (info & PROCESS_EE_T) ? EEREC_T : -1;
	if (_Rd_ == _Rt_)
	{
		std::swap(rs, rt);
		std::swap(regs, regt);
	}

	recBeginOaknutEmit();

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (regs >= 0)
		oakAsm->MOV(regd_x, oakXRegister(regs));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + rs * sizeof(GPR_reg))});

	if (regt >= 0)
		oakAsm->ADD(regd_x, regd_x, oakXRegister(regt));
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + rt * sizeof(GPR_reg))});
		oakAsm->ADD(regd_x, regd_x, OAK_XSCRATCH2);
	}

	recEndOaknutEmit();
}

static void recDADDU_(int info)
{
	recDADDU_emit_oaknut(info);
}

EERECOMPILE_CODERC0(DADDU, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT | XMMINFO_64BITOP);

//// SUB

static void recSUB_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = s64(s32(g_cpuConstRegs[_Rs_].UL[0] - g_cpuConstRegs[_Rt_].UL[0]));
}

static void recSUB_consts_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s32 sval = g_cpuConstRegs[_Rs_].SL[0];
	recBeginOaknutEmit();

	oakAsm->MOV(OAK_WSCRATCH, static_cast<u32>(sval));
	if (info & PROCESS_EE_T)
		oakAsm->SUB(OAK_WSCRATCH, OAK_WSCRATCH, oakWRegister(EEREC_T));
	else
	{
		oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].SL[0]))});
		oakAsm->SUB(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	}
	oakAsm->SXTW(oakXRegister(EEREC_D), OAK_WSCRATCH);

	recEndOaknutEmit();
}

static void recSUB_consts(int info)
{
	recSUB_consts_emit_oaknut(info);
}

static void recSUB_constt_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s32 tval = g_cpuConstRegs[_Rt_].SL[0];
	recBeginOaknutEmit();

	const oak::WReg regd_w = oakWRegister(EEREC_D);
	if (info & PROCESS_EE_S)
		oakAsm->MOV(regd_w, oakWRegister(EEREC_S));
	else
		oakLoad32(regd_w, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UL[0]))});

	if (tval != 0)
	{
		oakAsm->MOV(OAK_WSCRATCH, static_cast<u32>(tval));
		oakAsm->SUB(regd_w, regd_w, OAK_WSCRATCH);
	}
	oakAsm->SXTW(oakXRegister(EEREC_D), regd_w);

	recEndOaknutEmit();
}

static void recSUB_constt(int info)
{
	recSUB_constt_emit_oaknut(info);
}

static void recSUB_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	recBeginOaknutEmit();

	if (_Rs_ == _Rt_)
	{
		oakAsm->EOR(oakWRegister(EEREC_D), oakWRegister(EEREC_D), oakWRegister(EEREC_D));
		recEndOaknutEmit();
		return;
	}

	const bool t_in_host = (info & PROCESS_EE_T) != 0;
	const bool d_aliases_t = t_in_host && EEREC_D == EEREC_T;
	const oak::WReg result_w = d_aliases_t ? OAK_WSCRATCH : oakWRegister(EEREC_D);

	if (info & PROCESS_EE_S)
		oakAsm->MOV(result_w, oakWRegister(EEREC_S));
	else
		oakLoad32(result_w, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UL[0]))});

	if (t_in_host)
		oakAsm->SUB(result_w, result_w, oakWRegister(EEREC_T));
	else
	{
		oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UL[0]))});
		oakAsm->SUB(result_w, result_w, OAK_WSCRATCH2);
	}
	oakAsm->SXTW(oakXRegister(EEREC_D), result_w);

	recEndOaknutEmit();
}

static void recSUB_(int info)
{
	recSUB_emit_oaknut(info);
}

EERECOMPILE_CODERC0(SUB, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// SUBU
static void recSUBU_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = s64(s32(g_cpuConstRegs[_Rs_].UL[0] - g_cpuConstRegs[_Rt_].UL[0]));
}

static void recSUBU_consts_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s32 sval = g_cpuConstRegs[_Rs_].SL[0];
	recBeginOaknutEmit();

	oakAsm->MOV(OAK_WSCRATCH, static_cast<u32>(sval));
	if (info & PROCESS_EE_T)
		oakAsm->SUB(OAK_WSCRATCH, OAK_WSCRATCH, oakWRegister(EEREC_T));
	else
	{
		oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].SL[0]))});
		oakAsm->SUB(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	}
	oakAsm->SXTW(oakXRegister(EEREC_D), OAK_WSCRATCH);

	recEndOaknutEmit();
}

static void recSUBU_consts(int info)
{
	recSUBU_consts_emit_oaknut(info);
}

static void recSUBU_constt_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s32 tval = g_cpuConstRegs[_Rt_].SL[0];
	recBeginOaknutEmit();

	const oak::WReg regd_w = oakWRegister(EEREC_D);
	if (info & PROCESS_EE_S)
		oakAsm->MOV(regd_w, oakWRegister(EEREC_S));
	else
		oakLoad32(regd_w, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UL[0]))});

	if (tval != 0)
	{
		oakAsm->MOV(OAK_WSCRATCH, static_cast<u32>(tval));
		oakAsm->SUB(regd_w, regd_w, OAK_WSCRATCH);
	}
	oakAsm->SXTW(oakXRegister(EEREC_D), regd_w);

	recEndOaknutEmit();
}

static void recSUBU_constt(int info)
{
	recSUBU_constt_emit_oaknut(info);
}

static void recSUBU_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	recBeginOaknutEmit();

	if (_Rs_ == _Rt_)
	{
		oakAsm->EOR(oakWRegister(EEREC_D), oakWRegister(EEREC_D), oakWRegister(EEREC_D));
		recEndOaknutEmit();
		return;
	}

	const bool t_in_host = (info & PROCESS_EE_T) != 0;
	const bool d_aliases_t = t_in_host && EEREC_D == EEREC_T;
	const oak::WReg result_w = d_aliases_t ? OAK_WSCRATCH : oakWRegister(EEREC_D);

	if (info & PROCESS_EE_S)
		oakAsm->MOV(result_w, oakWRegister(EEREC_S));
	else
		oakLoad32(result_w, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UL[0]))});

	if (t_in_host)
		oakAsm->SUB(result_w, result_w, oakWRegister(EEREC_T));
	else
	{
		oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UL[0]))});
		oakAsm->SUB(result_w, result_w, OAK_WSCRATCH2);
	}
	oakAsm->SXTW(oakXRegister(EEREC_D), result_w);

	recEndOaknutEmit();
}

static void recSUBU_(int info)
{
	recSUBU_emit_oaknut(info);
}

EERECOMPILE_CODERC0(SUBU, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// DSUB
static void recDSUB_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] - g_cpuConstRegs[_Rt_].UD[0];
}

static void recDSUB_consts_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 sval = g_cpuConstRegs[_Rs_].SD[0];
	recBeginOaknutEmit();

	const bool t_in_host = (info & PROCESS_EE_T) != 0;
	const bool d_aliases_t = t_in_host && EEREC_D == EEREC_T;
	const oak::XReg result_x = d_aliases_t ? OAK_XSCRATCH : oakXRegister(EEREC_D);

	oakAsm->MOV(result_x, static_cast<u64>(sval));
	if (t_in_host)
		oakAsm->SUB(result_x, result_x, oakXRegister(EEREC_T));
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].SD[0]))});
		oakAsm->SUB(result_x, result_x, OAK_XSCRATCH2);
	}

	if (d_aliases_t)
		oakAsm->MOV(oakXRegister(EEREC_D), result_x);

	recEndOaknutEmit();
}

static void recDSUB_consts(int info)
{
	recDSUB_consts_emit_oaknut(info);
}

static void recDSUB_constt_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 tval = g_cpuConstRegs[_Rt_].SD[0];
	recBeginOaknutEmit();

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (info & PROCESS_EE_S)
		oakAsm->MOV(regd_x, oakXRegister(EEREC_S));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});

	if (tval != 0)
	{
		oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(tval));
		oakAsm->SUB(regd_x, regd_x, OAK_XSCRATCH);
	}

	recEndOaknutEmit();
}

static void recDSUB_constt(int info)
{
	recDSUB_constt_emit_oaknut(info);
}

static void recDSUB_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	recBeginOaknutEmit();

	if (_Rs_ == _Rt_)
	{
		oakAsm->EOR(oakWRegister(EEREC_D), oakWRegister(EEREC_D), oakWRegister(EEREC_D));
		recEndOaknutEmit();
		return;
	}

	const bool t_in_host = (info & PROCESS_EE_T) != 0;
	const bool d_aliases_t = t_in_host && EEREC_D == EEREC_T;
	const oak::XReg result_x = d_aliases_t ? OAK_XSCRATCH : oakXRegister(EEREC_D);

	if (info & PROCESS_EE_S)
		oakAsm->MOV(result_x, oakXRegister(EEREC_S));
	else
		oakLoad64(result_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});

	if (t_in_host)
		oakAsm->SUB(result_x, result_x, oakXRegister(EEREC_T));
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UD[0]))});
		oakAsm->SUB(result_x, result_x, OAK_XSCRATCH2);
	}

	if (d_aliases_t)
		oakAsm->MOV(oakXRegister(EEREC_D), result_x);

	recEndOaknutEmit();
}

static void recDSUB_(int info)
{
	recDSUB_emit_oaknut(info);
}

EERECOMPILE_CODERC0(DSUB, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

//// DSUBU
static void recDSUBU_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] - g_cpuConstRegs[_Rt_].UD[0];
}

static void recDSUBU_consts_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 sval = g_cpuConstRegs[_Rs_].SD[0];
	recBeginOaknutEmit();

	const bool t_in_host = (info & PROCESS_EE_T) != 0;
	const bool d_aliases_t = t_in_host && EEREC_D == EEREC_T;
	const oak::XReg result_x = d_aliases_t ? OAK_XSCRATCH : oakXRegister(EEREC_D);

	oakAsm->MOV(result_x, static_cast<u64>(sval));
	if (t_in_host)
		oakAsm->SUB(result_x, result_x, oakXRegister(EEREC_T));
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].SD[0]))});
		oakAsm->SUB(result_x, result_x, OAK_XSCRATCH2);
	}

	if (d_aliases_t)
		oakAsm->MOV(oakXRegister(EEREC_D), result_x);

	recEndOaknutEmit();
}

static void recDSUBU_consts(int info)
{
	recDSUBU_consts_emit_oaknut(info);
}

static void recDSUBU_constt_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 tval = g_cpuConstRegs[_Rt_].SD[0];
	recBeginOaknutEmit();

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (info & PROCESS_EE_S)
		oakAsm->MOV(regd_x, oakXRegister(EEREC_S));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});

	if (tval != 0)
	{
		oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(tval));
		oakAsm->SUB(regd_x, regd_x, OAK_XSCRATCH);
	}

	recEndOaknutEmit();
}

static void recDSUBU_constt(int info)
{
	recDSUBU_constt_emit_oaknut(info);
}

static void recDSUBU_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	recBeginOaknutEmit();

	if (_Rs_ == _Rt_)
	{
		oakAsm->EOR(oakWRegister(EEREC_D), oakWRegister(EEREC_D), oakWRegister(EEREC_D));
		recEndOaknutEmit();
		return;
	}

	const bool t_in_host = (info & PROCESS_EE_T) != 0;
	const bool d_aliases_t = t_in_host && EEREC_D == EEREC_T;
	const oak::XReg result_x = d_aliases_t ? OAK_XSCRATCH : oakXRegister(EEREC_D);

	if (info & PROCESS_EE_S)
		oakAsm->MOV(result_x, oakXRegister(EEREC_S));
	else
		oakLoad64(result_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});

	if (t_in_host)
		oakAsm->SUB(result_x, result_x, oakXRegister(EEREC_T));
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UD[0]))});
		oakAsm->SUB(result_x, result_x, OAK_XSCRATCH2);
	}

	if (d_aliases_t)
		oakAsm->MOV(oakXRegister(EEREC_D), result_x);

	recEndOaknutEmit();
}

static void recDSUBU_(int info)
{
	recDSUBU_emit_oaknut(info);
}

EERECOMPILE_CODERC0(DSUBU, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

//// AND
static void recAND_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] & g_cpuConstRegs[_Rt_].UD[0];
}

static void recAND_constv_emit_oaknut(int info, int creg, u32 vreg, int regv)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const GPR_reg64 cval = g_cpuConstRegs[creg];
	recBeginOaknutEmit();

	if (cval.SD[0] == 0)
	{
		oakAsm->MOV(oakXRegister(EEREC_D), 0);
		recEndOaknutEmit();
		return;
	}

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (regv >= 0)
		oakAsm->MOV(regd_x, oakXRegister(regv));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + vreg * sizeof(GPR_reg))});

	if (cval.SD[0] != -1)
	{
		if (oak::detail::encode_bit_imm(cval.UD[0]))
			oakAsm->AND(regd_x, regd_x, oak::BitImm64(cval.UD[0]));
		else
		{
			oakAsm->MOV(OAK_XSCRATCH, cval.UD[0]);
			oakAsm->AND(regd_x, regd_x, OAK_XSCRATCH);
		}
	}

	recEndOaknutEmit();
}

static void recAND_constv(int info, int creg, u32 vreg, int regv)
{
	recAND_constv_emit_oaknut(info, creg, vreg, regv);
}

static void recAND_consts(int info)
{
	recAND_constv(info, _Rs_, _Rt_, (info & PROCESS_EE_T) ? EEREC_T : -1);
}

static void recAND_constt(int info)
{
	recAND_constv(info, _Rt_, _Rs_, (info & PROCESS_EE_S) ? EEREC_S : -1);
}

static void recAND_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	u32 rs = _Rs_, rt = _Rt_;
	int regs = (info & PROCESS_EE_S) ? EEREC_S : -1;
	int regt = (info & PROCESS_EE_T) ? EEREC_T : -1;
	if (_Rd_ == _Rt_)
	{
		std::swap(rs, rt);
		std::swap(regs, regt);
	}

	recBeginOaknutEmit();

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (regs >= 0)
		oakAsm->MOV(regd_x, oakXRegister(regs));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + rs * sizeof(GPR_reg))});

	if (regt >= 0)
		oakAsm->AND(regd_x, regd_x, oakXRegister(regt));
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + rt * sizeof(GPR_reg))});
		oakAsm->AND(regd_x, regd_x, OAK_XSCRATCH2);
	}

	recEndOaknutEmit();
}

static void recAND_(int info)
{
	recAND_emit_oaknut(info);
}

EERECOMPILE_CODERC0(AND, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

//// OR
static void recOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] | g_cpuConstRegs[_Rt_].UD[0];
}

static void recOR_constv_emit_oaknut(int info, int creg, u32 vreg, int regv)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const GPR_reg64 cval = g_cpuConstRegs[creg];
	recBeginOaknutEmit();

	if (cval.SD[0] == -1)
	{
		oakAsm->MOV(oakXRegister(EEREC_D), cval.UD[0]);
		recEndOaknutEmit();
		return;
	}

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (regv >= 0)
		oakAsm->MOV(regd_x, oakXRegister(regv));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + vreg * sizeof(GPR_reg))});

	if (cval.SD[0] != 0)
	{
		if (oak::detail::encode_bit_imm(cval.UD[0]))
			oakAsm->ORR(regd_x, regd_x, oak::BitImm64(cval.UD[0]));
		else
		{
			oakAsm->MOV(OAK_XSCRATCH, cval.UD[0]);
			oakAsm->ORR(regd_x, regd_x, OAK_XSCRATCH);
		}
	}

	recEndOaknutEmit();
}

static void recOR_constv(int info, int creg, u32 vreg, int regv)
{
	recOR_constv_emit_oaknut(info, creg, vreg, regv);
}

static void recOR_consts(int info)
{
	recOR_constv(info, _Rs_, _Rt_, (info & PROCESS_EE_T) ? EEREC_T : -1);
}

static void recOR_constt(int info)
{
	recOR_constv(info, _Rt_, _Rs_, (info & PROCESS_EE_S) ? EEREC_S : -1);
}

static void recOR_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	u32 rs = _Rs_, rt = _Rt_;
	int regs = (info & PROCESS_EE_S) ? EEREC_S : -1;
	int regt = (info & PROCESS_EE_T) ? EEREC_T : -1;
	if (_Rd_ == _Rt_)
	{
		std::swap(rs, rt);
		std::swap(regs, regt);
	}

	recBeginOaknutEmit();

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (regs >= 0)
		oakAsm->MOV(regd_x, oakXRegister(regs));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + rs * sizeof(GPR_reg))});

	if (regt >= 0)
		oakAsm->ORR(regd_x, regd_x, oakXRegister(regt));
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + rt * sizeof(GPR_reg))});
		oakAsm->ORR(regd_x, regd_x, OAK_XSCRATCH2);
	}

	recEndOaknutEmit();
}

static void recOR_(int info)
{
	recOR_emit_oaknut(info);
}

EERECOMPILE_CODERC0(OR, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

//// XOR
static void recXOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] ^ g_cpuConstRegs[_Rt_].UD[0];
}

static void recXOR_constv_emit_oaknut(int info, int creg, u32 vreg, int regv)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const GPR_reg64 cval = g_cpuConstRegs[creg];
	recBeginOaknutEmit();

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (regv >= 0)
		oakAsm->MOV(regd_x, oakXRegister(regv));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + vreg * sizeof(GPR_reg))});

	if (cval.SD[0] != 0)
	{
		if (oak::detail::encode_bit_imm(cval.UD[0]))
			oakAsm->EOR(regd_x, regd_x, oak::BitImm64(cval.UD[0]));
		else
		{
			oakAsm->MOV(OAK_XSCRATCH, cval.UD[0]);
			oakAsm->EOR(regd_x, regd_x, OAK_XSCRATCH);
		}
	}

	recEndOaknutEmit();
}

static void recXOR_constv(int info, int creg, u32 vreg, int regv)
{
	recXOR_constv_emit_oaknut(info, creg, vreg, regv);
}

static void recXOR_consts(int info)
{
	recXOR_constv(info, _Rs_, _Rt_, (info & PROCESS_EE_T) ? EEREC_T : -1);
}

static void recXOR_constt(int info)
{
	recXOR_constv(info, _Rt_, _Rs_, (info & PROCESS_EE_S) ? EEREC_S : -1);
}

static void recXOR_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	u32 rs = _Rs_, rt = _Rt_;
	int regs = (info & PROCESS_EE_S) ? EEREC_S : -1;
	int regt = (info & PROCESS_EE_T) ? EEREC_T : -1;
	if (_Rd_ == _Rt_)
	{
		std::swap(rs, rt);
		std::swap(regs, regt);
	}

	if (rs == rt)
	{
		recBeginOaknutEmit();
		oakAsm->EOR(oakWRegister(EEREC_D), oakWRegister(EEREC_D), oakWRegister(EEREC_D));
		recEndOaknutEmit();
		return;
	}

	recBeginOaknutEmit();

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (regs >= 0)
		oakAsm->MOV(regd_x, oakXRegister(regs));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + rs * sizeof(GPR_reg))});

	if (regt >= 0)
		oakAsm->EOR(regd_x, regd_x, oakXRegister(regt));
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + rt * sizeof(GPR_reg))});
		oakAsm->EOR(regd_x, regd_x, OAK_XSCRATCH2);
	}

	recEndOaknutEmit();
}

static void recXOR_(int info)
{
	recXOR_emit_oaknut(info);
}

EERECOMPILE_CODERC0(XOR, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

//// NOR
static void recNOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = ~(g_cpuConstRegs[_Rs_].UD[0] | g_cpuConstRegs[_Rt_].UD[0]);
}

static void recNOR_constv_emit_oaknut(int info, int creg, u32 vreg, int regv)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const GPR_reg64 cval = g_cpuConstRegs[creg];
	recBeginOaknutEmit();

	if (cval.SD[0] == -1)
	{
		oakAsm->MOV(oakXRegister(EEREC_D), 0);
		recEndOaknutEmit();
		return;
	}

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (regv >= 0)
		oakAsm->MOV(regd_x, oakXRegister(regv));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + vreg * sizeof(GPR_reg))});

	if (cval.SD[0] != 0)
	{
		if (oak::detail::encode_bit_imm(cval.UD[0]))
			oakAsm->ORR(regd_x, regd_x, oak::BitImm64(cval.UD[0]));
		else
		{
			oakAsm->MOV(OAK_XSCRATCH, cval.UD[0]);
			oakAsm->ORR(regd_x, regd_x, OAK_XSCRATCH);
		}
	}
	oakAsm->MVN(regd_x, regd_x);

	recEndOaknutEmit();
}

static void recNOR_constv(int info, int creg, u32 vreg, int regv)
{
	recNOR_constv_emit_oaknut(info, creg, vreg, regv);
}

static void recNOR_consts(int info)
{
	recNOR_constv(info, _Rs_, _Rt_, (info & PROCESS_EE_T) ? EEREC_T : -1);
}

static void recNOR_constt(int info)
{
	recNOR_constv(info, _Rt_, _Rs_, (info & PROCESS_EE_S) ? EEREC_S : -1);
}

static void recNOR_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	u32 rs = _Rs_, rt = _Rt_;
	int regs = (info & PROCESS_EE_S) ? EEREC_S : -1;
	int regt = (info & PROCESS_EE_T) ? EEREC_T : -1;
	if (_Rd_ == _Rt_)
	{
		std::swap(rs, rt);
		std::swap(regs, regt);
	}

	recBeginOaknutEmit();

	const oak::XReg regd_x = oakXRegister(EEREC_D);
	if (regs >= 0)
		oakAsm->MOV(regd_x, oakXRegister(regs));
	else
		oakLoad64(regd_x, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + rs * sizeof(GPR_reg))});

	if (regt >= 0)
		oakAsm->ORR(regd_x, regd_x, oakXRegister(regt));
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[0].UD[0]) + rt * sizeof(GPR_reg))});
		oakAsm->ORR(regd_x, regd_x, OAK_XSCRATCH2);
	}
	oakAsm->MVN(regd_x, regd_x);

	recEndOaknutEmit();
}

static void recNOR_(int info)
{
	recNOR_emit_oaknut(info);
}

EERECOMPILE_CODERC0(NOR, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_64BITOP);

//// SLT - test with silent hill, lemans
static void recSLT_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].SD[0] < g_cpuConstRegs[_Rt_].SD[0];
}

static void recSLT_consts(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 cval = g_cpuConstRegs[_Rs_].SD[0];
	const int dreg = (_Rd_ == _Rt_) ? _allocX86reg(X86TYPE_TEMP, 0, 0) : EEREC_D;
	const int regt = (info & PROCESS_EE_T) ? EEREC_T : -1;

	recBeginOaknutEmit();
	oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(cval));
	if (regt >= 0)
		oakAsm->CMP(oakXRegister(regt), OAK_XSCRATCH);
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UD[0]))});
		oakAsm->CMP(OAK_XSCRATCH2, OAK_XSCRATCH);
	}
	oakAsm->CSET(oakWRegister(dreg), oak::util::GT);
	recEndOaknutEmit();

	if (dreg != EEREC_D)
	{
		std::swap(x86regs[dreg], x86regs[EEREC_D]);
		_freeX86reg(EEREC_D);
	}
}

static void recSLT_constt(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 cval = g_cpuConstRegs[_Rt_].SD[0];
	const int dreg = (_Rd_ == _Rs_) ? _allocX86reg(X86TYPE_TEMP, 0, 0) : EEREC_D;
	const int regs = (info & PROCESS_EE_S) ? EEREC_S : -1;

	recBeginOaknutEmit();
	oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(cval));
	if (regs >= 0)
		oakAsm->CMP(oakXRegister(regs), OAK_XSCRATCH);
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});
		oakAsm->CMP(OAK_XSCRATCH2, OAK_XSCRATCH);
	}
	oakAsm->CSET(oakWRegister(dreg), oak::util::LT);
	recEndOaknutEmit();

	if (dreg != EEREC_D)
	{
		std::swap(x86regs[dreg], x86regs[EEREC_D]);
		_freeX86reg(EEREC_D);
	}
}

static void recSLT_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const int dreg = (_Rd_ == _Rt_ || _Rd_ == _Rs_) ? _allocX86reg(X86TYPE_TEMP, 0, 0) : EEREC_D;
	const int regs = (info & PROCESS_EE_S) ? EEREC_S : _allocX86reg(X86TYPE_GPR, _Rs_, MODE_READ);

	recBeginOaknutEmit();
	if (info & PROCESS_EE_T)
		oakAsm->CMP(oakXRegister(regs), oakXRegister(EEREC_T));
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UD[0]))});
		oakAsm->CMP(oakXRegister(regs), OAK_XSCRATCH2);
	}
	oakAsm->CSET(oakWRegister(dreg), oak::util::LT);
	recEndOaknutEmit();

	if (dreg != EEREC_D)
	{
		std::swap(x86regs[dreg], x86regs[EEREC_D]);
		_freeX86reg(EEREC_D);
	}
}

static void recSLT_(int info)
{
	recSLT_emit_oaknut(info);
}

EERECOMPILE_CODERC0(SLT, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_NORENAME);

// SLTU - test with silent hill, lemans
static void recSLTU_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] < g_cpuConstRegs[_Rt_].UD[0];
}

static void recSLTU_consts(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 cval = g_cpuConstRegs[_Rs_].SD[0];
	const int dreg = (_Rd_ == _Rt_) ? _allocX86reg(X86TYPE_TEMP, 0, 0) : EEREC_D;
	const int regt = (info & PROCESS_EE_T) ? EEREC_T : -1;

	recBeginOaknutEmit();
	oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(cval));
	if (regt >= 0)
		oakAsm->CMP(oakXRegister(regt), OAK_XSCRATCH);
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UD[0]))});
		oakAsm->CMP(OAK_XSCRATCH2, OAK_XSCRATCH);
	}
	oakAsm->CSET(oakWRegister(dreg), oak::util::HI);
	recEndOaknutEmit();

	if (dreg != EEREC_D)
	{
		std::swap(x86regs[dreg], x86regs[EEREC_D]);
		_freeX86reg(EEREC_D);
	}
}

static void recSLTU_constt(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s64 cval = g_cpuConstRegs[_Rt_].SD[0];
	const int dreg = (_Rd_ == _Rs_) ? _allocX86reg(X86TYPE_TEMP, 0, 0) : EEREC_D;
	const int regs = (info & PROCESS_EE_S) ? EEREC_S : -1;

	recBeginOaknutEmit();
	oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(cval));
	if (regs >= 0)
		oakAsm->CMP(oakXRegister(regs), OAK_XSCRATCH);
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rs_].UD[0]))});
		oakAsm->CMP(OAK_XSCRATCH2, OAK_XSCRATCH);
	}
	oakAsm->CSET(oakWRegister(dreg), oak::util::CC);
	recEndOaknutEmit();

	if (dreg != EEREC_D)
	{
		std::swap(x86regs[dreg], x86regs[EEREC_D]);
		_freeX86reg(EEREC_D);
	}
}

static void recSLTU_emit_oaknut(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const int dreg = (_Rd_ == _Rt_ || _Rd_ == _Rs_) ? _allocX86reg(X86TYPE_TEMP, 0, 0) : EEREC_D;
	const int regs = (info & PROCESS_EE_S) ? EEREC_S : _allocX86reg(X86TYPE_GPR, _Rs_, MODE_READ);

	recBeginOaknutEmit();
	if (info & PROCESS_EE_T)
		oakAsm->CMP(oakXRegister(regs), oakXRegister(EEREC_T));
	else
	{
		oakLoad64(OAK_XSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[_Rt_].UD[0]))});
		oakAsm->CMP(oakXRegister(regs), OAK_XSCRATCH2);
	}
	oakAsm->CSET(oakWRegister(dreg), oak::util::CC);
	recEndOaknutEmit();

	if (dreg != EEREC_D)
	{
		std::swap(x86regs[dreg], x86regs[EEREC_D]);
		_freeX86reg(EEREC_D);
	}
}

static void recSLTU_(int info)
{
	recSLTU_emit_oaknut(info);
}

EERECOMPILE_CODERC0(SLTU, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED | XMMINFO_NORENAME);

#endif

} // namespace R5900::Dynarec::OpcodeImpl
