// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "arm64/OaknutHelpers.h"
#include "iR5900.h"
#include "iFPU.h"

#if !defined(__ANDROID__)
using namespace x86Emitter;
#endif

//alignas(16) const u32 g_minvals[4] = {0xff7fffff, 0xff7fffff, 0xff7fffff, 0xff7fffff};
//alignas(16) const u32 g_maxvals[4] = {0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff};

//------------------------------------------------------------------
namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {
namespace COP1 {

namespace DOUBLE
{

	void recABS_S_xmm(int info);
	void recADD_S_xmm(int info);
	void recADDA_S_xmm(int info);
	void recC_EQ_xmm(int info);
	void recC_LE_xmm(int info);
	void recC_LT_xmm(int info);
	void recDIV_S_xmm(int info);
	void recMADD_S_xmm(int info);
	void recMADDA_S_xmm(int info);
	void recMAX_S_xmm(int info);
	void recMIN_S_xmm(int info);
	void recMOV_S_xmm(int info);
	void recMSUB_S_xmm(int info);
	void recMSUBA_S_xmm(int info);
	void recMUL_S_xmm(int info);
	void recMULA_S_xmm(int info);
	void recNEG_S_xmm(int info);
	void recSUB_S_xmm(int info);
	void recSUBA_S_xmm(int info);
	void recSQRT_S_xmm(int info);
	void recRSQRT_S_xmm(int info);

}; // namespace DOUBLE

//------------------------------------------------------------------
// Helper Macros
//------------------------------------------------------------------
#define _Ft_ _Rt_
#define _Fs_ _Rd_
#define _Fd_ _Sa_

// FCR31 Flags
#define FPUflagC  0x00800000
#define FPUflagI  0x00020000
#define FPUflagD  0x00010000
#define FPUflagO  0x00008000
#define FPUflagU  0x00004000
#define FPUflagSI 0x00000040
#define FPUflagSD 0x00000020
#define FPUflagSO 0x00000010
#define FPUflagSU 0x00000008

// Add/Sub opcodes produce the same results as the ps2
#define FPU_CORRECT_ADD_SUB CHECK_FPU_OVERFLOW

//alignas(16) static const u32 s_neg[4] = {0x80000000, 0xffffffff, 0xffffffff, 0xffffffff};
//alignas(16) static const u32 s_pos[4] = {0x7fffffff, 0xffffffff, 0xffffffff, 0xffffffff};

#define REC_FPUBRANCH(f) \
	void f(); \
	void rec##f() \
	{ \
		iFlushCall(FLUSH_INTERPRETER); \
		recBeginOaknutEmit(); \
		oakEmitCall(reinterpret_cast<const void*>((uptr)R5900::Interpreter::OpcodeImpl::COP1::f)); \
		recEndOaknutEmit(); \
		g_branch = 2; \
	}

#define REC_FPUFUNC(f) \
	void f(); \
	void rec##f() \
	{ \
		iFlushCall(FLUSH_INTERPRETER); \
		recBeginOaknutEmit(); \
		oakEmitCall(reinterpret_cast<const void*>((uptr)R5900::Interpreter::OpcodeImpl::COP1::f)); \
		recEndOaknutEmit(); \
	}
//------------------------------------------------------------------

//------------------------------------------------------------------
// *FPU Opcodes!*
//------------------------------------------------------------------

// Those opcode are marked as special ! But I don't understand why we can't run them in the interpreter
#ifndef FPU_RECOMPILE

REC_FPUFUNC(CFC1);
REC_FPUFUNC(CTC1);
REC_FPUFUNC(MFC1);
REC_FPUFUNC(MTC1);

#else

//------------------------------------------------------------------
// CFC1 / CTC1
//------------------------------------------------------------------
static void recMoveGPRtoOakWForCOP1(oak::WReg to, int fromgpr)
{
	if (fromgpr == 0)
	{
		oakAsm->MOV(to, 0);
		return;
	}

	if (GPR_IS_CONST1(fromgpr))
	{
		oakAsm->MOV(to, g_cpuConstRegs[fromgpr].UL[0]);
		return;
	}

	const int gprreg = _checkX86reg(X86TYPE_GPR, fromgpr, MODE_READ);
	if (gprreg >= 0)
	{
		oakAsm->MOV(to, oakWRegister(gprreg));
		return;
	}

	const int xmmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ);
	if (xmmreg >= 0)
	{
		oakAsm->FMOV(to, oakSRegister(xmmreg));
		return;
	}

	oakLoad32(to, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, cpuRegs.GPR.r[fromgpr].UL[0]))});
}

static void recCFC1_emit_oaknut()
{
	if (!_Rt_)
		return;
	EE::Profiler.EmitOp(eeOpcode::CFC1);

	const int regt = _allocX86reg(X86TYPE_GPR, _Rt_, MODE_WRITE);
	recBeginOaknutEmit();

	if (_Fs_ == 31)
	{
		const oak::WReg regt_w = oakWRegister(regt);
		oakLoad32(regt_w, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fprc[31]))});
		oakAsm->SXTW(oakXRegister(regt), regt_w);
	}
	else if (_Fs_ == 0)
	{
		oakAsm->MOV(oakXRegister(regt), 0x2e00);
	}
	else
	{
		oakAsm->MOV(oakXRegister(regt), 0);
	}

	recEndOaknutEmit();
}

void recCFC1(void)
{
	recCFC1_emit_oaknut();
}

static void recCTC1_emit_oaknut()
{
	if (_Fs_ != 31)
		return;
	EE::Profiler.EmitOp(eeOpcode::CTC1);

	recBeginOaknutEmit();
	recMoveGPRtoOakWForCOP1(OAK_WSCRATCH2, _Rt_);
	oakStore32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fprc[31]))});
	recEndOaknutEmit();
}

void recCTC1()
{
	recCTC1_emit_oaknut();
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// MFC1
//------------------------------------------------------------------
static void recMFC1_emit_oaknut()
{
	if (!_Rt_)
		return;

	EE::Profiler.EmitOp(eeOpcode::MFC1);

	const int xmmregt = _allocIfUsedGPRtoXMM(_Rt_, MODE_READ | MODE_WRITE);
	const int regs = _allocIfUsedFPUtoXMM(_Fs_, MODE_READ);
	if (regs >= 0 && xmmregt >= 0)
	{
		pxAssert(!GPR_IS_CONST1(_Rt_));

		const int temp = _allocTempXMMreg(XMMT_FPS);
		recBeginOaknutEmit();
		const oak::QReg temp_q = oakQRegister(temp);
		oakAsm->ORR(temp_q.B16(), oakQRegister(regs).B16(), oakQRegister(regs).B16());
		oakAsm->SSHR(temp_q.S4(), temp_q.S4(), 31);
		oakAsm->MOV(oakQRegister(xmmregt).Selem()[0], oakQRegister(regs).Selem()[0]);
		oakAsm->MOV(oakQRegister(xmmregt).Selem()[1], temp_q.Selem()[0]);
		recEndOaknutEmit();
		_freeXMMreg(temp);
		return;
	}

	const int regt = _allocX86reg(X86TYPE_GPR, _Rt_, MODE_WRITE);
	pxAssert(!GPR_IS_CONST1(_Rt_));

	recBeginOaknutEmit();
	if (regs >= 0)
	{
		oakAsm->FMOV(oakWRegister(regt), oakSRegister(regs));
	}
	else
	{
		oakLoad32(oakWRegister(regt), {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[_Fs_].UL))});
	}
	oakAsm->SXTW(oakXRegister(regt), oakWRegister(regt));
	recEndOaknutEmit();
}

void recMFC1()
{
	recMFC1_emit_oaknut();
}

//------------------------------------------------------------------


//------------------------------------------------------------------
// MTC1
//------------------------------------------------------------------
static void recMTC1_emit_oaknut()
{
	EE::Profiler.EmitOp(eeOpcode::MTC1);
	if (GPR_IS_CONST1(_Rt_))
	{
		const int xmmreg = _allocIfUsedFPUtoXMM(_Fs_, MODE_WRITE);
		if (xmmreg >= 0)
		{
			const int x86reg = (g_cpuConstRegs[_Rt_].UL[0] == 0) ? -1 : _allocX86reg(X86TYPE_GPR, _Rt_, MODE_READ);
			recBeginOaknutEmit();
			if (g_cpuConstRegs[_Rt_].UL[0] == 0)
			{
				oakAsm->EOR(oakQRegister(xmmreg).B16(), oakQRegister(xmmreg).B16(), oakQRegister(xmmreg).B16());
			}
			else
			{
				oakAsm->FMOV(oakSRegister(xmmreg), oakWRegister(x86reg));
			}
			recEndOaknutEmit();
		}
		else
		{
			pxAssert(!_hasXMMreg(XMMTYPE_FPREG, _Fs_));
			recBeginOaknutEmit();
			oakAsm->MOV(OAK_WSCRATCH2, g_cpuConstRegs[_Rt_].UL[0]);
			oakStore32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[_Fs_].UL))});
			recEndOaknutEmit();
		}
	}
	else
	{
		const int xmmgpr = _checkXMMreg(XMMTYPE_GPRREG, _Rt_, MODE_READ);
		if (xmmgpr >= 0)
		{
			if (g_pCurInstInfo->regs[_Rt_] & EEINST_LASTUSE)
			{
				_deleteFPtoXMMreg(_Fs_, DELETE_REG_FREE_NO_WRITEBACK);
				_reallocateXMMreg(xmmgpr, XMMTYPE_FPREG, _Fs_, MODE_WRITE);
			}
			else
			{
				const int xmmreg2 = _allocIfUsedFPUtoXMM(_Fs_, MODE_WRITE);
				recBeginOaknutEmit();
				if (xmmreg2 >= 0)
				{
					oakAsm->MOV(oakQRegister(xmmreg2).Selem()[0], oakQRegister(xmmgpr).Selem()[0]);
				}
				else
				{
					oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(xmmgpr));
					oakStore32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[_Fs_].UL))});
				}
				recEndOaknutEmit();
			}
		}
		else
		{
			const int regt = _allocX86reg(X86TYPE_GPR, _Rt_, MODE_READ);
			const int xmmreg2 = _allocIfUsedFPUtoXMM(_Fs_, MODE_WRITE);

			recBeginOaknutEmit();
			if (xmmreg2 >= 0)
			{
				oakAsm->FMOV(oakSRegister(xmmreg2), oakWRegister(regt));
			}
			else
			{
				oakStore32(oakWRegister(regt), {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[_Fs_].UL))});
			}
			recEndOaknutEmit();
		}
	}
}

