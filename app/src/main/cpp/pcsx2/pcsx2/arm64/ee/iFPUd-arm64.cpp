// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "arm64/OaknutHelpers-arm64.h"
#include "common/emitter/x86emitter.h"
#include "arm64/ee/iR5900-arm64.h"
#include "iFPU-arm64.h"

/* This is a version of the FPU that emulates an exponent of 0xff and overflow/underflow flags */

/* Can be made faster by not converting stuff back and forth between instructions. */


//----------------------------------------------------------------
// FPU emulation status:
// ADD, SUB (incl. accumulation stage of MADD/MSUB) - no known problems.
// Mul (incl. multiplication stage of MADD/MSUB) - incorrect. PS2's result mantissa is sometimes
//													smaller by 0x1 than IEEE's result (with round to zero).
// DIV, SQRT, RSQRT - incorrect. PS2's result varies between IEEE's result with round to zero
//													and IEEE's result with round to +/-infinity.
// other stuff - no known problems.
//----------------------------------------------------------------


#if !defined(__ANDROID__)
using namespace x86Emitter;
#endif

// Set overflow flag (define only if FPU_RESULT is 1)
#define FPU_FLAGS_OVERFLOW 1
// Set underflow flag (define only if FPU_RESULT is 1)
#define FPU_FLAGS_UNDERFLOW 1

// If 1, result is not clamped (Gives correct results as in PS2,
// but can cause problems due to insufficient clamping levels in the VUs)
#define FPU_RESULT 1

// Set I&D flags. also impacts other aspects of DIV/R/SQRT correctness
#define FPU_FLAGS_ID 1

// Add/Sub opcodes produce the same results as the ps2
#define FPU_CORRECT_ADD_SUB CHECK_FPU_OVERFLOW

#ifdef FPU_RECOMPILE

//------------------------------------------------------------------
namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {
namespace COP1 {

namespace DOUBLE {

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

#define OAK_CPU(field) OakMemOperand{oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, field))}

static constexpr oak::WReg OAK_EAX = oak::util::W0;
static constexpr oak::WReg OAK_ECX = oak::util::W1;
static constexpr oak::WReg OAK_EDX = oak::util::W2;

static oak::QReg fpuDoubleScratchQ_emit_oaknut(int avoid0 = -1, int avoid1 = -1)
{
	if (avoid0 != OAK_QSCRATCH.index() && avoid1 != OAK_QSCRATCH.index())
		return OAK_QSCRATCH;
	if (avoid0 != OAK_QSCRATCH2.index() && avoid1 != OAK_QSCRATCH2.index())
		return OAK_QSCRATCH2;
	return OAK_QSCRATCH3;
}

static oak::QReg fpuDoubleLoadConstQ_emit_oaknut(OakMemOperand mem, oak::QReg avoid0 = oak::QReg(31), oak::QReg avoid1 = oak::QReg(31))
{
	const oak::QReg scratch = fpuDoubleScratchQ_emit_oaknut(avoid0.index(), avoid1.index());
	oakLoad128(scratch, mem);
	return scratch;
}

static oak::SReg fpuDoubleLoadConstS_emit_oaknut(OakMemOperand mem, oak::QReg avoid0 = oak::QReg(31), oak::QReg avoid1 = oak::QReg(31))
{
	return oakSRegister(fpuDoubleLoadConstQ_emit_oaknut(mem, avoid0, avoid1).index());
}

static oak::DReg fpuDoubleLoadConstD_emit_oaknut(OakMemOperand mem, oak::QReg avoid0 = oak::QReg(31), oak::QReg avoid1 = oak::QReg(31))
{
	return oakDRegister(fpuDoubleLoadConstQ_emit_oaknut(mem, avoid0, avoid1).index());
}

static void fpuDoubleClearFcr31Flags_emit_oaknut(u32 mask)
{
	oakLoad32(OAK_WSCRATCH2, OAK_CPU(fpuRegs.fprc[31]));
	oakAsm->MOV(OAK_WSCRATCH, ~mask);
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakStore32(OAK_WSCRATCH2, OAK_CPU(fpuRegs.fprc[31]));
}

static void fpuDoubleSetFcr31Flags_emit_oaknut(u32 mask)
{
	oakLoad32(OAK_WSCRATCH2, OAK_CPU(fpuRegs.fprc[31]));
	oakAsm->MOV(OAK_WSCRATCH, mask);
	oakAsm->ORR(OAK_WSCRATCH2, OAK_WSCRATCH2, OAK_WSCRATCH);
	oakStore32(OAK_WSCRATCH2, OAK_CPU(fpuRegs.fprc[31]));
}

static void fpuDoubleClearAccFlag_emit_oaknut()
{
	oakLoad32(OAK_WSCRATCH2, OAK_CPU(fpuRegs.ACCflag));
	oakAsm->AND(OAK_WSCRATCH2, OAK_WSCRATCH2, ~1u);
	oakStore32(OAK_WSCRATCH2, OAK_CPU(fpuRegs.ACCflag));
}

static void fpuDoubleSetAccFlag_emit_oaknut()
{
	oakLoad32(OAK_WSCRATCH2, OAK_CPU(fpuRegs.ACCflag));
	oakAsm->ORR(OAK_WSCRATCH2, OAK_WSCRATCH2, 1);
	oakStore32(OAK_WSCRATCH2, OAK_CPU(fpuRegs.ACCflag));
}

static void fpuDoubleLoadFpcr_emit_oaknut(OakMemOperand mem)
{
	oakLoad64(OAK_XSCRATCH, mem);
	oakAsm->MSR(oak::SystemReg::FPCR, OAK_XSCRATCH);
	oakAsm->ISB();
}

static void fpuDoubleLoadScalarFromFpr_emit_oaknut(int hostreg, int fpureg)
{
	oakLoad32(OAK_WSCRATCH2, OakMemOperand{oak::util::X27, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs.fpr) + sizeof(fpuRegs.fpr[0]) * fpureg)});
	oakAsm->FMOV(oakSRegister(hostreg), OAK_WSCRATCH2);
}

static void fpuDoubleFlushCachedFprToMemory_emit_oaknut(int fpureg)
{
	for (u32 i = 0; i < iREGCNT_XMM; i++)
	{
		if (xmmregs[i].inuse && xmmregs[i].type == XMMTYPE_FPREG && xmmregs[i].reg == fpureg && (xmmregs[i].mode & MODE_WRITE))
			_writebackXMMreg(i);
	}
}

static void fpuDoubleLoadAcc_emit_oaknut(int hostreg)
{
	oakLoad32(OAK_WSCRATCH2, OAK_CPU(fpuRegs.ACC));
	oakAsm->FMOV(oakSRegister(hostreg), OAK_WSCRATCH2);
}

static void fpuDoubleMovmskps_emit_oaknut(oak::WReg dst, oak::QReg src)
{
	oakAsm->MOV(OAK_XSCRATCH, 0x0000000200000001ull);
	oakAsm->INS(OAK_QSCRATCH2.Delem()[0], OAK_XSCRATCH);
	oakAsm->MOV(OAK_XSCRATCH, 0x0000000800000004ull);
	oakAsm->INS(OAK_QSCRATCH2.Delem()[1], OAK_XSCRATCH);
	oakAsm->SSHR(OAK_QSCRATCH.S4(), src.S4(), 31);
	oakAsm->AND(OAK_QSCRATCH.B16(), OAK_QSCRATCH2.B16(), OAK_QSCRATCH.B16());
	oakAsm->ADDV(OAK_SSCRATCH, OAK_QSCRATCH.S4());
	oakAsm->FMOV(dst, OAK_SSCRATCH);
}

static void fpuDoublePshufd_emit_oaknut(oak::QReg dst, oak::QReg src, int pIndex)
{
	oakLoad128(OAK_QSCRATCH3, OAK_CPU(shuffle.data[pIndex][0]));
	oakAsm->TBL(dst.B16(), oak::List(src.B16()), OAK_QSCRATCH3.B16());
}

//------------------------------------------------------------------

//------------------------------------------------------------------
// *FPU Opcodes!*
//------------------------------------------------------------------

//------------------------------------------------------------------
// PS2 -> DOUBLE
//------------------------------------------------------------------

//#define SINGLE(sign, exp, mant) (((u32)(sign) << 31) | ((u32)(exp) << 23) | (u32)(mant))
//#define DOUBLE(sign, exp, mant) (((sign##ULL) << 63) | ((exp##ULL) << 52) | (mant##ULL))

//struct FPUd_Globals
//{
//	u32 neg[4], pos[4];
//
//	u32 pos_inf[4], neg_inf[4],
//	    one_exp[4];
//
//	u64 dbl_one_exp[2];
//
//	u64 dbl_cvt_overflow, // needs special code if above or equal
//	    dbl_ps2_overflow, // overflow & clamp if above or equal
//	    dbl_underflow;    // underflow if below
//
//	u64 padding;
//
//	u64 dbl_s_pos[2];
//	//u64		dlb_s_neg[2];
//};

//alignas(32) static const FPUd_Globals s_const =
//{
//	{0x80000000, 0xffffffff, 0xffffffff, 0xffffffff},
//	{0x7fffffff, 0xffffffff, 0xffffffff, 0xffffffff},
//
//	{SINGLE(0, 0xff, 0), 0, 0, 0},
//	{SINGLE(1, 0xff, 0), 0, 0, 0},
//	{SINGLE(0,    1, 0), 0, 0, 0},
//
//	{DOUBLE(0, 1, 0), 0},
//
//	DOUBLE(0, 1151, 0), // cvt_overflow
//	DOUBLE(0, 1152, 0), // ps2_overflow
//	DOUBLE(0,  897, 0), // underflow
//
//	0,                  // Padding!!
//
//	{0x7fffffffffffffffULL, 0},
//	//{0x8000000000000000ULL, 0},
//};


// ToDouble : converts single-precision PS2 float to double-precision IEEE float

void ToDouble(int reg)
{
	const oak::QReg regQ = oakQRegister(reg);

	oakAsm->FCMP(oakSRegister(reg), fpuDoubleLoadConstS_emit_oaknut(OAK_CPU(mVUss4.s_const.pos_inf), regQ));
//	u8* to_complex = JE8(0); // Complex conversion if positive infinity or NaN
	oak::Label to_complex;
	oakAsm->B(oak::util::EQ, to_complex);
	oakAsm->B(oak::util::VS, to_complex);
	oakAsm->FCMP(oakSRegister(reg), fpuDoubleLoadConstS_emit_oaknut(OAK_CPU(mVUss4.s_const.neg_inf), regQ));
//	u8* to_complex2 = JE8(0); // Complex conversion if negative infinity
	oak::Label to_complex2;
	oakAsm->B(oak::util::EQ, to_complex2);
	oakAsm->B(oak::util::VS, to_complex2);

	oakAsm->FCVT(oakDRegister(reg), oakSRegister(reg));
//	u8* end = JMP8(0);
	oak::Label end;
	oakAsm->B(end);

//	x86SetJ8(to_complex);
	oakAsm->l(to_complex);
//	x86SetJ8(to_complex2);
	oakAsm->l(to_complex2);

	// Special conversion for when IEEE sees the value in reg as an INF/NaN
	oakAsm->SUB(regQ.S4(), regQ.S4(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.one_exp), regQ).S4());
	oakAsm->FCVT(oakDRegister(reg), oakSRegister(reg));
	oakAsm->FADD(regQ.D2(), regQ.D2(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.dbl_one_exp), regQ).D2());