void recMTC1()
{
	recMTC1_emit_oaknut();
}
#endif
//------------------------------------------------------------------


#ifndef FPU_RECOMPILE // If FPU_RECOMPILE is not defined, then use the interpreter opcodes. (CFC1, CTC1, MFC1, and MTC1 are special because they work specifically with the EE rec so they're defined above)

REC_FPUFUNC(ABS_S);
REC_FPUFUNC(ADD_S);
REC_FPUFUNC(ADDA_S);
REC_FPUBRANCH(BC1F);
REC_FPUBRANCH(BC1T);
REC_FPUBRANCH(BC1FL);
REC_FPUBRANCH(BC1TL);
REC_FPUFUNC(C_EQ);
REC_FPUFUNC(C_F);
REC_FPUFUNC(C_LE);
REC_FPUFUNC(C_LT);
REC_FPUFUNC(CVT_S);
REC_FPUFUNC(CVT_W);
REC_FPUFUNC(DIV_S);
REC_FPUFUNC(MAX_S);
REC_FPUFUNC(MIN_S);
REC_FPUFUNC(MADD_S);
REC_FPUFUNC(MADDA_S);
REC_FPUFUNC(MOV_S);
REC_FPUFUNC(MSUB_S);
REC_FPUFUNC(MSUBA_S);
REC_FPUFUNC(MUL_S);
REC_FPUFUNC(MULA_S);
REC_FPUFUNC(NEG_S);
REC_FPUFUNC(SUB_S);
REC_FPUFUNC(SUBA_S);
REC_FPUFUNC(SQRT_S);
REC_FPUFUNC(RSQRT_S);

#else // FPU_RECOMPILE

//------------------------------------------------------------------
// ABS XMM
//------------------------------------------------------------------
static void recABS_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::ABS_F);

	const oak::SReg regd_s = oakSRegister(EEREC_D);
	const oak::QReg regd_q = oakQRegister(EEREC_D);

	recBeginOaknutEmit();
	if (info & PROCESS_EE_S)
	{
		oakAsm->MOV(regd_q.Selem()[0], oakQRegister(EEREC_S).Selem()[0]);
	}
	else
	{
		oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[_Fs_].UL))});
		oakAsm->FMOV(regd_s, OAK_WSCRATCH2);
	}

	oakAsm->FMOV(OAK_WSCRATCH2, regd_s);
	oakAsm->MOV(OAK_WSCRATCH, 0x7fffffff);
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->FMOV(regd_s, OAK_WSCRATCH2);
	recEndOaknutEmit();

	// ABS.S preserves NaN/Inf payload bits and only clears the sign bit.
}

void recABS_S_xmm(int info)
{
	recABS_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(ABS_S, XMMINFO_WRITED | XMMINFO_READS);
//------------------------------------------------------------------


static void recFpuLoadScalarOperand_emit_oaknut(int dst, int fpu_reg, int cached_reg, bool cached)
{
	if (cached)
	{
		oakAsm->MOV(oakQRegister(dst).Selem()[0], oakQRegister(cached_reg).Selem()[0]);
	}
	else
	{
		oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[fpu_reg].UL))});
		oakAsm->FMOV(oakSRegister(dst), OAK_WSCRATCH2);
	}
}

static void recFpuDropCachedOperandNoWriteback(int fpu_reg)
{
	for (u32 i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse && xmmregs[i].type == XMMTYPE_FPREG && xmmregs[i].reg == fpu_reg)
			_freeXMMregWithoutWriteback(i);
	}
}

static void recFpuFlushCachedOperandToMemory(int fpu_reg)
{
	for (u32 i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse && xmmregs[i].type == XMMTYPE_FPREG && xmmregs[i].reg == fpu_reg && (xmmregs[i].mode & MODE_WRITE))
			_writebackXMMreg(i);
	}
}

static void recFpuLoadScalarMemoryOperand_emit_oaknut(int dst, int fpu_reg)
{
	oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[fpu_reg].UL))});
	oakAsm->FMOV(oakSRegister(dst), OAK_WSCRATCH2);
}

static void recFpuClampFloat3Operand_emit_oaknut(int reg)
{
	if (!CHECK_FPU_OVERFLOW)
		return;

	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(reg));
	oakAsm->MOV(OAK_WSCRATCH, 0x7f7fffff);
	oakAsm->CMP(OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->CSEL(OAK_WSCRATCH2, OAK_WSCRATCH, OAK_WSCRATCH2, oak::util::GT);
	oakAsm->MOV(OAK_WSCRATCH, 0xff7fffff);
	oakAsm->CMP(OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->CSEL(OAK_WSCRATCH2, OAK_WSCRATCH, OAK_WSCRATCH2, oak::util::HI);
	oakAsm->FMOV(oakSRegister(reg), OAK_WSCRATCH2);
}

static void recFpuDoubleClampOperand_emit_oaknut(int reg)
{
	if (!CHECK_FPU_OVERFLOW)
		return;

	oak::Label non_zero_exp;
	oak::Label done;

	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(reg));
	oakAsm->MOV(OAK_WSCRATCH, 0x7f800000);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->CBNZ(OAK_WSCRATCH, non_zero_exp);
	oakAsm->MOV(OAK_WSCRATCH, 0x80000000);
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->FMOV(oakSRegister(reg), OAK_WSCRATCH2);
	oakAsm->B(done);

	oakAsm->l(non_zero_exp);
	oakAsm->MOV(OAK_WSCRATCH, 0x7f800000);
	oakAsm->FMOV(OAK_SSCRATCH2, OAK_WSCRATCH2);
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->CMP(OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->FMOV(OAK_WSCRATCH2, OAK_SSCRATCH2);
	oakAsm->B(oak::util::NE, done);
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, 0x80000000);
	oakAsm->MOV(OAK_WSCRATCH, 0x7f7fffff);
	oakAsm->ORR(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->FMOV(oakSRegister(reg), OAK_WSCRATCH2);

	oakAsm->l(done);
}

static void recFpuFinishInterpreterResult_emit_oaknut(int reg)
{
	oak::Label no_overflow;
	oak::Label no_underflow;

	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(reg));
	oakAsm->FMOV(OAK_SSCRATCH2, OAK_WSCRATCH2);
	oakAsm->MOV(OAK_WSCRATCH, 0x7fffffff);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f800000);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->FMOV(OAK_WSCRATCH2, OAK_SSCRATCH2);
	oakAsm->B(oak::util::NE, no_overflow);
	oakAsm->MOV(OAK_WSCRATCH, 0x7f7fffff);
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, 0x80000000);
	oakAsm->ORR(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->FMOV(oakSRegister(reg), OAK_WSCRATCH2);
	oakAsm->B(no_underflow);

	oakAsm->l(no_overflow);
	oakAsm->MOV(OAK_WSCRATCH, 0x7f800000);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->CBNZ(OAK_WSCRATCH, no_underflow);
	oakAsm->MOV(OAK_WSCRATCH, 0x007fffff);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->CBZ(OAK_WSCRATCH, no_underflow);
	oakAsm->MOV(OAK_WSCRATCH, 0x80000000);
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->FMOV(oakSRegister(reg), OAK_WSCRATCH2);

	oakAsm->l(no_underflow);
}

static void recFpuOrFcr31_emit_oaknut(u32 bits)
{
	oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fprc[31]))});
	oakAsm->MOV(OAK_WSCRATCH, bits);
	oakAsm->ORR(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakStore32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fprc[31]))});
}

static void recFpuSetOverflowFlagsIfEitherExp255_emit_oaknut(int sreg, int treg)
{
	oak::Label set_flags;
	oak::Label done;

	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(sreg));
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f800000);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::EQ, set_flags);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(treg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, done);

	oakAsm->l(set_flags);
	recFpuOrFcr31_emit_oaknut(FPUflagO | FPUflagSO);
	oakAsm->l(done);
}

static void recFpuSetOverflowFlagsIfResultExp255_emit_oaknut(int reg)
{
	oak::Label done;

	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(reg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f7fffff);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::LO, done);
	recFpuOrFcr31_emit_oaknut(FPUflagO | FPUflagSO);
	oakAsm->l(done);
}

static void recFpuAdjustTinyOppositeSignTowardZero_emit_oaknut(int result_reg, oak::WReg smaller_bits)
{
	oak::Label no_adjust;

	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(result_reg));
	oakAsm->EOR(OAK_WSCRATCH, OAK_WSCRATCH, smaller_bits);
	oakAsm->TBZ(OAK_WSCRATCH, 31, no_adjust);
	oakAsm->MOV(OAK_WSCRATCH, 0x7fffffff);
	oakAsm->AND(OAK_WSCRATCH, smaller_bits, OAK_WSCRATCH);
	oakAsm->CBZ(OAK_WSCRATCH, no_adjust);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(result_reg));
	oakAsm->SUB(OAK_WSCRATCH, OAK_WSCRATCH, 1);
	oakAsm->FMOV(oakSRegister(result_reg), OAK_WSCRATCH);
	oakAsm->l(no_adjust);
}

static void recFpuAddSubExact_emit_oaknut(int regd, int sreg, int treg, bool sub)
{
	oak::Label special_normal;
	oak::Label special_s_exp255;
	oak::Label special_both_exp255;
	oak::Label special_zero;
	oak::Label diff_ge_25;
	oak::Label diff_gt_0;
	oak::Label diff_eq_0;
	oak::Label diff_le_minus_25;
	oak::Label done_far_positive;
	oak::Label done_positive;
	oak::Label done_far_negative;
	oak::Label done_negative;

	const oak::SReg regd_s = oakSRegister(regd);
	const oak::SReg sreg_s = oakSRegister(sreg);
	const oak::SReg treg_s = oakSRegister(treg);

	oakAsm->FMOV(OAK_WSCRATCH, sreg_s);
	oakAsm->FMOV(OAK_WSCRATCH2, treg_s);
	oakAsm->AND(oak::util::W4, OAK_WSCRATCH, 0x7f800000);
	oakAsm->EOR(oak::util::W4, oak::util::W4, 0x7f800000);
	oakAsm->CBZ(oak::util::W4, special_s_exp255);
	oakAsm->AND(oak::util::W4, OAK_WSCRATCH2, 0x7f800000);
	oakAsm->EOR(oak::util::W4, oak::util::W4, 0x7f800000);
	oakAsm->CBNZ(oak::util::W4, special_normal);
	if (sub)
		oakAsm->EOR(OAK_WSCRATCH2, OAK_WSCRATCH2, 0x80000000);
	oakAsm->FMOV(regd_s, OAK_WSCRATCH2);
	oakAsm->B(done_negative);

	oakAsm->l(special_s_exp255);
	oakAsm->AND(oak::util::W4, OAK_WSCRATCH2, 0x7f800000);
	oakAsm->EOR(oak::util::W4, oak::util::W4, 0x7f800000);
	oakAsm->CBZ(oak::util::W4, special_both_exp255);
	oakAsm->FMOV(regd_s, OAK_WSCRATCH);
	oakAsm->B(done_negative);

	oakAsm->l(special_both_exp255);
	if (sub)
		oakAsm->EOR(OAK_WSCRATCH2, OAK_WSCRATCH2, 0x80000000);
	oakAsm->EOR(oak::util::W4, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->TST(oak::util::W4, 0x80000000);
	oakAsm->B(oak::util::NE, special_zero);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->MOV(oak::util::W4, 0x7fffffff);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, oak::util::W4);
	oakAsm->FMOV(regd_s, OAK_WSCRATCH);
	oakAsm->B(done_negative);

	oakAsm->l(special_zero);
	oakAsm->MOV(OAK_WSCRATCH, 0);
	oakAsm->FMOV(regd_s, OAK_WSCRATCH);
	oakAsm->B(done_negative);

	oakAsm->l(special_normal);
	oakAsm->FMOV(OAK_WSCRATCH, sreg_s);
	oakAsm->FMOV(OAK_WSCRATCH2, treg_s);
	oakAsm->FMOV(OAK_SSCRATCH2, OAK_WSCRATCH);
	oakAsm->FMOV(OAK_SSCRATCH3, OAK_WSCRATCH2);
	oakAsm->FMOV(regd_s, OAK_WSCRATCH);
	oakAsm->LSR(OAK_WSCRATCH, OAK_WSCRATCH, 23);
	oakAsm->LSR(OAK_WSCRATCH2, OAK_WSCRATCH2, 23);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0xff);
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, 0xff);
	oakAsm->SUB(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, 25);
	oakAsm->B(oak::util::GE, diff_ge_25);
	oakAsm->CMP(OAK_WSCRATCH, 0);
	oakAsm->B(oak::util::GT, diff_gt_0);
	oakAsm->B(oak::util::EQ, diff_eq_0);
	oakAsm->CMN(OAK_WSCRATCH, 25);
	oakAsm->B(oak::util::LE, diff_le_minus_25);

	oakAsm->NEG(OAK_WSCRATCH, OAK_WSCRATCH);
	oakAsm->SUB(OAK_WSCRATCH, OAK_WSCRATCH, 1);
	oakAsm->MOV(OAK_WSCRATCH2, 0xffffffff);
	oakAsm->LSL(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->FMOV(OAK_WSCRATCH, OAK_SSCRATCH2);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->FMOV(regd_s, OAK_WSCRATCH);
	if (sub)
		oakAsm->FSUB(regd_s, regd_s, OAK_SSCRATCH3);
	else
		oakAsm->FADD(regd_s, regd_s, OAK_SSCRATCH3);
	oakAsm->B(done_negative);

	oakAsm->l(diff_ge_25);
	oakAsm->FMOV(OAK_WSCRATCH2, OAK_SSCRATCH3);
	oakAsm->MOV(OAK_WSCRATCH, 0x80000000);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->FMOV(OAK_SSCRATCH, OAK_WSCRATCH);
	if (sub)
		oakAsm->FSUB(regd_s, regd_s, OAK_SSCRATCH);
	else
		oakAsm->FADD(regd_s, regd_s, OAK_SSCRATCH);
	oakAsm->FMOV(OAK_WSCRATCH2, OAK_SSCRATCH3);
	if (sub)
		oakAsm->EOR(OAK_WSCRATCH2, OAK_WSCRATCH2, 0x80000000);
	recFpuAdjustTinyOppositeSignTowardZero_emit_oaknut(regd, OAK_WSCRATCH2);
	oakAsm->B(done_far_positive);

	oakAsm->l(diff_gt_0);
	oakAsm->SUB(OAK_WSCRATCH, OAK_WSCRATCH, 1);
	oakAsm->MOV(OAK_WSCRATCH2, 0xffffffff);
	oakAsm->LSL(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->FMOV(OAK_WSCRATCH, OAK_SSCRATCH3);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->FMOV(OAK_SSCRATCH, OAK_WSCRATCH);
	if (sub)
		oakAsm->FSUB(regd_s, regd_s, OAK_SSCRATCH);
	else
		oakAsm->FADD(regd_s, regd_s, OAK_SSCRATCH);
	oakAsm->B(done_positive);

	oakAsm->l(diff_le_minus_25);
	oakAsm->FMOV(OAK_WSCRATCH2, OAK_SSCRATCH2);
	oakAsm->MOV(OAK_WSCRATCH, 0x80000000);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->FMOV(regd_s, OAK_WSCRATCH);
	if (sub)
		oakAsm->FSUB(regd_s, regd_s, OAK_SSCRATCH3);
	else
		oakAsm->FADD(regd_s, regd_s, OAK_SSCRATCH3);
	oakAsm->FMOV(OAK_WSCRATCH2, OAK_SSCRATCH2);
	recFpuAdjustTinyOppositeSignTowardZero_emit_oaknut(regd, OAK_WSCRATCH2);
	oakAsm->B(done_far_negative);

	oakAsm->l(diff_eq_0);
	if (sub)
		oakAsm->FSUB(regd_s, regd_s, OAK_SSCRATCH3);
	else
		oakAsm->FADD(regd_s, regd_s, OAK_SSCRATCH3);

	oakAsm->l(done_far_positive);
	oakAsm->l(done_positive);
	oakAsm->l(done_far_negative);
	oakAsm->l(done_negative);
}

static void recADD_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::ADD_F);

	const int sreg = _allocTempXMMreg(XMMT_FPS);
	const int treg = _allocTempXMMreg(XMMT_FPS);

	recFpuFlushCachedOperandToMemory(_Fs_);
	if (_Ft_ != _Fs_)
		recFpuFlushCachedOperandToMemory(_Ft_);

	recBeginOaknutEmit();
	recFpuLoadScalarMemoryOperand_emit_oaknut(sreg, _Fs_);
	recFpuLoadScalarMemoryOperand_emit_oaknut(treg, _Ft_);
	recFpuDoubleClampOperand_emit_oaknut(sreg);
	recFpuDoubleClampOperand_emit_oaknut(treg);
	if (CHECK_FPU_EXTRA_OVERFLOW)
	{
		recFpuClampFloat3Operand_emit_oaknut(sreg);
		recFpuClampFloat3Operand_emit_oaknut(treg);
	}
	if (FPU_CORRECT_ADD_SUB)
	{
		recFpuAddSubExact_emit_oaknut(EEREC_D, sreg, treg, false);
	}
	else
	{
		oakAsm->FADD(oakSRegister(EEREC_D), oakSRegister(sreg), oakSRegister(treg));
	}
	recEndOaknutEmit();

	_freeXMMreg(treg);
	_freeXMMreg(sreg);
}

//------------------------------------------------------------------
// ADD XMM
//------------------------------------------------------------------
void recADD_S_xmm(int info)
{
	recADD_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(ADD_S, XMMINFO_WRITED | XMMINFO_READD | XMMINFO_READS | XMMINFO_READT);

static void recADDA_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::ADDA_F);

	const int sreg = _allocTempXMMreg(XMMT_FPS);
	const int treg = _allocTempXMMreg(XMMT_FPS);

	recBeginOaknutEmit();
	recFpuLoadScalarOperand_emit_oaknut(sreg, _Fs_, EEREC_S, info & PROCESS_EE_S);
	recFpuLoadScalarOperand_emit_oaknut(treg, _Ft_, EEREC_T, info & PROCESS_EE_T);
	recFpuDoubleClampOperand_emit_oaknut(sreg);
	recFpuDoubleClampOperand_emit_oaknut(treg);
	if (CHECK_FPU_EXTRA_OVERFLOW)
	{
		recFpuClampFloat3Operand_emit_oaknut(sreg);
		recFpuClampFloat3Operand_emit_oaknut(treg);
	}
	if (FPU_CORRECT_ADD_SUB)
	{
		recFpuAddSubExact_emit_oaknut(EEREC_ACC, sreg, treg, false);
	}
	else
	{
		oakAsm->FADD(oakSRegister(EEREC_ACC), oakSRegister(sreg), oakSRegister(treg));
	}
	recEndOaknutEmit();

	_freeXMMreg(treg);
	_freeXMMreg(sreg);
}