//	x86SetJ8(end);
	oakAsm->l(end);
}

//------------------------------------------------------------------
// DOUBLE -> PS2
//------------------------------------------------------------------

// If FPU_RESULT is defined, results are more like the real PS2's FPU.
// But new issues may happen if the VU isn't clamping all operands since games may transfer FPU results into the VU.
// Ar tonelico 1 does this with the result from DIV/RSQRT (when a division by zero occurs).
// Otherwise, results are still usually better than iFPU-arm64.cpp.

// ToPS2FPU_Full - converts double-precision IEEE float to single-precision PS2 float

// converts small normal numbers to PS2 equivalent
// converts large normal numbers to PS2 equivalent (which represent NaN/inf in IEEE)
// converts really large normal numbers to PS2 signed max
// converts really small normal numbers to zero (flush)
// doesn't handle inf/nan/denormal

void ToPS2FPU_Full(int reg, bool flags, int absreg, bool acc, bool addsub)
{
	const oak::QReg regQ = oakQRegister(reg);
	const oak::QReg regAbs = oakQRegister(absreg);

	if (flags) {
		fpuDoubleClearFcr31Flags_emit_oaknut(FPUflagO | FPUflagU);
	}
	if (flags && acc) {
		fpuDoubleClearAccFlag_emit_oaknut();
	}

	oakAsm->MOV(regAbs.B16(), regQ.B16());
	oakAsm->AND(regAbs.B16(), regAbs.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.dbl_s_pos), regAbs).B16());

	oakAsm->FCMP(oakDRegister(absreg), fpuDoubleLoadConstD_emit_oaknut(OAK_CPU(mVUss4.s_const.dbl_cvt_overflow), regAbs));
//	u8* to_complex = JAE8(0);
	oak::Label to_complex;
	oakAsm->B(oak::util::CS, to_complex);

	oakAsm->FCMP(oakDRegister(absreg), fpuDoubleLoadConstD_emit_oaknut(OAK_CPU(mVUss4.s_const.dbl_underflow), regAbs));
//	u8* to_underflow = JB8(0);
	oak::Label to_underflow;
	oakAsm->B(oak::util::CC, to_underflow);

	oakAsm->FCVT(oakSRegister(reg), oakDRegister(reg));

//	u32* end = JMP32(0);
	oak::Label end;
	oakAsm->B(end);

//	x86SetJ8(to_complex);
	oakAsm->l(to_complex);
	oakAsm->FCMP(oakDRegister(absreg), fpuDoubleLoadConstD_emit_oaknut(OAK_CPU(mVUss4.s_const.dbl_ps2_overflow), regAbs));
//	u8* to_overflow = JAE8(0);
	oak::Label to_overflow;
	oakAsm->B(oak::util::CS, to_overflow);

	oakAsm->SUB(regQ.D2(), regQ.D2(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.dbl_one_exp), regQ).D2());
	oakAsm->FCVT(oakSRegister(reg), oakDRegister(reg));
	oakAsm->ADD(regQ.S4(), regQ.S4(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.one_exp), regQ).S4());

//	u32* end2 = JMP32(0);
	oak::Label end2;
	oakAsm->B(end2);

//	x86SetJ8(to_overflow);
	oakAsm->l(to_overflow);
	oakAsm->FCVT(oakSRegister(reg), oakDRegister(reg));
	oakAsm->ORR(regQ.B16(), regQ.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.pos), regQ).B16());
	if (flags && FPU_FLAGS_OVERFLOW) {
		fpuDoubleSetFcr31Flags_emit_oaknut(FPUflagO | FPUflagSO);
	}
	if (flags && FPU_FLAGS_OVERFLOW && acc) {
		fpuDoubleSetAccFlag_emit_oaknut();
	}
//	u8* end3 = JMP8(0);
	oak::Label end3;
	oakAsm->B(end3);

//	x86SetJ8(to_underflow);
	oakAsm->l(to_underflow);