void recADDA_S_xmm(int info)
{
	recADDA_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(ADDA_S, XMMINFO_WRITEACC | XMMINFO_READS | XMMINFO_READT);
//------------------------------------------------------------------

//------------------------------------------------------------------
// BC1x XMM
//------------------------------------------------------------------

static void recBC1BranchTest_emit_oaknut()
{
	// Do not remove this as a simple performance cleanup. On ARM64 this currently
	// acts as a required EE state barrier; removing it caused startup black screens.
	_eeFlushAllDirty();

	// COP1 branch conditionals are based on the following equation:
	// (fpuRegs.fprc[31] & 0x00800000)
	// BC2F checks if the statement is false, BC2T checks if the statement is true.

	recBeginOaknutEmit();
	oakLoad32(OAK_WSCRATCH, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fprc[31]))});
	oakAsm->TST(OAK_WSCRATCH, FPUflagC);
	recEndOaknutEmit();
}

static bool recBC1IsBranch(u32 code, u32 rt)
{
	return ((code >> 26) == 0x11) && (((code >> 21) & 0x1f) == 8) && (((code >> 16) & 0x1f) == rt);
}

static u32 recBC1BranchTarget(u32 code, u32 branch_pc)
{
	return branch_pc + 4 + (static_cast<s32>(static_cast<s16>(code & 0xffff)) * 4);
}

static bool recBC1TDelayChainTarget(u32* branch_to)
{
	constexpr int SCAN_LIMIT = 8;
	u32 scan_pc = pc;
	u32 candidate_branch_to = *branch_to;
	bool found = false;

	for (int depth = 0; depth < SCAN_LIMIT; depth++)
	{
		const u32 code = *reinterpret_cast<u32*>(PSM(scan_pc));
		if (!recBC1IsBranch(code, 1))
			break;

		candidate_branch_to = recBC1BranchTarget(code, scan_pc);
		scan_pc += 4;
		found = true;
	}

	if (recBC1IsBranch(*reinterpret_cast<u32*>(PSM(scan_pc)), 1))
		return false;

	if (found)
		*branch_to = candidate_branch_to;

	return found;
}

static bool recBC1TLDelayChainTarget(u32* branch_to)
{
	constexpr int SCAN_LIMIT = 8;
	u32 scan_pc = pc;
	u32 candidate_branch_to = *branch_to;
	bool found = false;

	for (int depth = 0; depth < SCAN_LIMIT; depth++)
	{
		const u32 code = *reinterpret_cast<u32*>(PSM(scan_pc));
		if (!recBC1IsBranch(code, 3))
			break;

		candidate_branch_to = recBC1BranchTarget(code, scan_pc);
		scan_pc += 4;
		found = true;
	}

	if (recBC1IsBranch(*reinterpret_cast<u32*>(PSM(scan_pc)), 3))
		return false;

	if (found)
		*branch_to = candidate_branch_to;

	return found;
}

void recBC1F()
{
	EE::Profiler.EmitOp(eeOpcode::BC1F);
	const u32 branchTo = ((s32)_Imm_ * 4) + pc;
	const bool swap = TrySwapDelaySlot(0, 0, 0, true);
	recBC1BranchTest_emit_oaknut();
	recDoBranchImmOaknut(branchTo, oak::Cond::NE, false, swap);
}

void recBC1T()
{
	EE::Profiler.EmitOp(eeOpcode::BC1T);
	u32 branchTo = ((s32)_Imm_ * 4) + pc;
	const bool swap = recBC1TDelayChainTarget(&branchTo) || TrySwapDelaySlot(0, 0, 0, true);
	recBC1BranchTest_emit_oaknut();
	recDoBranchImmOaknut(branchTo, oak::Cond::EQ, false, swap);
}

void recBC1FL()
{
	EE::Profiler.EmitOp(eeOpcode::BC1FL);
	const u32 branchTo = ((s32)_Imm_ * 4) + pc;
	recBC1BranchTest_emit_oaknut();
	recDoBranchImmOaknut(branchTo, oak::Cond::NE, true, false);
}

void recBC1TL()
{
	EE::Profiler.EmitOp(eeOpcode::BC1TL);
	u32 branchTo = ((s32)_Imm_ * 4) + pc;
	const bool swap = recBC1TLDelayChainTarget(&branchTo);
	recBC1BranchTest_emit_oaknut();
	recDoBranchImmOaknut(branchTo, oak::Cond::EQ, true, swap);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// C.x.S XMM
//------------------------------------------------------------------
static void recFpuCompareLoadOperand_emit_oaknut(int dst, int fpu_reg, int cached_reg, bool cached)
{
	if (cached)
	{
		oakAsm->MOV(oakQRegister(dst).Selem()[0], oakQRegister(cached_reg).Selem()[0]);
	}
	else
	{
		oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[fpu_reg].UL))});
		oakAsm->FMOV(oakSRegister(dst), OAK_WSCRATCH2);
	}
}

static void recFpuCompareNormalizeKey_emit_oaknut(int reg)
{
	oak::Label not_exp255;

	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(reg));
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f800000);
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH2, 0);
	oakAsm->CSEL(OAK_WSCRATCH, oak::util::WZR, OAK_WSCRATCH, oak::util::EQ);
	oakAsm->MOV(oak::util::W4, 0x7f800000);
	oakAsm->CMP(OAK_WSCRATCH2, oak::util::W4);
	oakAsm->B(oak::util::NE, not_exp255);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->MOV(oak::util::W4, 0x7f7fffff);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, oak::util::W4);
	oakAsm->l(not_exp255);
	oakAsm->TST(OAK_WSCRATCH, 0x80000000);
	oakAsm->MVN(OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->CSEL(OAK_WSCRATCH, OAK_WSCRATCH2, OAK_WSCRATCH, oak::util::NE);
	oakAsm->FMOV(oakSRegister(reg), OAK_WSCRATCH);
}

static void recFpuCompareWriteFlag_emit_oaknut(oak::Cond condition)
{
	oakAsm->CSET(OAK_WSCRATCH, condition);
	oakAsm->LSL(OAK_WSCRATCH, OAK_WSCRATCH, 23);
	oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fprc[31]))});
	oakAsm->MOV(oak::util::W4, ~static_cast<u32>(FPUflagC));
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, oak::util::W4);
	oakAsm->ORR(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakStore32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fprc[31]))});
}

static void recC_EQ_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::CEQ_F);

	const int sreg = _allocTempXMMreg(XMMT_FPS);
	const int treg = _allocTempXMMreg(XMMT_FPS);

	recBeginOaknutEmit();
	recFpuCompareLoadOperand_emit_oaknut(sreg, _Fs_, EEREC_S, info & PROCESS_EE_S);
	recFpuCompareLoadOperand_emit_oaknut(treg, _Ft_, EEREC_T, info & PROCESS_EE_T);
	recFpuCompareNormalizeKey_emit_oaknut(sreg);
	recFpuCompareNormalizeKey_emit_oaknut(treg);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(sreg));
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(treg));
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	recFpuCompareWriteFlag_emit_oaknut(oak::util::EQ);
	recEndOaknutEmit();

	_freeXMMreg(treg);
	_freeXMMreg(sreg);
}

void recC_EQ_xmm(int info)
{
	recC_EQ_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(C_EQ, XMMINFO_READS | XMMINFO_READT);
//REC_FPUFUNC(C_EQ);

static void recC_F_emit_oaknut()
{
	EE::Profiler.EmitOp(eeOpcode::CF_F);

	recBeginOaknutEmit();
	oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fprc[31]))});
	oakAsm->MOV(OAK_WSCRATCH, ~static_cast<u32>(FPUflagC));
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakStore32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fprc[31]))});
	recEndOaknutEmit();
}

void recC_F()
{
	recC_F_emit_oaknut();
}
//REC_FPUFUNC(C_F);

static void recC_LE_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::CLE_F);

	const int sreg = _allocTempXMMreg(XMMT_FPS);
	const int treg = _allocTempXMMreg(XMMT_FPS);

	recBeginOaknutEmit();
	recFpuCompareLoadOperand_emit_oaknut(sreg, _Fs_, EEREC_S, info & PROCESS_EE_S);
	recFpuCompareLoadOperand_emit_oaknut(treg, _Ft_, EEREC_T, info & PROCESS_EE_T);
	recFpuCompareNormalizeKey_emit_oaknut(sreg);
	recFpuCompareNormalizeKey_emit_oaknut(treg);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(sreg));
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(treg));
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	recFpuCompareWriteFlag_emit_oaknut(oak::util::LS);
	recEndOaknutEmit();

	_freeXMMreg(treg);
	_freeXMMreg(sreg);
}

void recC_LE_xmm(int info)
{
	recC_LE_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(C_LE, XMMINFO_READS | XMMINFO_READT);
//REC_FPUFUNC(C_LE);

static void recC_LT_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::CLT_F);

	const int sreg = _allocTempXMMreg(XMMT_FPS);
	const int treg = _allocTempXMMreg(XMMT_FPS);

	recBeginOaknutEmit();
	recFpuCompareLoadOperand_emit_oaknut(sreg, _Fs_, EEREC_S, info & PROCESS_EE_S);
	recFpuCompareLoadOperand_emit_oaknut(treg, _Ft_, EEREC_T, info & PROCESS_EE_T);
	recFpuCompareNormalizeKey_emit_oaknut(sreg);
	recFpuCompareNormalizeKey_emit_oaknut(treg);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(sreg));
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(treg));
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	recFpuCompareWriteFlag_emit_oaknut(oak::util::CC);
	recEndOaknutEmit();

	_freeXMMreg(treg);
	_freeXMMreg(sreg);
}