//	u8* end4 = nullptr;
	oak::Label end4;
	if (flags && FPU_FLAGS_UNDERFLOW) //set underflow flags if not zero
	{
		oakAsm->EOR(regAbs.B16(), regAbs.B16(), regAbs.B16());
		oakAsm->FCMP(oakDRegister(reg), oakDRegister(absreg));
//		u8* is_zero = JE8(0);
		oak::Label is_zero;
		oakAsm->B(oak::util::EQ, is_zero);

		fpuDoubleSetFcr31Flags_emit_oaknut(FPUflagU | FPUflagSU);
		if (addsub)
		{
			//On ADD/SUB, the PS2 simply leaves the mantissa bits as they are (after normalization)
			//IEEE either clears them (FtZ) or returns the denormalized result.
			//not thoroughly tested : other operations such as MUL and DIV seem to clear all mantissa bits?
			oakAsm->MOV(regAbs.B16(), regQ.B16());
			oakAsm->SHL(regQ.S4(), regQ.S4(), 12);
			oakAsm->USHR(regQ.D2(), regQ.D2(), 41);
			oakAsm->USHR(regAbs.D2(), regAbs.D2(), 63);
			oakAsm->SHL(regAbs.S4(), regAbs.S4(), 31);
			oakAsm->ORR(regQ.B16(), regQ.B16(), regAbs.B16());
//			end4 = JMP8(0);
			oakAsm->B(end4);
		}

//		x86SetJ8(is_zero);
		oakAsm->l(is_zero);
	}
	oakAsm->FCVT(oakSRegister(reg), oakDRegister(reg));
	oakAsm->AND(regQ.B16(), regQ.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.neg), regQ).B16());

//	x86SetJ32(end);
	oakAsm->l(end);
//	x86SetJ32(end2);
	oakAsm->l(end2);

//	x86SetJ8(end3);
	oakAsm->l(end3);
	if (flags && FPU_FLAGS_UNDERFLOW && addsub) {
//        x86SetJ8(end4);
		oakAsm->l(end4);
	}
}

void ToPS2FPU(int reg, bool flags, int absreg, bool acc, bool addsub = false)
{
	if (FPU_RESULT) {
        ToPS2FPU_Full(reg, flags, absreg, acc, addsub);
	}
	else
	{
		const oak::QReg regQ = oakQRegister(reg);

		oakAsm->FCVT(oakSRegister(reg), oakDRegister(reg));
		oakAsm->FMINNM(oakSRegister(reg), oakSRegister(reg), fpuDoubleLoadConstS_emit_oaknut(OAK_CPU(mVUss4.g_maxvals[0]), regQ));
		oakAsm->FMAXNM(oakSRegister(reg), oakSRegister(reg), fpuDoubleLoadConstS_emit_oaknut(OAK_CPU(mVUss4.g_minvals[0]), regQ));
	}
}

//sets the maximum (positive or negative) value into regd.
void SetMaxValue(int regd)
{
	const oak::QReg regQ = oakQRegister(regd);

	if (FPU_RESULT) {
		oakAsm->ORR(regQ.B16(), regQ.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.pos[0]), regQ).B16());
	}
	else
	{
		oakAsm->AND(regQ.B16(), regQ.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.neg[0]), regQ).B16());
		oakAsm->ORR(regQ.B16(), regQ.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.g_maxvals[0]), regQ).B16());
	}
}

#define GET_S(sreg) \
	do { \
		if (info & PROCESS_EE_S) \
			oakAsm->MOV(oakQRegister(sreg).Selem()[0], oakQRegister(EEREC_S).Selem()[0]); \
		else \
			fpuDoubleLoadScalarFromFpr_emit_oaknut(sreg, _Fs_); \
	} while (0)

#define ALLOC_S(sreg) \
	do { \
		(sreg) = _allocTempXMMreg(XMMT_FPS); \
		GET_S(sreg); \
	} while (0)

#define GET_T(treg) \
	do { \
		if (info & PROCESS_EE_T) \
			oakAsm->MOV(oakQRegister(treg).Selem()[0], oakQRegister(EEREC_T).Selem()[0]); \
		else \
			fpuDoubleLoadScalarFromFpr_emit_oaknut(treg, _Ft_); \
	} while (0)

#define ALLOC_T(treg) \
	do { \
		(treg) = _allocTempXMMreg(XMMT_FPS); \
		GET_T(treg); \
	} while (0)

#define GET_ACC(areg) \
	do { \
		if (info & PROCESS_EE_ACC) \
			oakAsm->MOV(oakQRegister(areg).Selem()[0], oakQRegister(EEREC_ACC).Selem()[0]); \
		else \
			fpuDoubleLoadAcc_emit_oaknut(areg); \
	} while (0)

#define ALLOC_ACC(areg) \
	do { \
		(areg) = _allocTempXMMreg(XMMT_FPS); \
		GET_ACC(areg); \
	} while (0)

#define CLEAR_OU_FLAGS \
	do { \
		fpuDoubleClearFcr31Flags_emit_oaknut(FPUflagO | FPUflagU); \
	} while (0)


//------------------------------------------------------------------
// ABS XMM
//------------------------------------------------------------------
void recABS_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::ABS_F);
	GET_S(EEREC_D);

	CLEAR_OU_FLAGS;

	const oak::QReg regED = oakQRegister(EEREC_D);
	oakAsm->AND(regED.B16(), regED.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.pos), regED).B16());
}

FPURECOMPILE_CONSTCODE(ABS_S, XMMINFO_WRITED | XMMINFO_READS);
//------------------------------------------------------------------


//------------------------------------------------------------------
// FPU_ADD_SUB (Used to mimic PS2's FPU add/sub behavior)
//------------------------------------------------------------------
// Compliant IEEE FPU uses, in computations, uses additional "guard" bits to the right of the mantissa
// but EE-FPU doesn't. Substraction (and addition of positive and negative) may shift the mantissa left,
// causing those bits to appear in the result; this function masks out the bits of the mantissa that will
// get shifted right to the guard bits to ensure that the guard bits are empty.
// The difference of the exponents = the amount that the smaller operand will be shifted right by.
// Modification - the PS2 uses a single guard bit? (Coded by Nneeve)
//------------------------------------------------------------------
void FPU_ADD_SUB(int tempd, int tempt) //tempd and tempt are overwritten, they are floats
{
	oak::Label j8Ptr0, j8Ptr1, j8Ptr2, j8Ptr3, j8Ptr4, j8Ptr5, j8Ptr6;

	const oak::QReg regD = oakQRegister(tempd);
	const oak::QReg regT = oakQRegister(tempt);

	const int xmmtemp = _allocTempXMMreg(XMMT_FPS); //temporary for anding with regd/regt
	const oak::QReg regTemp = oakQRegister(xmmtemp);

	oakAsm->FMOV(OAK_ECX, oakSRegister(tempd));
	oakAsm->FMOV(OAK_EAX, oakSRegister(tempt));

	//mask the exponents
	oakAsm->LSR(OAK_ECX, OAK_ECX, 23);
	oakAsm->LSR(OAK_EAX, OAK_EAX, 23);
	oakAsm->AND(OAK_ECX, OAK_ECX, 0xff);
	oakAsm->AND(OAK_EAX, OAK_EAX, 0xff);

	oakAsm->SUB(OAK_ECX, OAK_ECX, OAK_EAX);
	oakAsm->CMP(OAK_ECX, 25);
//	j8Ptr[0] = JGE8(0);
	oakAsm->B(oak::util::GE, j8Ptr0);
	oakAsm->CMP(OAK_ECX, 0);
//	j8Ptr[1] = JG8(0);
	oakAsm->B(oak::util::GT, j8Ptr1);
//	j8Ptr[2] = JE8(0);
	oakAsm->B(oak::util::EQ, j8Ptr2);
	oakAsm->CMN(OAK_ECX, 25);
//	j8Ptr[3] = JLE8(0);
	oakAsm->B(oak::util::LE, j8Ptr3);

	//diff = -24 .. -1 , expd < expt
	oakAsm->NEG(OAK_ECX, OAK_ECX);
	oakAsm->SUB(OAK_ECX, OAK_ECX, 1);
	oakAsm->MOV(OAK_EAX, 0xffffffff);
	oakAsm->LSL(OAK_EAX, OAK_EAX, OAK_ECX);
	oakAsm->FMOV(oakSRegister(xmmtemp), OAK_EAX);
	oakAsm->AND(regD.B16(), regD.B16(), regTemp.B16());
//	j8Ptr[4] = JMP8(0);
	oakAsm->B(j8Ptr4);

//	x86SetJ8(j8Ptr[0]);
	oakAsm->l(j8Ptr0);
    //diff = 25 .. 255 , expt < expd
	oakAsm->AND(regT.B16(), regT.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.neg), regT).B16());
//	j8Ptr[5] = JMP8(0);
	oakAsm->B(j8Ptr5);

//	x86SetJ8(j8Ptr[1]);
	oakAsm->l(j8Ptr1);
    //diff = 1 .. 24, expt < expd
	oakAsm->SUB(OAK_ECX, OAK_ECX, 1);
	oakAsm->MOV(OAK_EAX, 0xffffffff);
	oakAsm->LSL(OAK_EAX, OAK_EAX, OAK_ECX);
	oakAsm->FMOV(oakSRegister(xmmtemp), OAK_EAX);
	oakAsm->AND(regT.B16(), regT.B16(), regTemp.B16());
//	j8Ptr[6] = JMP8(0);
	oakAsm->B(j8Ptr6);

//	x86SetJ8(j8Ptr[3]);
	oakAsm->l(j8Ptr3);
    //diff = -255 .. -25, expd < expt
	oakAsm->AND(regD.B16(), regD.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.neg), regD).B16());

//	x86SetJ8(j8Ptr[2]);
	oakAsm->l(j8Ptr2);
    //diff == 0

//	x86SetJ8(j8Ptr[4]);
	oakAsm->l(j8Ptr4);
//	x86SetJ8(j8Ptr[5]);
	oakAsm->l(j8Ptr5);
//	x86SetJ8(j8Ptr[6]);
	oakAsm->l(j8Ptr6);

	_freeXMMreg(xmmtemp);
}

void FPU_MUL(int info, int regd, int sreg, int treg, bool acc)
{
//	u32* endMul = nullptr;
	oak::Label endMul;

	const oak::QReg regD = oakQRegister(regd);
	const oak::QReg regS = oakQRegister(sreg);

	if (CHECK_FPUMULHACK)
	{
		// 	if ((s == 0x3e800000) && (t == 0x40490fdb))
		// 		return 0x3f490fda; // needed for Tales of Destiny Remake (only in a very specific room late-game)
		// 	else
		// 		return 0;

//		alignas(16) static constexpr const u32 result[4] = { 0x3f490fda };

		oak::Label noHack;
		oakAsm->FMOV(OAK_ECX, oakSRegister(sreg));
		oakAsm->MOV(OAK_EDX, 0x3e800000u);
		oakAsm->CMP(OAK_ECX, OAK_EDX);
		oakAsm->B(oak::util::NE, noHack);
		oakAsm->FMOV(OAK_ECX, oakSRegister(treg));
		oakAsm->MOV(OAK_EDX, 0x40490fdbu);
		oakAsm->CMP(OAK_ECX, OAK_EDX);
		oakAsm->B(oak::util::NE, noHack);
			oakLoad128(regD, OAK_CPU(mVUss4.result));
//			endMul = JMP32(0);
			oakAsm->B(endMul);
//		x86SetJ8(noHack);
		oakAsm->l(noHack);
	}

	ToDouble(sreg); ToDouble(treg);
	oakAsm->FMUL(oakDRegister(sreg), oakDRegister(sreg), oakDRegister(treg));
	ToPS2FPU(sreg, true, treg, acc);
	oakAsm->MOV(regD.Selem()[0], regS.Selem()[0]);

	if (CHECK_FPUMULHACK) {
//        x86SetJ32(endMul);
		oakAsm->l(endMul);
	}
}

//------------------------------------------------------------------
// ADD XMM
//------------------------------------------------------------------
static void recADD_S_emit_oaknut(int info)
{
	int sreg, treg;
	const bool d_aliases_s = (_Fd_ == _Fs_);
	const bool d_aliases_t = (_Fd_ == _Ft_);

	if (d_aliases_s)
		fpuDoubleFlushCachedFprToMemory_emit_oaknut(_Fs_);
	if (d_aliases_t && _Ft_ != _Fs_)
		fpuDoubleFlushCachedFprToMemory_emit_oaknut(_Ft_);

	sreg = _allocTempXMMreg(XMMT_FPS);
	if (d_aliases_s)
		fpuDoubleLoadScalarFromFpr_emit_oaknut(sreg, _Fs_);
	else
		GET_S(sreg);

	treg = _allocTempXMMreg(XMMT_FPS);
	if (d_aliases_t)
		fpuDoubleLoadScalarFromFpr_emit_oaknut(treg, _Ft_);
	else
		GET_T(treg);

	if (FPU_CORRECT_ADD_SUB)
		FPU_ADD_SUB(sreg, treg);

	ToDouble(sreg);
	ToDouble(treg);
	oakAsm->FADD(oakDRegister(sreg), oakDRegister(sreg), oakDRegister(treg));

	ToPS2FPU(sreg, true, treg, false, true);
	oakAsm->MOV(oakQRegister(EEREC_D).Selem()[0], oakQRegister(sreg).Selem()[0]);

	_freeXMMreg(sreg);
	_freeXMMreg(treg);
}

void recADD_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::ADD_F);
	recADD_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(ADD_S, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

static void recADDA_S_emit_oaknut(int info)
{
	int sreg, treg;
	ALLOC_S(sreg);
	ALLOC_T(treg);

	if (FPU_CORRECT_ADD_SUB)
		FPU_ADD_SUB(sreg, treg);

	ToDouble(sreg);
	ToDouble(treg);
	oakAsm->FADD(oakDRegister(sreg), oakDRegister(sreg), oakDRegister(treg));

	ToPS2FPU(sreg, true, treg, true, true);
	oakAsm->MOV(oakQRegister(EEREC_ACC).Selem()[0], oakQRegister(sreg).Selem()[0]);

	_freeXMMreg(sreg);
	_freeXMMreg(treg);
}

void recADDA_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::ADDA_F);
	recADDA_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(ADDA_S, XMMINFO_WRITEACC | XMMINFO_READS | XMMINFO_READT);
//------------------------------------------------------------------

void recCMP(int info)
{
	int sreg, treg;
	ALLOC_S(sreg); ALLOC_T(treg);
	ToDouble(sreg); ToDouble(treg);

	oakAsm->FCMP(oakDRegister(sreg), oakDRegister(treg));

	_freeXMMreg(sreg); _freeXMMreg(treg);
}

//------------------------------------------------------------------
// C.x.S XMM
//------------------------------------------------------------------
void recC_EQ_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::CEQ_F);
	recCMP(info);

	oak::Label j8Ptr0, j8Ptr1;

//	j8Ptr[0] = JZ8(0);
	oakAsm->B(oak::util::EQ, j8Ptr0);
	oakAsm->B(oak::util::VS, j8Ptr0);
	fpuDoubleClearFcr31Flags_emit_oaknut(FPUflagC);
//		j8Ptr[1] = JMP8(0);
	oakAsm->B(j8Ptr1);
//	x86SetJ8(j8Ptr[0]);
	oakAsm->l(j8Ptr0);
	fpuDoubleSetFcr31Flags_emit_oaknut(FPUflagC);
//	x86SetJ8(j8Ptr[1]);
	oakAsm->l(j8Ptr1);
}

FPURECOMPILE_CONSTCODE(C_EQ, XMMINFO_READS | XMMINFO_READT);

void recC_LE_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::CLE_F);
	recCMP(info);

	oak::Label j8Ptr0, j8Ptr1;