void recC_LT_xmm(int info)
{
	recC_LT_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(C_LT, XMMINFO_READS | XMMINFO_READT);
//REC_FPUFUNC(C_LT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// CVT.x XMM
//------------------------------------------------------------------
static void recCVT_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::CVTS_F);

	if (info & PROCESS_EE_D)
	{
		recBeginOaknutEmit();
		if (info & PROCESS_EE_S)
		{
			oakAsm->SCVTF(oakSRegister(EEREC_D), oakSRegister(EEREC_S));
		}
		else
		{
			oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[_Fs_].UL))});
			oakAsm->SCVTF(oakSRegister(EEREC_D), OAK_WSCRATCH2);
		}
		recEndOaknutEmit();
	}
	else
	{
		const int temp = _allocTempXMMreg(XMMT_FPS);

		recBeginOaknutEmit();
		if (info & PROCESS_EE_S)
		{
			oakAsm->SCVTF(oakSRegister(temp), oakSRegister(EEREC_S));
		}
		else
		{
			oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[_Fs_].UL))});
			oakAsm->SCVTF(oakSRegister(temp), OAK_WSCRATCH2);
		}
		oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(temp));
		oakStore32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[_Fd_].UL))});
		recEndOaknutEmit();

		_freeXMMreg(temp);
	}
}

void recCVT_S_xmm(int info)
{
	recCVT_S_emit_oaknut(info);
}

void recCVT_S()
{
	// Float version is fully accurate, no double version
	eeFPURecompileCode(recCVT_S_xmm, R5900::Interpreter::OpcodeImpl::COP1::CVT_S, XMMINFO_WRITED | XMMINFO_READS);
}

static void recCVT_W_emit_oaknut()
{
	// Float version is fully accurate, no double version

	// If we have the following EmitOP() on the top then it'll get calculated twice when CHECK_FPU_FULL is true
	// as we also have an EmitOP() at recCVT_W() on iFPUd.cpp.  hence we have it below the possible return.
	EE::Profiler.EmitOp(eeOpcode::CVTW);

	const int regs = _checkXMMreg(XMMTYPE_FPREG, _Fs_, MODE_READ);

	// kill register allocation for dst because we write directly to fpuRegs.fpr[_Fd_]
	_deleteFPtoXMMreg(_Fd_, DELETE_REG_FREE_NO_WRITEBACK);

	recBeginOaknutEmit();
	if (regs >= 0)
		oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(regs));
	else
		oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[_Fs_].UL))});

	oakAsm->FMOV(OAK_SSCRATCH2, OAK_WSCRATCH2);
	oakAsm->FCVTZS(OAK_WSCRATCH, OAK_SSCRATCH2);

	oakAsm->MOV(oak::util::W4, 0x7fffffff);
	oakAsm->TST(OAK_WSCRATCH2, 0x80000000);
	oakAsm->MOV(OAK_WSCRATCH2, 0x80000000);
	oakAsm->CSEL(oak::util::W4, OAK_WSCRATCH2, oak::util::W4, oak::util::NE);
	oakAsm->FMOV(OAK_WSCRATCH2, OAK_SSCRATCH2);
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, 0x7f800000);
	oakAsm->MOV(OAK_WSCRATCH, 0x4f000000);
	oakAsm->CMP(OAK_WSCRATCH2, OAK_WSCRATCH);
	oak::Label cvt_no_saturate;
	oak::Label cvt_done;
	oakAsm->B(oak::util::LO, cvt_no_saturate);
	oakAsm->MOV(OAK_WSCRATCH, oak::util::W4);
	oakAsm->B(cvt_done);
	oakAsm->l(cvt_no_saturate);
	oakAsm->FCVTZS(OAK_WSCRATCH, OAK_SSCRATCH2);
	oakAsm->l(cvt_done);
	oakStore32(OAK_WSCRATCH, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[_Fd_].UL))});
	recEndOaknutEmit();
}

void recCVT_W()
{
	recCVT_W_emit_oaknut();
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// DIV XMM
//------------------------------------------------------------------
static void recFpuSetFpcr_emit_oaknut(u64 fpcr)
{
	oakAsm->MOV(OAK_XSCRATCH, fpcr);
	oakAsm->MSR(oak::SystemReg::FPCR, OAK_XSCRATCH);
	oakAsm->ISB();
}

static void recDIV_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::DIV_F);

	oak::Label normal_div;
	oak::Label numerator_exp255;
	oak::Label denominator_exp255;
	oak::Label both_exp255;
	oak::Label done;
	const int sreg = _allocTempXMMreg(XMMT_FPS);
	const int treg = _allocTempXMMreg(XMMT_FPS);
	const int srawreg = _allocTempXMMreg(XMMT_FPS);
	const int trawreg = _allocTempXMMreg(XMMT_FPS);

	recBeginOaknutEmit();
	recFpuLoadScalarOperand_emit_oaknut(sreg, _Fs_, EEREC_S, info & PROCESS_EE_S);
	recFpuLoadScalarOperand_emit_oaknut(treg, _Ft_, EEREC_T, info & PROCESS_EE_T);
	oakAsm->MOV(oakQRegister(srawreg).Selem()[0], oakQRegister(sreg).Selem()[0]);
	oakAsm->MOV(oakQRegister(trawreg).Selem()[0], oakQRegister(treg).Selem()[0]);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(srawreg));
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f800000);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::EQ, numerator_exp255);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(trawreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::EQ, denominator_exp255);
	oakAsm->B(normal_div);

	oakAsm->l(numerator_exp255);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(trawreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::EQ, both_exp255);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(srawreg));
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(trawreg));
	oakAsm->EOR(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, 0x80000000);
	oakAsm->MOV(OAK_WSCRATCH, 0x7f7fffff);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);
	oakAsm->B(done);

	oakAsm->l(denominator_exp255);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(srawreg));
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(trawreg));
	oakAsm->EOR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);
	oakAsm->B(done);

	oakAsm->l(both_exp255);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(srawreg));
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(trawreg));
	oakAsm->EOR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->MOV(OAK_WSCRATCH2, 0x3f800000);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);
	oakAsm->B(done);

	oakAsm->l(normal_div);
	recFpuDoubleClampOperand_emit_oaknut(sreg);
	recFpuDoubleClampOperand_emit_oaknut(treg);
	oakAsm->FDIV(oakSRegister(EEREC_D), oakSRegister(sreg), oakSRegister(treg));
	recFpuFinishInterpreterResult_emit_oaknut(EEREC_D);

	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(trawreg));
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(srawreg));
	oakAsm->EOR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->MOV(oak::util::W4, 0x7f7fffff);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, oak::util::W4);
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(trawreg));
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, 0x7f800000);
	oakAsm->CMP(OAK_WSCRATCH2, 0);
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(EEREC_D));
	oakAsm->CSEL(OAK_WSCRATCH2, OAK_WSCRATCH, OAK_WSCRATCH2, oak::util::EQ);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH2);

	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(srawreg));
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, 0x7f800000);
	oakAsm->CMP(OAK_WSCRATCH2, 0);
	oakAsm->MOV(OAK_WSCRATCH, FPUflagI | FPUflagSI);
	oakAsm->MOV(OAK_WSCRATCH2, FPUflagD | FPUflagSD);
	oakAsm->CSEL(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2, oak::util::EQ);
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(trawreg));
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, 0x7f800000);
	oakAsm->CMP(OAK_WSCRATCH2, 0);
	oakAsm->CSEL(OAK_WSCRATCH, OAK_WSCRATCH, oak::util::WZR, oak::util::EQ);
	oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fprc[31]))});
	oakAsm->ORR(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakStore32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fprc[31]))});
	oakAsm->l(done);
	recEndOaknutEmit();

	_freeXMMreg(trawreg);
	_freeXMMreg(srawreg);
	_freeXMMreg(treg);
	_freeXMMreg(sreg);
}

void recDIV_S_xmm(int info)
{
	recDIV_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(DIV_S, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
//------------------------------------------------------------------



//------------------------------------------------------------------
// MADD XMM
//------------------------------------------------------------------
static void recFpuLoadAccOperand_emit_oaknut(int dst, int cached_reg, bool cached)
{
	if (cached)
	{
		oakAsm->MOV(oakQRegister(dst).Selem()[0], oakQRegister(cached_reg).Selem()[0]);
	}
	else
	{
		oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.ACC.UL))});
		oakAsm->FMOV(oakSRegister(dst), OAK_WSCRATCH2);
	}
}

static void recFpuMaddProduct_emit_oaknut(int productreg, int sreg, int treg, bool clamp_product)
{
	oak::Label s_exp255;
	oak::Label t_exp255;
	oak::Label both_exp255;
	oak::Label s_special_zero;
	oak::Label t_special_zero;
	oak::Label normal_product;
	oak::Label done;

	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(sreg));
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f800000);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::EQ, s_exp255);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(treg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::EQ, t_exp255);
	oakAsm->B(normal_product);

	oakAsm->l(s_exp255);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(treg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::EQ, both_exp255);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(treg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->CBZ(OAK_WSCRATCH, s_special_zero);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(sreg));
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(treg));
	oakAsm->EOR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f7fffff);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->FMOV(oakSRegister(productreg), OAK_WSCRATCH);
	oakAsm->B(done);

	oakAsm->l(s_special_zero);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(sreg));
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(treg));
	oakAsm->EOR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->FMOV(oakSRegister(productreg), OAK_WSCRATCH);
	oakAsm->B(done);

	oakAsm->l(t_exp255);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(sreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->CBZ(OAK_WSCRATCH, t_special_zero);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(sreg));
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(treg));
	oakAsm->EOR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f7fffff);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->FMOV(oakSRegister(productreg), OAK_WSCRATCH);
	oakAsm->B(done);

	oakAsm->l(t_special_zero);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(sreg));
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(treg));
	oakAsm->EOR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->FMOV(oakSRegister(productreg), OAK_WSCRATCH);
	oakAsm->B(done);

	oakAsm->l(both_exp255);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(sreg));
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(treg));
	oakAsm->EOR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f7fffff);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->FMOV(oakSRegister(productreg), OAK_WSCRATCH);
	oakAsm->B(done);

	oakAsm->l(normal_product);
	recFpuDoubleClampOperand_emit_oaknut(sreg);
	recFpuDoubleClampOperand_emit_oaknut(treg);
	oakAsm->FMUL(oakSRegister(productreg), oakSRegister(sreg), oakSRegister(treg));
	if (clamp_product)
		recFpuDoubleClampOperand_emit_oaknut(productreg);

	oakAsm->l(done);
}