//	j8Ptr[0] = JBE8(0);
	oakAsm->B(oak::util::LS, j8Ptr0);
	oakAsm->B(oak::util::VS, j8Ptr0);
	fpuDoubleClearFcr31Flags_emit_oaknut(FPUflagC);
//		j8Ptr[1] = JMP8(0);
	oakAsm->B(j8Ptr1);
//	x86SetJ8(j8Ptr[0]);
	oakAsm->l(j8Ptr0);
	fpuDoubleSetFcr31Flags_emit_oaknut(FPUflagC);
//	x86SetJ8(j8Ptr[1]);
	oakAsm->l(j8Ptr1);
}

FPURECOMPILE_CONSTCODE(C_LE, XMMINFO_READS | XMMINFO_READT);

void recC_LT_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::CLT_F);
	recCMP(info);

	oak::Label j8Ptr0, j8Ptr1;

//	j8Ptr[0] = JB8(0);
	oakAsm->B(oak::util::CC, j8Ptr0);
	oakAsm->B(oak::util::VS, j8Ptr0);
	fpuDoubleClearFcr31Flags_emit_oaknut(FPUflagC);
//		j8Ptr[1] = JMP8(0);
	oakAsm->B(j8Ptr1);
//	x86SetJ8(j8Ptr[0]);
	oakAsm->l(j8Ptr0);
	fpuDoubleSetFcr31Flags_emit_oaknut(FPUflagC);
//	x86SetJ8(j8Ptr[1]);
	oakAsm->l(j8Ptr1);
}

FPURECOMPILE_CONSTCODE(C_LT, XMMINFO_READS | XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// CVT.x XMM
//------------------------------------------------------------------

// CVT.S: Identical to non-double variant, omitted
// CVT.W: Identical to non-double variant, omitted

//------------------------------------------------------------------


//------------------------------------------------------------------
// DIV XMM
//------------------------------------------------------------------
static void recDIV_S_flags_emit_oaknut(int regd, int regt)
{
//	u8 *pjmp1, *pjmp2;
//	u32 *ajmp32, *bjmp32;
	oak::Label pjmp1, pjmp2;
	oak::Label ajmp32, bjmp32;

	const oak::QReg regD = oakQRegister(regd);
	const oak::QReg regT = oakQRegister(regt);

	const int t1reg = _allocTempXMMreg(XMMT_FPS);
	const oak::QReg regT1 = oakQRegister(t1reg);

	fpuDoubleClearFcr31Flags_emit_oaknut(FPUflagI | FPUflagD);

	//--- Check for divide by zero ---
	oakAsm->EOR(regT1.B16(), regT1.B16(), regT1.B16());
	oakAsm->FCMEQ(oakSRegister(t1reg), oakSRegister(t1reg), oakSRegister(regt));
	fpuDoubleMovmskps_emit_oaknut(OAK_EAX, regT1);
	oakAsm->AND(OAK_EAX, OAK_EAX, 1);
//	ajmp32 = JZ32(0); //Skip if not set
	oakAsm->CBZ(OAK_EAX, ajmp32);

		//--- Check for 0/0 ---
		oakAsm->EOR(regT1.B16(), regT1.B16(), regT1.B16());
		oakAsm->FCMEQ(oakSRegister(t1reg), oakSRegister(t1reg), oakSRegister(regd));
		fpuDoubleMovmskps_emit_oaknut(OAK_EAX, regT1);
		oakAsm->AND(OAK_EAX, OAK_EAX, 1);
//		pjmp1 = JZ8(0); //Skip if not set
		oakAsm->CBZ(OAK_EAX, pjmp1);
			fpuDoubleSetFcr31Flags_emit_oaknut(FPUflagI | FPUflagSI);
//			pjmp2 = JMP8(0);
			oakAsm->B(pjmp2);
//		x86SetJ8(pjmp1); //x/0 but not 0/0
		oakAsm->l(pjmp1);
			fpuDoubleSetFcr31Flags_emit_oaknut(FPUflagD | FPUflagSD);
//		x86SetJ8(pjmp2);
		oakAsm->l(pjmp2);

		//--- Make regd +/- Maximum ---
		oakAsm->EOR(regD.B16(), regD.B16(), regT.B16());
		SetMaxValue(regd); //clamp to max
//		bjmp32 = JMP32(0);
		oakAsm->B(bjmp32);

//	x86SetJ32(ajmp32);
	oakAsm->l(ajmp32);

	//--- Normal Divide ---
	ToDouble(regd); ToDouble(regt);

	oakAsm->FDIV(oakDRegister(regd), oakDRegister(regd), oakDRegister(regt));

	ToPS2FPU(regd, false, regt, false);

//	x86SetJ32(bjmp32);
	oakAsm->l(bjmp32);

	_freeXMMreg(t1reg);
}

static void recDIV_S_no_flags_emit_oaknut(int regd, int regt)
{
	ToDouble(regd); ToDouble(regt);

	oakAsm->FDIV(oakDRegister(regd), oakDRegister(regd), oakDRegister(regt));

	ToPS2FPU(regd, false, regt, false);
}

alignas(16) static FPControlRegister roundmode_nearest;

static void recDIV_S_emit_oaknut(int info)
{
	if (EmuConfig.Cpu.FPUFPCR.bitmask != EmuConfig.Cpu.FPUDivFPCR.bitmask) {
		fpuDoubleLoadFpcr_emit_oaknut(OAK_CPU(Cpu.FPUDivFPCR.bitmask));
	}

	int sreg, treg;

	ALLOC_S(sreg); ALLOC_T(treg);

	if (FPU_FLAGS_ID)
		recDIV_S_flags_emit_oaknut(sreg, treg);
	else
		recDIV_S_no_flags_emit_oaknut(sreg, treg);

	oakAsm->MOV(oakQRegister(EEREC_D).Selem()[0], oakQRegister(sreg).Selem()[0]);

	if (EmuConfig.Cpu.FPUFPCR.bitmask != EmuConfig.Cpu.FPUDivFPCR.bitmask) {
		fpuDoubleLoadFpcr_emit_oaknut(OAK_CPU(Cpu.FPUFPCR.bitmask));
	}

	_freeXMMreg(sreg); _freeXMMreg(treg);
}

void recDIV_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::DIV_F);
	recDIV_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(DIV_S, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MADD/MSUB XMM
//------------------------------------------------------------------

// Unlike what the documentation implies, it seems that MADD/MSUB support all numbers just like other operations
// The complex overflow conditions the document describes apparently test whether the multiplication's result
// has overflowed and whether the last operation that used ACC as a destination has overflowed.
// For example,   { adda.s -MAX, 0.0 ; madd.s fd, MAX, 1.0 } -> fd = 0
// while          { adda.s -MAX, -MAX ; madd.s fd, MAX, 1.0 } -> fd = -MAX
// (where MAX is 0x7fffffff and -MAX is 0xffffffff)
static void recMADD_S_emit_oaknut(int info)
{
	int sreg, treg;
	ALLOC_S(sreg); ALLOC_T(treg);

	FPU_MUL(info, sreg, sreg, treg, false);

	GET_ACC(treg);

	if (FPU_CORRECT_ADD_SUB)
		FPU_ADD_SUB(treg, sreg); //might be problematic for something!!!!

	//          TEST FOR ACC/MUL OVERFLOWS, PROPOGATE THEM IF THEY OCCUR


	const oak::QReg regS = oakQRegister(sreg);
	const oak::QReg regT = oakQRegister(treg);


	oakLoad32(OAK_WSCRATCH, OAK_CPU(fpuRegs.fprc[31]));
	oakAsm->TST(OAK_WSCRATCH, FPUflagO);
//	u8* mulovf = JNZ8(0);
	oak::Label mulovf;
	oakAsm->B(oak::util::NE, mulovf);
	ToDouble(sreg); //else, convert

	oakLoad32(OAK_WSCRATCH, OAK_CPU(fpuRegs.ACCflag));
	oakAsm->TST(OAK_WSCRATCH, 1);
//	u8* accovf = JNZ8(0);
	oak::Label accovf;
	oakAsm->B(oak::util::NE, accovf);
	ToDouble(treg); //else, convert
//	u8* operation = JMP8(0);
	oak::Label operation;
	oakAsm->B(operation);

//	x86SetJ8(mulovf);
	oakAsm->l(mulovf);
	oakAsm->MOV(regT.B16(), regS.B16());

//	x86SetJ8(accovf);
	oakAsm->l(accovf);
	SetMaxValue(treg); //just in case... I think it has to be a MaxValue already here
	CLEAR_OU_FLAGS; //clear U flag
	if (FPU_FLAGS_OVERFLOW) {
		fpuDoubleSetFcr31Flags_emit_oaknut(FPUflagO | FPUflagSO);
	}
//	u32* skipall = JMP32(0);
	oak::Label skipall;
	oakAsm->B(skipall);

	//			PERFORM THE ACCUMULATION AND TEST RESULT. CONVERT TO SINGLE

//	x86SetJ8(operation);
	oakAsm->l(operation);
	oakAsm->FADD(oakDRegister(treg), oakDRegister(treg), oakDRegister(sreg));

	ToPS2FPU(treg, true, sreg, false, true);
//	x86SetJ32(skipall);
	oakAsm->l(skipall);

	oakAsm->MOV(oakQRegister(EEREC_D).Selem()[0], regT.Selem()[0]);

	_freeXMMreg(sreg); _freeXMMreg(treg);
}

static void recMADDA_S_emit_oaknut(int info)
{
	int sreg, treg;
	ALLOC_S(sreg); ALLOC_T(treg);

	FPU_MUL(info, sreg, sreg, treg, false);

	GET_ACC(treg);

	if (FPU_CORRECT_ADD_SUB)
		FPU_ADD_SUB(treg, sreg); //might be problematic for something!!!!

	const oak::QReg regS = oakQRegister(sreg);
	const oak::QReg regT = oakQRegister(treg);

	oakLoad32(OAK_WSCRATCH, OAK_CPU(fpuRegs.fprc[31]));
	oakAsm->TST(OAK_WSCRATCH, FPUflagO);
	oak::Label mulovf;
	oakAsm->B(oak::util::NE, mulovf);
	ToDouble(sreg);

	oakLoad32(OAK_WSCRATCH, OAK_CPU(fpuRegs.ACCflag));
	oakAsm->TST(OAK_WSCRATCH, 1);
	oak::Label accovf;
	oakAsm->B(oak::util::NE, accovf);
	ToDouble(treg);
	oak::Label operation;
	oakAsm->B(operation);

	oakAsm->l(mulovf);
	oakAsm->MOV(regT.B16(), regS.B16());

	oakAsm->l(accovf);
	SetMaxValue(treg);
	CLEAR_OU_FLAGS;
	if (FPU_FLAGS_OVERFLOW) {
		fpuDoubleSetFcr31Flags_emit_oaknut(FPUflagO | FPUflagSO);
		fpuDoubleSetAccFlag_emit_oaknut();
	}
	oak::Label skipall;
	oakAsm->B(skipall);

	oakAsm->l(operation);
	oakAsm->FADD(oakDRegister(treg), oakDRegister(treg), oakDRegister(sreg));

	ToPS2FPU(treg, true, sreg, true, true);
	oakAsm->l(skipall);

	oakAsm->MOV(oakQRegister(EEREC_ACC).Selem()[0], regT.Selem()[0]);

	_freeXMMreg(sreg); _freeXMMreg(treg);
}

static void recMSUB_S_emit_oaknut(int info)
{
	int sreg, treg;
	ALLOC_S(sreg); ALLOC_T(treg);

	FPU_MUL(info, sreg, sreg, treg, false);

	GET_ACC(treg);

	if (FPU_CORRECT_ADD_SUB)
		FPU_ADD_SUB(treg, sreg); //might be problematic for something!!!!

	const oak::QReg regS = oakQRegister(sreg);
	const oak::QReg regT = oakQRegister(treg);

	oakLoad32(OAK_WSCRATCH, OAK_CPU(fpuRegs.fprc[31]));
	oakAsm->TST(OAK_WSCRATCH, FPUflagO);
	oak::Label mulovf;
	oakAsm->B(oak::util::NE, mulovf);
	ToDouble(sreg);

	oakLoad32(OAK_WSCRATCH, OAK_CPU(fpuRegs.ACCflag));
	oakAsm->TST(OAK_WSCRATCH, 1);
	oak::Label accovf;
	oakAsm->B(oak::util::NE, accovf);
	ToDouble(treg);
	oak::Label operation;
	oakAsm->B(operation);

	oakAsm->l(mulovf);
	oakAsm->EOR(regS.B16(), regS.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.neg), regS).B16());
	oakAsm->MOV(regT.B16(), regS.B16());

	oakAsm->l(accovf);
	SetMaxValue(treg);
	CLEAR_OU_FLAGS;
	if (FPU_FLAGS_OVERFLOW) {
		fpuDoubleSetFcr31Flags_emit_oaknut(FPUflagO | FPUflagSO);
	}
	oak::Label skipall;
	oakAsm->B(skipall);

	oakAsm->l(operation);
	oakAsm->FSUB(oakDRegister(treg), oakDRegister(treg), oakDRegister(sreg));

	ToPS2FPU(treg, true, sreg, false, true);
	oakAsm->l(skipall);

	oakAsm->MOV(oakQRegister(EEREC_D).Selem()[0], regT.Selem()[0]);

	_freeXMMreg(sreg); _freeXMMreg(treg);
}

static void recMSUBA_S_emit_oaknut(int info)
{
	int sreg, treg;
	ALLOC_S(sreg); ALLOC_T(treg);

	FPU_MUL(info, sreg, sreg, treg, false);

	GET_ACC(treg);

	if (FPU_CORRECT_ADD_SUB)
		FPU_ADD_SUB(treg, sreg); //might be problematic for something!!!!

	const oak::QReg regS = oakQRegister(sreg);
	const oak::QReg regT = oakQRegister(treg);

	oakLoad32(OAK_WSCRATCH, OAK_CPU(fpuRegs.fprc[31]));
	oakAsm->TST(OAK_WSCRATCH, FPUflagO);
	oak::Label mulovf;
	oakAsm->B(oak::util::NE, mulovf);
	ToDouble(sreg);

	oakLoad32(OAK_WSCRATCH, OAK_CPU(fpuRegs.ACCflag));
	oakAsm->TST(OAK_WSCRATCH, 1);
	oak::Label accovf;
	oakAsm->B(oak::util::NE, accovf);
	ToDouble(treg);
	oak::Label operation;
	oakAsm->B(operation);

	oakAsm->l(mulovf);
	oakAsm->EOR(regS.B16(), regS.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.neg), regS).B16());
	oakAsm->MOV(regT.B16(), regS.B16());

	oakAsm->l(accovf);
	SetMaxValue(treg);
	CLEAR_OU_FLAGS;
	if (FPU_FLAGS_OVERFLOW) {
		fpuDoubleSetFcr31Flags_emit_oaknut(FPUflagO | FPUflagSO);
		fpuDoubleSetAccFlag_emit_oaknut();
	}
	oak::Label skipall;
	oakAsm->B(skipall);

	oakAsm->l(operation);
	oakAsm->FSUB(oakDRegister(treg), oakDRegister(treg), oakDRegister(sreg));

	ToPS2FPU(treg, true, sreg, true, true);
	oakAsm->l(skipall);

	oakAsm->MOV(oakQRegister(EEREC_ACC).Selem()[0], regT.Selem()[0]);

	_freeXMMreg(sreg); _freeXMMreg(treg);
}