static void recFpuMaddRestoreExp255Product_emit_oaknut(int dst, int accreg, int productreg, int sreg, int treg)
{
	oak::Label restore;
	oak::Label check_product;
	oak::Label done;

	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(accreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f7fffff);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::EQ, check_product);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f800000);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, done);
	oakAsm->l(check_product);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(productreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f7fffff);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, done);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f800000);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(sreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::EQ, restore);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(treg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, done);
	oakAsm->l(restore);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(productreg));
	oakAsm->FMOV(oakSRegister(dst), OAK_WSCRATCH);
	oakAsm->l(done);
}

static void recFpuMsubRestoreExp255Product_emit_oaknut(int dst, int accreg, int productreg, int sreg, int treg)
{
	oak::Label restore;
	oak::Label check_product;
	oak::Label done;

	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(accreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f7fffff);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::EQ, check_product);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f800000);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, done);
	oakAsm->l(check_product);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(productreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f7fffff);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, done);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f800000);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(sreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::EQ, restore);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(treg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, done);
	oakAsm->l(restore);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(productreg));
	oakAsm->EOR(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->FMOV(oakSRegister(dst), OAK_WSCRATCH);
	oakAsm->l(done);
}

static void recFpuMaddRestoreSpecialAcc_emit_oaknut(int dst, int accreg, int productreg)
{
	oak::Label done;

	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(productreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f7fffff);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::EQ, done);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(accreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, done);
	oakAsm->MOV(oakQRegister(dst).Selem()[0], oakQRegister(accreg).Selem()[0]);
	oakAsm->l(done);
}

static void recMADD_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MADD_F);

	const int sreg = _allocTempXMMreg(XMMT_FPS);
	const int treg = _allocTempXMMreg(XMMT_FPS);
	const int accreg = _allocTempXMMreg(XMMT_FPS);
	const int productreg = _allocTempXMMreg(XMMT_FPS);

	recBeginOaknutEmit();
	recFpuLoadScalarOperand_emit_oaknut(sreg, _Fs_, EEREC_S, info & PROCESS_EE_S);
	recFpuLoadScalarOperand_emit_oaknut(treg, _Ft_, EEREC_T, info & PROCESS_EE_T);
	recFpuLoadAccOperand_emit_oaknut(accreg, EEREC_ACC, info & PROCESS_EE_ACC);
	recFpuMaddProduct_emit_oaknut(productreg, sreg, treg, true);
	oakAsm->FADD(oakSRegister(EEREC_D), oakSRegister(accreg), oakSRegister(productreg));
	recFpuFinishInterpreterResult_emit_oaknut(EEREC_D);
	recFpuMaddRestoreSpecialAcc_emit_oaknut(EEREC_D, accreg, productreg);
	recEndOaknutEmit();

	_freeXMMreg(productreg);
	_freeXMMreg(accreg);
	_freeXMMreg(treg);
	_freeXMMreg(sreg);
}

void recMADD_S_xmm(int info)
{
	recMADD_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(MADD_S, XMMINFO_WRITED | XMMINFO_READACC | XMMINFO_READS | XMMINFO_READT);

static void recMADDA_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MADDA_F);

	const int sreg = _allocTempXMMreg(XMMT_FPS);
	const int treg = _allocTempXMMreg(XMMT_FPS);
	const int accreg = _allocTempXMMreg(XMMT_FPS);
	const int productreg = _allocTempXMMreg(XMMT_FPS);

	recBeginOaknutEmit();
	recFpuLoadScalarOperand_emit_oaknut(sreg, _Fs_, EEREC_S, info & PROCESS_EE_S);
	recFpuLoadScalarOperand_emit_oaknut(treg, _Ft_, EEREC_T, info & PROCESS_EE_T);
	recFpuLoadAccOperand_emit_oaknut(accreg, EEREC_ACC, info & PROCESS_EE_ACC);
	recFpuMaddProduct_emit_oaknut(productreg, sreg, treg, true);
	oakAsm->FADD(oakSRegister(EEREC_ACC), oakSRegister(accreg), oakSRegister(productreg));
	recFpuFinishInterpreterResult_emit_oaknut(EEREC_ACC);
	recFpuMaddRestoreExp255Product_emit_oaknut(EEREC_ACC, accreg, productreg, sreg, treg);
	recFpuMaddRestoreSpecialAcc_emit_oaknut(EEREC_ACC, accreg, productreg);
	recEndOaknutEmit();

	_freeXMMreg(productreg);
	_freeXMMreg(accreg);
	_freeXMMreg(treg);
	_freeXMMreg(sreg);
}

void recMADDA_S_xmm(int info)
{
	recMADDA_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(MADDA_S, XMMINFO_WRITEACC | XMMINFO_READACC | XMMINFO_READS | XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MAX / MIN XMM
//------------------------------------------------------------------
static void recFpuLoadScalarBitsOperand_emit_oaknut(oak::WReg dst, int fpu_reg, int cached_reg, bool cached)
{
	if (cached)
	{
		oakAsm->FMOV(dst, oakSRegister(cached_reg));
	}
	else
	{
		oakLoad32(dst, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[fpu_reg].UL))});
	}
}

static void recMAX_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MAX_F);

	recBeginOaknutEmit();
	recFpuLoadScalarBitsOperand_emit_oaknut(OAK_WSCRATCH, _Fs_, EEREC_S, info & PROCESS_EE_S);
	recFpuLoadScalarBitsOperand_emit_oaknut(OAK_WSCRATCH2, _Ft_, EEREC_T, info & PROCESS_EE_T);
	oakAsm->AND(oak::util::W4, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->TST(oak::util::W4, 0x80000000);
	oak::Label max_negative;
	oak::Label max_done;
	oakAsm->B(oak::util::NE, max_negative);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CSEL(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2, oak::util::GE);
	oakAsm->B(max_done);
	oakAsm->l(max_negative);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CSEL(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2, oak::util::LE);
	oakAsm->l(max_done);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);
	recEndOaknutEmit();
}

void recMAX_S_xmm(int info)
{
	recMAX_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(MAX_S, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

static void recMIN_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MIN_F);

	recBeginOaknutEmit();
	recFpuLoadScalarBitsOperand_emit_oaknut(OAK_WSCRATCH, _Fs_, EEREC_S, info & PROCESS_EE_S);
	recFpuLoadScalarBitsOperand_emit_oaknut(OAK_WSCRATCH2, _Ft_, EEREC_T, info & PROCESS_EE_T);
	oakAsm->AND(oak::util::W4, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->TST(oak::util::W4, 0x80000000);
	oak::Label min_negative;
	oak::Label min_done;
	oakAsm->B(oak::util::NE, min_negative);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CSEL(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2, oak::util::LE);
	oakAsm->B(min_done);
	oakAsm->l(min_negative);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CSEL(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2, oak::util::GE);
	oakAsm->l(min_done);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);
	recEndOaknutEmit();
}

void recMIN_S_xmm(int info)
{
	recMIN_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(MIN_S, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MOV XMM
//------------------------------------------------------------------
static void recMOV_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MOV_F);

	recBeginOaknutEmit();
	if (info & PROCESS_EE_S)
	{
		oakAsm->MOV(oakQRegister(EEREC_D).Selem()[0], oakQRegister(EEREC_S).Selem()[0]);
	}
	else
	{
		oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[_Fs_].UL))});
		oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH2);
	}
	recEndOaknutEmit();
}

void recMOV_S_xmm(int info)
{
	recMOV_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(MOV_S, XMMINFO_WRITED | XMMINFO_READS);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MSUB XMM
//------------------------------------------------------------------
static void recMSUB_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MSUB_F);

	const int sreg = _allocTempXMMreg(XMMT_FPS);
	const int treg = _allocTempXMMreg(XMMT_FPS);
	const int accreg = _allocTempXMMreg(XMMT_FPS);
	const int productreg = _allocTempXMMreg(XMMT_FPS);

	recBeginOaknutEmit();
	recFpuLoadScalarOperand_emit_oaknut(sreg, _Fs_, EEREC_S, info & PROCESS_EE_S);
	recFpuLoadScalarOperand_emit_oaknut(treg, _Ft_, EEREC_T, info & PROCESS_EE_T);
	recFpuLoadAccOperand_emit_oaknut(accreg, EEREC_ACC, info & PROCESS_EE_ACC);
	recFpuMaddProduct_emit_oaknut(productreg, sreg, treg, true);
	oakAsm->FSUB(oakSRegister(EEREC_D), oakSRegister(accreg), oakSRegister(productreg));
	recFpuFinishInterpreterResult_emit_oaknut(EEREC_D);
	recEndOaknutEmit();

	_freeXMMreg(productreg);
	_freeXMMreg(accreg);
	_freeXMMreg(treg);
	_freeXMMreg(sreg);
}

void recMSUB_S_xmm(int info)
{
	recMSUB_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(MSUB_S, XMMINFO_WRITED | XMMINFO_READACC | XMMINFO_READS | XMMINFO_READT);

static void recMSUBA_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MSUBA_F);

	const int sreg = _allocTempXMMreg(XMMT_FPS);
	const int treg = _allocTempXMMreg(XMMT_FPS);
	const int accreg = _allocTempXMMreg(XMMT_FPS);
	const int productreg = _allocTempXMMreg(XMMT_FPS);

	recBeginOaknutEmit();
	recFpuLoadScalarOperand_emit_oaknut(sreg, _Fs_, EEREC_S, info & PROCESS_EE_S);
	recFpuLoadScalarOperand_emit_oaknut(treg, _Ft_, EEREC_T, info & PROCESS_EE_T);
	recFpuLoadAccOperand_emit_oaknut(accreg, EEREC_ACC, info & PROCESS_EE_ACC);
	recFpuMaddProduct_emit_oaknut(productreg, sreg, treg, true);
	oakAsm->FSUB(oakSRegister(EEREC_ACC), oakSRegister(accreg), oakSRegister(productreg));
	recFpuFinishInterpreterResult_emit_oaknut(EEREC_ACC);
	recFpuMsubRestoreExp255Product_emit_oaknut(EEREC_ACC, accreg, productreg, sreg, treg);
	recEndOaknutEmit();

	_freeXMMreg(productreg);
	_freeXMMreg(accreg);
	_freeXMMreg(treg);
	_freeXMMreg(sreg);
}

void recMSUBA_S_xmm(int info)
{
	recMSUBA_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(MSUBA_S, XMMINFO_WRITEACC | XMMINFO_READACC | XMMINFO_READS | XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MUL XMM
//------------------------------------------------------------------
static void recFpuClampFloatResult_emit_oaknut(int regd)
{
	if (CHECK_FPU_OVERFLOW)
	{
		oakAsm->MOV(OAK_WSCRATCH, 0x7f7fffff);
		oakAsm->FMOV(OAK_SSCRATCH, OAK_WSCRATCH);
		oakAsm->FMINNM(oakSRegister(regd), oakSRegister(regd), OAK_SSCRATCH);
		oakAsm->MOV(OAK_WSCRATCH, 0xff7fffff);
		oakAsm->FMOV(OAK_SSCRATCH, OAK_WSCRATCH);
		oakAsm->FMAXNM(oakSRegister(regd), oakSRegister(regd), OAK_SSCRATCH);
	}
}

static void recFpuFinishMulResult_emit_oaknut(int regd)
{
	if (!CHECK_FPU_OVERFLOW)
		return;

	oak::Label done;

	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(regd));
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f800000);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::EQ, done);
	recFpuClampFloatResult_emit_oaknut(regd);
	oakAsm->l(done);
}

static void recFpuMulExact_emit_oaknut(int info, int regd)
{
	const int sreg = _allocTempXMMreg(XMMT_FPS);
	const int treg = _allocTempXMMreg(XMMT_FPS);

	recBeginOaknutEmit();
	recFpuLoadScalarOperand_emit_oaknut(sreg, _Fs_, EEREC_S, info & PROCESS_EE_S);
	recFpuLoadScalarOperand_emit_oaknut(treg, _Ft_, EEREC_T, info & PROCESS_EE_T);
	if (CHECK_FPUMULHACK)
	{
		oak::Label no_hack;
		oak::Label done;

		oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(sreg));
		oakAsm->EOR(OAK_WSCRATCH, OAK_WSCRATCH, 0x3e800000);
		oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(treg));
		oakAsm->EOR(OAK_WSCRATCH2, OAK_WSCRATCH2, 0x40490fdb);
		oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
		oakAsm->CBNZ(OAK_WSCRATCH, no_hack);
		oakAsm->MOV(OAK_WSCRATCH, 0x3f490fda);
		oakAsm->FMOV(oakSRegister(regd), OAK_WSCRATCH);
		oakAsm->B(done);

		oakAsm->l(no_hack);
		recFpuMaddProduct_emit_oaknut(regd, sreg, treg, false);
		oakAsm->l(done);
	}
	else
	{
		recFpuMaddProduct_emit_oaknut(regd, sreg, treg, false);
	}
	recFpuFinishMulResult_emit_oaknut(regd);
	recEndOaknutEmit();

	_freeXMMreg(treg);
	_freeXMMreg(sreg);
}

static void recMUL_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MUL_F);
	recFpuMulExact_emit_oaknut(info, EEREC_D);
}

void recMUL_S_xmm(int info)
{
	recMUL_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(MUL_S, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

static void recMULA_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MULA_F);
	recFpuMulExact_emit_oaknut(info, EEREC_ACC);
}

void recMULA_S_xmm(int info)
{
	recMULA_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(MULA_S, XMMINFO_WRITEACC | XMMINFO_READS | XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// NEG XMM
//------------------------------------------------------------------
static void recNEG_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::NEG_F);

	const oak::SReg regd_s = oakSRegister(EEREC_D);
	const oak::QReg regd_q = oakQRegister(EEREC_D);

	recBeginOaknutEmit();
	if (info & PROCESS_EE_S)
	{
		oakAsm->MOV(regd_q.Selem()[0], oakQRegister(EEREC_S).Selem()[0]);
	}
	else
	{
		oakLoad32(OAK_WSCRATCH2, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr[_Fs_].UL))});
		oakAsm->FMOV(regd_s, OAK_WSCRATCH2);
	}

	oakAsm->FMOV(OAK_WSCRATCH2, regd_s);
	oakAsm->MOV(OAK_WSCRATCH, 0x80000000);
	oakAsm->EOR(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakAsm->FMOV(regd_s, OAK_WSCRATCH2);
	recEndOaknutEmit();

	// NEG.S preserves NaN/Inf payload bits and only toggles the sign bit.
}

void recNEG_S_xmm(int info)
{
	recNEG_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(NEG_S, XMMINFO_WRITED | XMMINFO_READS);
//------------------------------------------------------------------


//------------------------------------------------------------------
// SUB XMM
//------------------------------------------------------------------
static void recSUB_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::SUB_F);

	const int sreg = _allocTempXMMreg(XMMT_FPS);
	const int treg = _allocTempXMMreg(XMMT_FPS);
	const bool d_aliases_s = (_Fd_ == _Fs_);
	const bool d_aliases_t = (_Fd_ == _Ft_);

	if (d_aliases_s)
		recFpuFlushCachedOperandToMemory(_Fs_);
	if (d_aliases_t && _Ft_ != _Fs_)
		recFpuFlushCachedOperandToMemory(_Ft_);

	recBeginOaknutEmit();
	if (d_aliases_s)
		recFpuLoadScalarMemoryOperand_emit_oaknut(sreg, _Fs_);
	else
		recFpuLoadScalarOperand_emit_oaknut(sreg, _Fs_, EEREC_S, info & PROCESS_EE_S);
	if (d_aliases_t)
		recFpuLoadScalarMemoryOperand_emit_oaknut(treg, _Ft_);
	else
		recFpuLoadScalarOperand_emit_oaknut(treg, _Ft_, EEREC_T, info & PROCESS_EE_T);
	recFpuDoubleClampOperand_emit_oaknut(sreg);
	recFpuDoubleClampOperand_emit_oaknut(treg);
	if (FPU_CORRECT_ADD_SUB)
	{
		recFpuAddSubExact_emit_oaknut(EEREC_D, sreg, treg, true);
	}
	else
	{
		oakAsm->FSUB(oakSRegister(EEREC_D), oakSRegister(sreg), oakSRegister(treg));
	}
	recEndOaknutEmit();

	_freeXMMreg(treg);
	_freeXMMreg(sreg);
}

void recSUB_S_xmm(int info)
{
	recSUB_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(SUB_S, XMMINFO_WRITED | XMMINFO_READD | XMMINFO_READS | XMMINFO_READT);


static void recSUBA_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::SUBA_F);

	const int sreg = _allocTempXMMreg(XMMT_FPS);
	const int treg = _allocTempXMMreg(XMMT_FPS);

	recBeginOaknutEmit();
	recFpuLoadScalarOperand_emit_oaknut(sreg, _Fs_, EEREC_S, info & PROCESS_EE_S);
	recFpuLoadScalarOperand_emit_oaknut(treg, _Ft_, EEREC_T, info & PROCESS_EE_T);
	recFpuDoubleClampOperand_emit_oaknut(sreg);
	recFpuDoubleClampOperand_emit_oaknut(treg);
	if (FPU_CORRECT_ADD_SUB)
	{
		recFpuAddSubExact_emit_oaknut(EEREC_ACC, sreg, treg, true);
	}
	else
	{
		oakAsm->FSUB(oakSRegister(EEREC_ACC), oakSRegister(sreg), oakSRegister(treg));
	}
	recEndOaknutEmit();

	_freeXMMreg(treg);
	_freeXMMreg(sreg);
}