void recMADD_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MADD_F);
	recMADD_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(MADD_S, XMMINFO_WRITED | XMMINFO_READACC | XMMINFO_READS | XMMINFO_READT);

void recMADDA_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MADDA_F);
	recMADDA_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(MADDA_S, XMMINFO_WRITEACC | XMMINFO_READACC | XMMINFO_READS | XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MAX / MIN XMM
//------------------------------------------------------------------

//alignas(16) static const u32 minmax_mask[8] =
//{
//	0xffffffff, 0x80000000, 0, 0,
//	0,          0x40000000, 0, 0,
//};
// FPU's MAX/MIN work with all numbers (including "denormals"). Check VU's logical min max for more info.
static void recMAX_S_emit_oaknut(int info)
{
	int sreg, treg;
	ALLOC_S(sreg); ALLOC_T(treg);

	CLEAR_OU_FLAGS;

	const oak::QReg regS = oakQRegister(sreg);
	const oak::QReg regT = oakQRegister(treg);

	fpuDoublePshufd_emit_oaknut(regS, regS, 0x00);
	oakAsm->AND(regS.B16(), regS.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.minmax_mask), regS).B16());
	oakAsm->ORR(regS.B16(), regS.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.minmax_mask[4]), regS).B16());
	fpuDoublePshufd_emit_oaknut(regT, regT, 0x00);
	oakAsm->AND(regT.B16(), regT.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.minmax_mask), regT).B16());
	oakAsm->ORR(regT.B16(), regT.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.minmax_mask[4]), regT).B16());
	oakAsm->FMAXNM(oakDRegister(sreg), oakDRegister(sreg), oakDRegister(treg));

	oakAsm->MOV(oakQRegister(EEREC_D).Selem()[0], regS.Selem()[0]);

	_freeXMMreg(sreg); _freeXMMreg(treg);
}

void recMAX_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MAX_F);
	recMAX_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(MAX_S, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

static void recMIN_S_emit_oaknut(int info)
{
	int sreg, treg;
	ALLOC_S(sreg); ALLOC_T(treg);

	CLEAR_OU_FLAGS;

	const oak::QReg regS = oakQRegister(sreg);
	const oak::QReg regT = oakQRegister(treg);

	fpuDoublePshufd_emit_oaknut(regS, regS, 0x00);
	oakAsm->AND(regS.B16(), regS.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.minmax_mask), regS).B16());
	oakAsm->ORR(regS.B16(), regS.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.minmax_mask[4]), regS).B16());
	fpuDoublePshufd_emit_oaknut(regT, regT, 0x00);
	oakAsm->AND(regT.B16(), regT.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.minmax_mask), regT).B16());
	oakAsm->ORR(regT.B16(), regT.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.minmax_mask[4]), regT).B16());
	oakAsm->FMINNM(oakDRegister(sreg), oakDRegister(sreg), oakDRegister(treg));

	oakAsm->MOV(oakQRegister(EEREC_D).Selem()[0], regS.Selem()[0]);

	_freeXMMreg(sreg); _freeXMMreg(treg);
}

void recMIN_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MIN_F);
	recMIN_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(MIN_S, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MOV XMM
//------------------------------------------------------------------
void recMOV_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MOV_F);
	GET_S(EEREC_D);
}

FPURECOMPILE_CONSTCODE(MOV_S, XMMINFO_WRITED | XMMINFO_READS);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MSUB XMM
//------------------------------------------------------------------

void recMSUB_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MSUB_F);
	recMSUB_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(MSUB_S, XMMINFO_WRITED | XMMINFO_READACC | XMMINFO_READS | XMMINFO_READT);

void recMSUBA_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MSUBA_F);
	recMSUBA_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(MSUBA_S, XMMINFO_WRITEACC | XMMINFO_READACC | XMMINFO_READS | XMMINFO_READT);
//------------------------------------------------------------------

//------------------------------------------------------------------
// MUL XMM
//------------------------------------------------------------------
void recMUL_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MUL_F);
	int sreg, treg;
	ALLOC_S(sreg); ALLOC_T(treg);

	FPU_MUL(info, EEREC_D, sreg, treg, false);
	_freeXMMreg(sreg); _freeXMMreg(treg);
}

FPURECOMPILE_CONSTCODE(MUL_S, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);

void recMULA_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::MULA_F);
	int sreg, treg;
	ALLOC_S(sreg); ALLOC_T(treg);

	FPU_MUL(info, EEREC_ACC, sreg, treg, true);
	_freeXMMreg(sreg); _freeXMMreg(treg);
}

FPURECOMPILE_CONSTCODE(MULA_S, XMMINFO_WRITEACC | XMMINFO_READS | XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// NEG XMM
//------------------------------------------------------------------
void recNEG_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::NEG_F);
	GET_S(EEREC_D);

	CLEAR_OU_FLAGS;

	oakAsm->EOR(oakQRegister(EEREC_D).B16(), oakQRegister(EEREC_D).B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.neg[0]), oakQRegister(EEREC_D)).B16());
}

FPURECOMPILE_CONSTCODE(NEG_S, XMMINFO_WRITED | XMMINFO_READS);
//------------------------------------------------------------------


//------------------------------------------------------------------
// SUB XMM
//------------------------------------------------------------------
static void recSUB_S_emit_oaknut(int info)
{
	int sreg, treg;
	const bool d_aliases_s = (_Fd_ == _Fs_);
	const bool d_aliases_t = (_Fd_ == _Ft_);

	if (d_aliases_s)
		fpuDoubleFlushCachedFprToMemory_emit_oaknut(_Fs_);
	if (d_aliases_t && _Ft_ != _Fs_)
		fpuDoubleFlushCachedFprToMemory_emit_oaknut(_Ft_);

	sreg = _allocTempXMMreg(XMMT_FPS);
	if (d_aliases_s)
		fpuDoubleLoadScalarFromFpr_emit_oaknut(sreg, _Fs_);
	else
		GET_S(sreg);

	treg = _allocTempXMMreg(XMMT_FPS);
	if (d_aliases_t)
		fpuDoubleLoadScalarFromFpr_emit_oaknut(treg, _Ft_);
	else
		GET_T(treg);

	if (FPU_CORRECT_ADD_SUB)
		FPU_ADD_SUB(sreg, treg);

	ToDouble(sreg);
	ToDouble(treg);
	oakAsm->FSUB(oakDRegister(sreg), oakDRegister(sreg), oakDRegister(treg));

	ToPS2FPU(sreg, true, treg, false, true);
	oakAsm->MOV(oakQRegister(EEREC_D).Selem()[0], oakQRegister(sreg).Selem()[0]);

	_freeXMMreg(sreg);
	_freeXMMreg(treg);
}

void recSUB_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::SUB_F);
	recSUB_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(SUB_S, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);


static void recSUBA_S_emit_oaknut(int info)
{
	int sreg, treg;
	ALLOC_S(sreg);
	ALLOC_T(treg);

	if (FPU_CORRECT_ADD_SUB)
		FPU_ADD_SUB(sreg, treg);

	ToDouble(sreg);
	ToDouble(treg);
	oakAsm->FSUB(oakDRegister(sreg), oakDRegister(sreg), oakDRegister(treg));

	ToPS2FPU(sreg, true, treg, true, true);
	oakAsm->MOV(oakQRegister(EEREC_ACC).Selem()[0], oakQRegister(sreg).Selem()[0]);

	_freeXMMreg(sreg);
	_freeXMMreg(treg);
}