void recSUBA_S_xmm(int info)
{
	recSUBA_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(SUBA_S, XMMINFO_WRITEACC | XMMINFO_READS | XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// SQRT XMM
//------------------------------------------------------------------
static void recSQRT_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::SQRT_F);

	FPControlRegister sqrt_fpcr = EmuConfig.Cpu.FPUFPCR;
	sqrt_fpcr.SetRoundMode(FPRoundMode::Nearest);
	const int trawreg = _allocTempXMMreg(XMMT_FPS);

	recBeginOaknutEmit();
	recFpuLoadScalarOperand_emit_oaknut(EEREC_D, _Ft_, EEREC_T, info & PROCESS_EE_T);
	oakAsm->MOV(oakQRegister(trawreg).Selem()[0], oakQRegister(EEREC_D).Selem()[0]);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(EEREC_D));
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->CMP(OAK_WSCRATCH2, 0x00800000);
	oakAsm->CSET(oak::util::W4, oak::util::HS);
	oakAsm->TST(OAK_WSCRATCH, 0x80000000);
	oakAsm->CSEL(oak::util::W4, oak::util::W4, oak::util::WZR, oak::util::NE);
	oakAsm->CMP(oak::util::W4, 0);
	oakAsm->CSEL(OAK_WSCRATCH, OAK_WSCRATCH2, OAK_WSCRATCH, oak::util::NE);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);
	oakAsm->MOV(OAK_WSCRATCH2, FPUflagI | FPUflagSI);
	oakAsm->CSEL(OAK_WSCRATCH2, OAK_WSCRATCH2, oak::util::WZR, oak::util::NE);
	oakLoad32(OAK_WSCRATCH, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fprc[31]))});
	oakAsm->MOV(oak::util::W4, ~(FPUflagI | FPUflagD));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, oak::util::W4);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakStore32(OAK_WSCRATCH, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fprc[31]))});
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(EEREC_D));
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->CMP(OAK_WSCRATCH2, 0x00800000);
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH, 0x80000000);
	oakAsm->CSEL(OAK_WSCRATCH, OAK_WSCRATCH2, OAK_WSCRATCH, oak::util::LO);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);
	recFpuClampFloat3Operand_emit_oaknut(EEREC_D);
	if (EmuConfig.Cpu.FPUFPCR.bitmask != sqrt_fpcr.bitmask)
		recFpuSetFpcr_emit_oaknut(sqrt_fpcr.bitmask);
	oakAsm->FSQRT(oakSRegister(EEREC_D), oakSRegister(EEREC_D));
	if (EmuConfig.Cpu.FPUFPCR.bitmask != sqrt_fpcr.bitmask)
		recFpuSetFpcr_emit_oaknut(EmuConfig.Cpu.FPUFPCR.bitmask);
	recFpuFinishInterpreterResult_emit_oaknut(EEREC_D);
	oak::Label sqrt_not_max_mantissa;
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(trawreg));
	oakAsm->MOV(OAK_WSCRATCH2, 0x3fffffff);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, sqrt_not_max_mantissa);
	oakAsm->MOV(OAK_WSCRATCH, 0x3fb504f2);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);
	oakAsm->l(sqrt_not_max_mantissa);
	oak::Label sqrt_not_garbage2;
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(trawreg));
	oakAsm->MOV(OAK_WSCRATCH2, 0xdeadbeef);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, sqrt_not_garbage2);
	oakAsm->MOV(OAK_WSCRATCH, 0x4f152107);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);
	oakAsm->l(sqrt_not_garbage2);
	oak::Label sqrt_not_exp255;
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(trawreg));
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f800000);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, sqrt_not_exp255);
	oakAsm->MOV(OAK_WSCRATCH, 0x5f7fffff);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);
	oakAsm->l(sqrt_not_exp255);
	recEndOaknutEmit();

	_freeXMMreg(trawreg);
}

void recSQRT_S_xmm(int info)
{
	recSQRT_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(SQRT_S, XMMINFO_WRITED | XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// RSQRT XMM
//------------------------------------------------------------------
static void recRSQRT_S_emit_oaknut(int info)
{
	EE::Profiler.EmitOp(eeOpcode::RSQRT_F);

	const int sreg = _allocTempXMMreg(XMMT_FPS);
	const int treg = _allocTempXMMreg(XMMT_FPS);
	const int srawreg = _allocTempXMMreg(XMMT_FPS);
	const int trawreg = _allocTempXMMreg(XMMT_FPS);

	recBeginOaknutEmit();
	recFpuLoadScalarOperand_emit_oaknut(sreg, _Fs_, EEREC_S, info & PROCESS_EE_S);
	recFpuLoadScalarOperand_emit_oaknut(treg, _Ft_, EEREC_T, info & PROCESS_EE_T);
	oakAsm->MOV(oakQRegister(srawreg).Selem()[0], oakQRegister(sreg).Selem()[0]);
	oakAsm->MOV(oakQRegister(trawreg).Selem()[0], oakQRegister(treg).Selem()[0]);

	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(treg));
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->CMP(OAK_WSCRATCH2, 0x00800000);
	oakAsm->CSET(oak::util::W4, oak::util::HS);
	oakAsm->TST(OAK_WSCRATCH, 0x80000000);
	oakAsm->CSEL(oak::util::W4, oak::util::W4, oak::util::WZR, oak::util::NE);
	oakAsm->CMP(oak::util::W4, 0);
	oakAsm->CSEL(OAK_WSCRATCH, OAK_WSCRATCH2, OAK_WSCRATCH, oak::util::NE);
	oakAsm->FMOV(oakSRegister(treg), OAK_WSCRATCH);

	recFpuClampFloat3Operand_emit_oaknut(treg);
	recFpuDoubleClampOperand_emit_oaknut(sreg);
	oakAsm->FSQRT(oakSRegister(treg), oakSRegister(treg));
	oakAsm->FDIV(oakSRegister(EEREC_D), oakSRegister(sreg), oakSRegister(treg));
	recFpuFinishInterpreterResult_emit_oaknut(EEREC_D);

	oak::Label rsqrt_not_one_over_five;
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(srawreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->MOV(OAK_WSCRATCH2, 0x3f800000);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, rsqrt_not_one_over_five);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(trawreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->MOV(OAK_WSCRATCH2, 0x41c80000);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, rsqrt_not_one_over_five);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(srawreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->MOV(OAK_WSCRATCH2, 0x3e4ccccc);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);
	oakAsm->l(rsqrt_not_one_over_five);

	oak::Label rsqrt_not_one_over_max_mantissa;
	oak::Label rsqrt_one_over_exp255_max_mantissa;
	oak::Label rsqrt_one_over_max_mantissa_store;
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(srawreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->MOV(OAK_WSCRATCH2, 0x3f800000);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, rsqrt_not_one_over_max_mantissa);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(trawreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->MOV(OAK_WSCRATCH2, 0x3fffffff);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::EQ, rsqrt_one_over_max_mantissa_store);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7fffffff);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, rsqrt_not_one_over_max_mantissa);
	oakAsm->B(rsqrt_one_over_exp255_max_mantissa);
	oakAsm->l(rsqrt_one_over_max_mantissa_store);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(srawreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->MOV(OAK_WSCRATCH2, 0x3f3504f4);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);
	oakAsm->B(rsqrt_not_one_over_max_mantissa);
	oakAsm->l(rsqrt_one_over_exp255_max_mantissa);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(srawreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->MOV(OAK_WSCRATCH2, 0x1f800000);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);
	oakAsm->l(rsqrt_not_one_over_max_mantissa);

	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(trawreg));
	oakAsm->FMOV(OAK_WSCRATCH2, oakSRegister(trawreg));
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, 0x80000000);
	oakAsm->MOV(oak::util::W4, 0x7f7fffff);
	oakAsm->ORR(OAK_WSCRATCH2, OAK_WSCRATCH2, oak::util::W4);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->CMP(OAK_WSCRATCH, 0x00800000);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(EEREC_D));
	oakAsm->CSEL(OAK_WSCRATCH, OAK_WSCRATCH2, OAK_WSCRATCH, oak::util::LO);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);

	oak::Label rsqrt_not_special_over_one;
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(srawreg));
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f800000);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, rsqrt_not_special_over_one);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(trawreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->MOV(OAK_WSCRATCH2, 0x3f800000);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, rsqrt_not_special_over_one);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(srawreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f7fffff);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);
	oakAsm->l(rsqrt_not_special_over_one);

	oak::Label rsqrt_not_both_exp255;
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(srawreg));
	oakAsm->MOV(OAK_WSCRATCH2, 0x7f800000);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, rsqrt_not_both_exp255);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(trawreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->CMP(OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->B(oak::util::NE, rsqrt_not_both_exp255);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(srawreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x80000000);
	oakAsm->MOV(OAK_WSCRATCH2, 0x5f800000);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakAsm->FMOV(oakSRegister(EEREC_D), OAK_WSCRATCH);
	oakAsm->l(rsqrt_not_both_exp255);

	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(trawreg));
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->CMP(OAK_WSCRATCH2, 0x00800000);
	oakAsm->CSET(oak::util::W4, oak::util::HS);
	oakAsm->TST(OAK_WSCRATCH, 0x80000000);
	oakAsm->CSEL(oak::util::W4, oak::util::W4, oak::util::WZR, oak::util::NE);
	oakAsm->CMP(oak::util::W4, 0);
	oakAsm->MOV(OAK_WSCRATCH2, FPUflagI | FPUflagSI);
	oakAsm->CSEL(OAK_WSCRATCH2, OAK_WSCRATCH2, oak::util::WZR, oak::util::NE);
	oakAsm->FMOV(OAK_WSCRATCH, oakSRegister(trawreg));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x7fffffff);
	oakAsm->CMP(OAK_WSCRATCH, 0x00800000);
	oakAsm->MOV(oak::util::W4, FPUflagD | FPUflagSD);
	oakAsm->CSEL(oak::util::W4, oak::util::W4, oak::util::WZR, oak::util::LO);
	oakAsm->ORR(OAK_WSCRATCH2, OAK_WSCRATCH2, oak::util::W4);
	oakLoad32(OAK_WSCRATCH, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fprc[31]))});
	oakAsm->MOV(oak::util::W4, ~(FPUflagI | FPUflagD));
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, oak::util::W4);
	oakAsm->ORR(OAK_WSCRATCH, OAK_WSCRATCH, OAK_WSCRATCH2);
	oakStore32(OAK_WSCRATCH, {oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fprc[31]))});
	recEndOaknutEmit();

	_freeXMMreg(trawreg);
	_freeXMMreg(srawreg);
	_freeXMMreg(treg);
	_freeXMMreg(sreg);
}

void recRSQRT_S_xmm(int info)
{
	recRSQRT_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(RSQRT_S, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

#endif // FPU_RECOMPILE

} // namespace COP1
} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