void recSUBA_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::SUBA_F);
	recSUBA_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(SUBA_S, XMMINFO_WRITEACC | XMMINFO_READS | XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// SQRT XMM
//------------------------------------------------------------------
void recSQRT_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::SQRT_F);
	int roundmodeFlag = 0;
	const int t1reg = _allocTempXMMreg(XMMT_FPS);

	if (EmuConfig.Cpu.FPUFPCR.GetRoundMode() != FPRoundMode::Nearest)
	{
		// Set roundmode to nearest if it isn't already
		roundmode_nearest = EmuConfig.Cpu.FPUFPCR;
		roundmode_nearest.SetRoundMode(FPRoundMode::Nearest);
		fpuDoubleLoadFpcr_emit_oaknut(OAK_CPU(Cpu.FPUFPCR.bitmask));
		roundmodeFlag = 1;
	}

	GET_T(EEREC_D);

	const oak::QReg regED = oakQRegister(EEREC_D);

	if (FPU_FLAGS_ID)
	{
		fpuDoubleClearFcr31Flags_emit_oaknut(FPUflagI | FPUflagD);

		//--- Check for negative SQRT --- (sqrt(-0) = 0, unlike what the docs say)
		fpuDoubleMovmskps_emit_oaknut(OAK_EAX, regED);
		oakAsm->AND(OAK_EAX, OAK_EAX, 1);
//		u8* pjmp = JZ8(0); //Skip if none are
		oak::Label pjmp;
		oakAsm->CBZ(OAK_EAX, pjmp);
			fpuDoubleSetFcr31Flags_emit_oaknut(FPUflagI | FPUflagSI);
			oakAsm->AND(regED.B16(), regED.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.pos[0]), regED).B16());
//		x86SetJ8(pjmp);
		oakAsm->l(pjmp);
	}
	else
	{
		oakAsm->AND(regED.B16(), regED.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.pos[0]), regED).B16());
	}


	ToDouble(EEREC_D);

	oakAsm->FSQRT(oakDRegister(EEREC_D), oakDRegister(EEREC_D));

	ToPS2FPU(EEREC_D, false, t1reg, false);

	if (roundmodeFlag == 1) {
		fpuDoubleLoadFpcr_emit_oaknut(OAK_CPU(Cpu.FPUFPCR.bitmask));
	}

	_freeXMMreg(t1reg);
}

FPURECOMPILE_CONSTCODE(SQRT_S, XMMINFO_WRITED | XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// RSQRT XMM
//------------------------------------------------------------------
static void recRSQRT_S_flags_emit_oaknut(int regd, int regt)
{
//	u8 *pjmp1, *pjmp2;
//	u8 *qjmp1, *qjmp2;
//	u32* pjmp32;
	oak::Label pjmp1, pjmp2;
	oak::Label qjmp1, qjmp2;
	oak::Label pjmp32;

	const oak::QReg regT = oakQRegister(regt);

	int t1reg = _allocTempXMMreg(XMMT_FPS);
	const oak::QReg regT1 = oakQRegister(t1reg);

	fpuDoubleClearFcr31Flags_emit_oaknut(FPUflagI | FPUflagD);

	//--- (first) Check for negative SQRT ---
	fpuDoubleMovmskps_emit_oaknut(OAK_EAX, regT);
	oakAsm->AND(OAK_EAX, OAK_EAX, 1);
//	pjmp2 = JZ8(0); //Skip if not set
	oakAsm->CBZ(OAK_EAX, pjmp2);
		fpuDoubleSetFcr31Flags_emit_oaknut(FPUflagI | FPUflagSI);
		oakAsm->AND(regT.B16(), regT.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.pos[0]), regT).B16());
//	x86SetJ8(pjmp2);
	oakAsm->l(pjmp2);

	//--- Check for zero ---
	oakAsm->EOR(regT1.B16(), regT1.B16(), regT1.B16());
	oakAsm->FCMEQ(oakSRegister(t1reg), oakSRegister(t1reg), oakSRegister(regt));
	fpuDoubleMovmskps_emit_oaknut(OAK_EAX, regT1);
	oakAsm->AND(OAK_EAX, OAK_EAX, 1);
//	pjmp1 = JZ8(0); //Skip if not set
	oakAsm->CBZ(OAK_EAX, pjmp1);

		//--- Check for 0/0 ---
		oakAsm->EOR(regT1.B16(), regT1.B16(), regT1.B16());
		oakAsm->FCMEQ(oakSRegister(t1reg), oakSRegister(t1reg), oakSRegister(regd));
		fpuDoubleMovmskps_emit_oaknut(OAK_EAX, regT1);
		oakAsm->AND(OAK_EAX, OAK_EAX, 1);
//		qjmp1 = JZ8(0); //Skip if not set
		oakAsm->CBZ(OAK_EAX, qjmp1);
			fpuDoubleSetFcr31Flags_emit_oaknut(FPUflagI | FPUflagSI);
//			qjmp2 = JMP8(0);
			oakAsm->B(qjmp2);
//		x86SetJ8(qjmp1); //x/0 but not 0/0
		oakAsm->l(qjmp1);
			fpuDoubleSetFcr31Flags_emit_oaknut(FPUflagD | FPUflagSD);
//		x86SetJ8(qjmp2);
		oakAsm->l(qjmp2);

		SetMaxValue(regd); //clamp to max
//		pjmp32 = JMP32(0);
		oakAsm->B(pjmp32);
//	x86SetJ8(pjmp1);
	oakAsm->l(pjmp1);

	ToDouble(regt); ToDouble(regd);

	oakAsm->FSQRT(oakDRegister(regt), oakDRegister(regt));
	oakAsm->FDIV(oakDRegister(regd), oakDRegister(regd), oakDRegister(regt));

	ToPS2FPU(regd, false, regt, false);
//	x86SetJ32(pjmp32);
	oakAsm->l(pjmp32);

	_freeXMMreg(t1reg);
}

static void recRSQRT_S_no_flags_emit_oaknut(int regd, int regt)
{
	const oak::QReg regT = oakQRegister(regt);

	oakAsm->AND(regT.B16(), regT.B16(), fpuDoubleLoadConstQ_emit_oaknut(OAK_CPU(mVUss4.s_const.pos[0]), regT).B16());

	ToDouble(regt); ToDouble(regd);

	oakAsm->FSQRT(oakDRegister(regt), oakDRegister(regt));
	oakAsm->FDIV(oakDRegister(regd), oakDRegister(regd), oakDRegister(regt));

	ToPS2FPU(regd, false, regt, false);
}

static void recRSQRT_S_emit_oaknut(int info)
{
	int sreg, treg;

	// iFPU (regular FPU) doesn't touch roundmode for rSQRT.
	// Should this do the same?  or is changing the roundmode to nearest the better
	// behavior for both recs? --air

	bool roundmodeFlag = false;
	if (EmuConfig.Cpu.FPUFPCR.GetRoundMode() != FPRoundMode::Nearest)
	{
		// Set roundmode to nearest if it isn't already
		roundmode_nearest = EmuConfig.Cpu.FPUFPCR;
		roundmode_nearest.SetRoundMode(FPRoundMode::Nearest);
		fpuDoubleLoadFpcr_emit_oaknut(OAK_CPU(Cpu.FPUFPCR.bitmask));
		roundmodeFlag = true;
	}

	ALLOC_S(sreg); ALLOC_T(treg);

	if (FPU_FLAGS_ID)
		recRSQRT_S_flags_emit_oaknut(sreg, treg);
	else
		recRSQRT_S_no_flags_emit_oaknut(sreg, treg);

	oakAsm->MOV(oakQRegister(EEREC_D).Selem()[0], oakQRegister(sreg).Selem()[0]);

	_freeXMMreg(treg); _freeXMMreg(sreg);

	if (roundmodeFlag) {
		fpuDoubleLoadFpcr_emit_oaknut(OAK_CPU(Cpu.FPUFPCR.bitmask));
	}
}

void recRSQRT_S_xmm(int info)
{
	EE::Profiler.EmitOp(eeOpcode::RSQRT_F);
	recRSQRT_S_emit_oaknut(info);
}

FPURECOMPILE_CONSTCODE(RSQRT_S, XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);


} // namespace DOUBLE
} // namespace COP1
} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
#endif
