// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

//------------------------------------------------------------------
// mVUupdateFlags() - Updates status/mac flags
//------------------------------------------------------------------

#define AND_XYZW ((_XYZW_SS && modXYZW) ? (1) : (mFLAG.doFlag ? (_X_Y_Z_W) : (flipMask[_X_Y_Z_W])))
#define ADD_XYZW ((_XYZW_SS && modXYZW) ? (_X ? 3 : (_Y ? 2 : (_Z ? 1 : 0))) : 0)


//alignas(16) const u32 sse4_compvals[2][4] = {
//	{0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff}, //1111
//	{0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff}, //1111
//};

const std::array<u16, 16> flipMask{0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

static __fi OakMemOperand mVUUpperOakGlobMem(s64 offset)
{
	return mVUAllocOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, mVUglob)) + offset);
}

static __fi OakMemOperand mVUUpperOakSs4Mem(s64 offset)
{
	return mVUAllocOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, mVUss4)) + offset);
}

static __fi void mVUUpperPshufd_oaknut(int dst, int src, u8 imm)
{
	const oak::QReg dst_q = oakQRegister(dst);
	const oak::QReg src_q = oakQRegister(src);

	if (imm == 0x00) { oakAsm->DUP(dst_q.S4(), src_q.Selem()[0]); return; }
	if (imm == 0x55) { oakAsm->DUP(dst_q.S4(), src_q.Selem()[1]); return; }
	if (imm == 0xaa) { oakAsm->DUP(dst_q.S4(), src_q.Selem()[2]); return; }
	if (imm == 0xff) { oakAsm->DUP(dst_q.S4(), src_q.Selem()[3]); return; }
	if (imm == 0x1b)
	{
		oakAsm->REV64(OAK_QSCRATCH3.S4(), src_q.S4());
		oakAsm->EXT(dst_q.B16(), OAK_QSCRATCH3.B16(), OAK_QSCRATCH3.B16(), 8);
		return;
	}

	oakLoad128(OAK_QSCRATCH3,
		mVUAllocOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, shuffle.data[imm][0]))));
	oakAsm->MOV(OAK_QSCRATCH2.B16(), src_q.B16());
	oakAsm->TBL(dst_q.B16(), oak::List(OAK_QSCRATCH2.B16()), OAK_QSCRATCH3.B16());
}

static __fi void mVUUpperMovmskps_oaknut(const oak::WReg& dst, const oak::QReg& src)
{
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakSs4Mem(offsetof(mVU_SSE4, mac_mask)));
	oakAsm->SSHR(OAK_QSCRATCH2.S4(), src.S4(), 31);
	oakAsm->AND(OAK_QSCRATCH2.B16(), OAK_QSCRATCH2.B16(), OAK_QSCRATCH3.B16());
	oakAsm->ADDV(OAK_SSCRATCH, OAK_QSCRATCH2.S4());
	oakAsm->FMOV(dst, OAK_SSCRATCH);
}

static __fi void mVUUpperClamp1Vector_oaknut(mV, int reg, bool bClampE)
{
	if (((!clampE && CHECK_VU_OVERFLOW(mVU.index)) || (clampE && bClampE)) && mVU.regAlloc->checkVFClamp(reg))
		mVUClamp1VectorBits_oaknut(reg);
}

static __fi void mVUUpperClamp2Vector_oaknut(mV, int reg, bool bClampE)
{
	if (((!clampE && CHECK_VU_SIGN_OVERFLOW(mVU.index)) || (clampE && bClampE && CHECK_VU_SIGN_OVERFLOW(mVU.index))) &&
		mVU.regAlloc->checkVFClamp(reg))
	{
		const oak::QReg reg_q = oakQRegister(reg);
		oakLoad128(OAK_QSCRATCH3, mVUUpperOakSs4Mem(offsetof(mVU_SSE4, sse4_maxvals[0][0])));
		oakAsm->SMIN(reg_q.S4(), reg_q.S4(), OAK_QSCRATCH3.S4());
		oakLoad128(OAK_QSCRATCH3, mVUUpperOakSs4Mem(offsetof(mVU_SSE4, sse4_minvals[0][0])));
		oakAsm->UMIN(reg_q.S4(), reg_q.S4(), OAK_QSCRATCH3.S4());
		return;
	}

	mVUUpperClamp1Vector_oaknut(mVU, reg, bClampE);
}

static __fi void mVUUpperClamp3Vector_oaknut(mV, int reg)
{
	if (clampE && mVU.regAlloc->checkVFClamp(reg))
		mVUUpperClamp2Vector_oaknut(mVU, reg, true);
	else if (isVU0 && mVU.regAlloc->checkVFClamp(reg))
		mVUClampDenormalVectorBits_oaknut(reg);
}

static __fi void mVUUpperClamp4Vector_oaknut(mV, int reg)
{
	if (clampE && !CHECK_VU_SIGN_OVERFLOW(mVU.index) && mVU.regAlloc->checkVFClamp(reg))
		mVUUpperClamp1Vector_oaknut(mVU, reg, true);
	else if (isVU0 && mVU.regAlloc->checkVFClamp(reg))
		mVUClampDenormalVectorBits_oaknut(reg);
}

static __fi void mVUUpperClamp1Scalar_oaknut(mV, int reg, bool bClampE)
{
	if (((!clampE && CHECK_VU_OVERFLOW(mVU.index)) || (clampE && bClampE)) && mVU.regAlloc->checkVFClamp(reg))
		mVUClamp1ScalarBits_oaknut(reg);
}

static __fi void mVUUpperClamp2Scalar_oaknut(mV, int reg, bool bClampE)
{
	if (((!clampE && CHECK_VU_SIGN_OVERFLOW(mVU.index)) || (clampE && bClampE && CHECK_VU_SIGN_OVERFLOW(mVU.index))) &&
		mVU.regAlloc->checkVFClamp(reg))
	{
		const oak::QReg reg_q = oakQRegister(reg);
		oakLoad128(OAK_QSCRATCH3, mVUUpperOakSs4Mem(offsetof(mVU_SSE4, sse4_maxvals[0][0])));
		oakAsm->SMIN(reg_q.S4(), reg_q.S4(), OAK_QSCRATCH3.S4());
		oakLoad128(OAK_QSCRATCH3, mVUUpperOakSs4Mem(offsetof(mVU_SSE4, sse4_minvals[0][0])));
		oakAsm->UMIN(reg_q.S4(), reg_q.S4(), OAK_QSCRATCH3.S4());
		return;
	}

	mVUUpperClamp1Scalar_oaknut(mVU, reg, bClampE);
}

static __fi void mVUUpperClamp3Scalar_oaknut(mV, int reg)
{
	if (clampE && mVU.regAlloc->checkVFClamp(reg))
		mVUUpperClamp2Scalar_oaknut(mVU, reg, true);
	else if (isVU0 && mVU.regAlloc->checkVFClamp(reg))
		mVUClampDenormalScalarBits_oaknut(reg);
}

static __fi void mVUUpperClamp4Scalar_oaknut(mV, int reg)
{
	if (clampE && !CHECK_VU_SIGN_OVERFLOW(mVU.index) && mVU.regAlloc->checkVFClamp(reg))
		mVUUpperClamp1Scalar_oaknut(mVU, reg, true);
	else if (isVU0 && mVU.regAlloc->checkVFClamp(reg))
		mVUClampDenormalScalarBits_oaknut(reg);
}

static __fi void mVUUpperAddSs_oaknut(mV, int to, int from)
{
	mVUUpperClamp3Scalar_oaknut(mVU, to);
	mVUUpperClamp3Scalar_oaknut(mVU, from);
	oakAsm->FADD(oakSRegister(to), oakSRegister(to), oakSRegister(from));
	mVUUpperClamp4Scalar_oaknut(mVU, to);
}

static __fi void mVUUpperAddPs_oaknut(mV, int to, int from)
{
	const oak::QReg to_q = oakQRegister(to);
	const oak::QReg from_q = oakQRegister(from);
	mVUUpperClamp3Vector_oaknut(mVU, to);
	mVUUpperClamp3Vector_oaknut(mVU, from);
	oakAsm->FADD(to_q.S4(), to_q.S4(), from_q.S4());
	mVUUpperClamp4Vector_oaknut(mVU, to);
}

static __fi void mVUUpperSubSs_oaknut(mV, int to, int from)
{
	mVUUpperClamp3Scalar_oaknut(mVU, to);
	mVUUpperClamp3Scalar_oaknut(mVU, from);
	oakAsm->FSUB(oakSRegister(to), oakSRegister(to), oakSRegister(from));
	mVUUpperClamp4Scalar_oaknut(mVU, to);
}

static __fi void mVUUpperMulPs_oaknut(mV, int to, int from)
{
	const oak::QReg to_q = oakQRegister(to);
	const oak::QReg from_q = oakQRegister(from);
	mVUUpperClamp3Vector_oaknut(mVU, to);
	mVUUpperClamp3Vector_oaknut(mVU, from);
	oakAsm->FMUL(to_q.S4(), to_q.S4(), from_q.S4());
	mVUUpperClamp4Vector_oaknut(mVU, to);
}

static __fi void mVUUpperSubPs_oaknut(mV, int to, int from)
{
	const oak::QReg to_q = oakQRegister(to);
	const oak::QReg from_q = oakQRegister(from);
	mVUUpperClamp3Vector_oaknut(mVU, to);
	mVUUpperClamp3Vector_oaknut(mVU, from);
	oakAsm->FSUB(to_q.S4(), to_q.S4(), from_q.S4());
	mVUUpperClamp4Vector_oaknut(mVU, to);
}

static __fi void mVUUpperMulSs_oaknut(mV, int to, int from)
{
	mVUUpperClamp3Scalar_oaknut(mVU, to);
	mVUUpperClamp3Scalar_oaknut(mVU, from);
	oakAsm->FMUL(oakSRegister(to), oakSRegister(to), oakSRegister(from));
	mVUUpperClamp4Scalar_oaknut(mVU, to);
}

static __fi void mVUUpperFmlaPs_oaknut(mV, int acc, int left, int right)
{
	const oak::QReg acc_q = oakQRegister(acc);
	const oak::QReg left_q = oakQRegister(left);
	const oak::QReg right_q = oakQRegister(right);
	mVUUpperClamp3Vector_oaknut(mVU, acc);
	mVUUpperClamp3Vector_oaknut(mVU, left);
	mVUUpperClamp3Vector_oaknut(mVU, right);
	oakAsm->FMUL(OAK_QSCRATCH.S4(), left_q.S4(), right_q.S4());
	oakAsm->FADD(acc_q.S4(), acc_q.S4(), OAK_QSCRATCH.S4());
	mVUUpperClamp4Vector_oaknut(mVU, acc);
}

static __fi void mVUUpperFmlaSs_oaknut(mV, int acc, int left, int right)
{
	mVUUpperClamp3Scalar_oaknut(mVU, acc);
	mVUUpperClamp3Scalar_oaknut(mVU, left);
	mVUUpperClamp3Scalar_oaknut(mVU, right);
	oakAsm->FMUL(OAK_SSCRATCH, oakSRegister(left), oakQRegister(right).Selem()[0]);
	oakAsm->FADD(oakSRegister(acc), oakSRegister(acc), OAK_SSCRATCH);
	mVUUpperClamp4Scalar_oaknut(mVU, acc);
}

static __fi void mVUUpperFmlaPsLane_oaknut(int acc, int left, int right, int lane)
{
	oakAsm->FMUL(OAK_QSCRATCH.S4(), oakQRegister(left).S4(), oakQRegister(right).Selem()[lane]);
	oakAsm->FADD(oakQRegister(acc).S4(), oakQRegister(acc).S4(), OAK_QSCRATCH.S4());
}

static __fi void mVUUpperFmlaSsLane_oaknut(int acc, int left, int right, int lane)
{
	oakAsm->FMUL(OAK_SSCRATCH, oakSRegister(left), oakQRegister(right).Selem()[lane]);
	oakAsm->FADD(oakSRegister(acc), oakSRegister(acc), OAK_SSCRATCH);
}

static __fi void mVUUpperVuDoubleVector_oaknut(int reg, int t1, int t2)
{
	const oak::QReg reg_q = oakQRegister(reg);
	const oak::QReg t1_q = oakQRegister(t1);
	const oak::QReg t2_q = oakQRegister(t2);

	oakLoad128(t1_q, mVUUpperOakGlobMem(offsetof(mVU_Globals, exponent)));
	oakAsm->AND(t1_q.B16(), t1_q.B16(), reg_q.B16());

	oakAsm->EOR(t2_q.B16(), t2_q.B16(), t2_q.B16());
	oakAsm->CMEQ(t2_q.S4(), t1_q.S4(), t2_q.S4());
	oakLoad128(OAK_QSCRATCH, mVUUpperOakGlobMem(offsetof(mVU_Globals, signbit)));
	oakAsm->AND(OAK_QSCRATCH.B16(), reg_q.B16(), OAK_QSCRATCH.B16());
	oakAsm->BSL(t2_q.B16(), OAK_QSCRATCH.B16(), reg_q.B16());
	oakAsm->MOV(reg_q.B16(), t2_q.B16());

	if (CHECK_VU_OVERFLOW(0))
	{
		oakLoad128(t2_q, mVUUpperOakGlobMem(offsetof(mVU_Globals, exponent)));
		oakAsm->CMEQ(t1_q.S4(), t1_q.S4(), t2_q.S4());
		oakLoad128(OAK_QSCRATCH, mVUUpperOakGlobMem(offsetof(mVU_Globals, signbit)));
		oakAsm->AND(OAK_QSCRATCH.B16(), reg_q.B16(), OAK_QSCRATCH.B16());
		oakLoad128(OAK_QSCRATCH2, mVUUpperOakGlobMem(offsetof(mVU_Globals, maxvals)));
		oakAsm->ORR(OAK_QSCRATCH.B16(), OAK_QSCRATCH.B16(), OAK_QSCRATCH2.B16());
		oakAsm->BSL(t1_q.B16(), OAK_QSCRATCH.B16(), reg_q.B16());
		oakAsm->MOV(reg_q.B16(), t1_q.B16());
	}
}

static __fi void mVUUpperMulPsLane_oaknut(int to, int from, int lane)
{
	oakAsm->FMUL(oakQRegister(to).S4(), oakQRegister(to).S4(), oakQRegister(from).Selem()[lane]);
}

static __fi void mVUUpperMulSsLane_oaknut(int to, int from, int lane)
{
	oakAsm->FMUL(oakSRegister(to), oakSRegister(to), oakQRegister(from).Selem()[lane]);
}

static __fi void mVUUpperFmlsPs_oaknut(mV, int acc, int left, int right)
{
	const oak::QReg acc_q = oakQRegister(acc);
	const oak::QReg left_q = oakQRegister(left);
	const oak::QReg right_q = oakQRegister(right);
	mVUUpperClamp3Vector_oaknut(mVU, acc);
	mVUUpperClamp3Vector_oaknut(mVU, left);
	mVUUpperClamp3Vector_oaknut(mVU, right);
	oakAsm->FMUL(OAK_QSCRATCH.S4(), left_q.S4(), right_q.S4());
	oakAsm->FSUB(acc_q.S4(), acc_q.S4(), OAK_QSCRATCH.S4());
	mVUUpperClamp4Vector_oaknut(mVU, acc);
}

static __fi void mVUUpperFmlsSs_oaknut(mV, int acc, int left, int right)
{
	mVUUpperClamp3Scalar_oaknut(mVU, acc);
	mVUUpperClamp3Scalar_oaknut(mVU, left);
	mVUUpperClamp3Scalar_oaknut(mVU, right);
	oakAsm->FMUL(OAK_SSCRATCH, oakSRegister(left), oakQRegister(right).Selem()[0]);
	oakAsm->FSUB(oakSRegister(acc), oakSRegister(acc), OAK_SSCRATCH);
	mVUUpperClamp4Scalar_oaknut(mVU, acc);
}

static __fi void mVUUpperMergeRegs_oaknut(int dest, int src, int xyzw, bool modXYZW = false)
{
	xyzw &= 0xf;
	if (dest == src || xyzw == 0)
		return;

	const oak::QReg dst_q = oakQRegister(dest);
	const oak::QReg src_q = oakQRegister(src);

	if (xyzw == 0xf)
	{
		oakAsm->MOV(dst_q.B16(), src_q.B16());
		return;
	}

	if (modXYZW)
	{
		if (xyzw == 1)
		{
			oakAsm->MOV(dst_q.Selem()[3], src_q.Selem()[0]);
			return;
		}
		if (xyzw == 2)
		{
			oakAsm->MOV(dst_q.Selem()[2], src_q.Selem()[0]);
			return;
		}
		if (xyzw == 4)
		{
			oakAsm->MOV(dst_q.Selem()[1], src_q.Selem()[0]);
			return;
		}
	}

	xyzw = ((xyzw & 1) << 3) | ((xyzw & 2) << 1) | ((xyzw & 4) >> 1) | ((xyzw & 8) >> 3);
	for (u32 i = 0; i < 4; ++i)
	{
		if (xyzw & (1u << i))
			oakAsm->MOV(dst_q.Selem()[i], src_q.Selem()[i]);
	}
}

static __fi void mVUUpperDupLane_oaknut(int dest, int src, int lane)
{
	oakAsm->DUP(oakQRegister(dest).S4(), oakQRegister(src).Selem()[lane]);
}

static __fi void mVUUpperShufPsImm0_oaknut(int dest, int src)
{
	const oak::QReg dest_q = oakQRegister(dest);
	const oak::QReg src_q = oakQRegister(src);
	oakAsm->MOV(OAK_QSCRATCH2.B16(), src_q.B16());
	oakAsm->MOV(dest_q.Selem()[1], dest_q.Selem()[0]);
	oakAsm->MOV(dest_q.Selem()[2], OAK_QSCRATCH2.Selem()[0]);
	oakAsm->MOV(dest_q.Selem()[3], OAK_QSCRATCH2.Selem()[0]);
}

static __fi void mVUUpperMaxScalar_oaknut(int dest, int src, [[maybe_unused]] int temp)
{
	const oak::QReg dest_q = oakQRegister(dest);
	const oak::QReg src_q = oakQRegister(src);
	oakAsm->MOV(OAK_QSCRATCH.B16(), dest_q.B16());
	oakAsm->SSHR(OAK_QSCRATCH.S4(), OAK_QSCRATCH.S4(), 31);
	oakAsm->USHR(OAK_QSCRATCH.S4(), OAK_QSCRATCH.S4(), 1);
	oakAsm->EOR(OAK_QSCRATCH.B16(), OAK_QSCRATCH.B16(), dest_q.B16());

	oakAsm->MOV(OAK_QSCRATCH2.B16(), src_q.B16());
	oakAsm->SSHR(OAK_QSCRATCH2.S4(), OAK_QSCRATCH2.S4(), 31);
	oakAsm->USHR(OAK_QSCRATCH2.S4(), OAK_QSCRATCH2.S4(), 1);
	oakAsm->EOR(OAK_QSCRATCH2.B16(), OAK_QSCRATCH2.B16(), src_q.B16());

	oakAsm->CMGT(OAK_QSCRATCH.S4(), OAK_QSCRATCH.S4(), OAK_QSCRATCH2.S4());
	oakAsm->AND(dest_q.B16(), dest_q.B16(), OAK_QSCRATCH.B16());
	oakAsm->BIC(OAK_QSCRATCH.B16(), src_q.B16(), OAK_QSCRATCH.B16());
	oakAsm->ORR(dest_q.B16(), dest_q.B16(), OAK_QSCRATCH.B16());
}

static __fi void mVUUpperMaxVector_oaknut(int dest, int src, int temp1, int temp2)
{
	const oak::QReg dest_q = oakQRegister(dest);
	const oak::QReg src_q = oakQRegister(src);
	const oak::QReg temp1_q = oakQRegister(temp1);
	const oak::QReg temp2_q = oakQRegister(temp2);
	oakAsm->MOV(temp1_q.B16(), dest_q.B16());
	oakAsm->SSHR(temp1_q.S4(), temp1_q.S4(), 31);
	oakAsm->USHR(temp1_q.S4(), temp1_q.S4(), 1);
	oakAsm->EOR(temp1_q.B16(), temp1_q.B16(), dest_q.B16());

	oakAsm->MOV(temp2_q.B16(), src_q.B16());
	oakAsm->SSHR(temp2_q.S4(), temp2_q.S4(), 31);
	oakAsm->USHR(temp2_q.S4(), temp2_q.S4(), 1);
	oakAsm->EOR(temp2_q.B16(), temp2_q.B16(), src_q.B16());

	oakAsm->CMGT(temp1_q.S4(), temp1_q.S4(), temp2_q.S4());
	oakAsm->AND(dest_q.B16(), dest_q.B16(), temp1_q.B16());
	oakAsm->BIC(temp1_q.B16(), src_q.B16(), temp1_q.B16());
	oakAsm->ORR(dest_q.B16(), dest_q.B16(), temp1_q.B16());
}

static __fi void mVUUpperMiniScalar_oaknut(int dest, int src, [[maybe_unused]] int temp)
{
	const oak::QReg dest_q = oakQRegister(dest);
	const oak::QReg src_q = oakQRegister(src);
	oakAsm->MOV(OAK_QSCRATCH.B16(), dest_q.B16());
	oakAsm->SSHR(OAK_QSCRATCH.S4(), OAK_QSCRATCH.S4(), 31);
	oakAsm->USHR(OAK_QSCRATCH.S4(), OAK_QSCRATCH.S4(), 1);
	oakAsm->EOR(OAK_QSCRATCH.B16(), OAK_QSCRATCH.B16(), dest_q.B16());

	oakAsm->MOV(OAK_QSCRATCH2.B16(), src_q.B16());
	oakAsm->SSHR(OAK_QSCRATCH2.S4(), OAK_QSCRATCH2.S4(), 31);
	oakAsm->USHR(OAK_QSCRATCH2.S4(), OAK_QSCRATCH2.S4(), 1);
	oakAsm->EOR(OAK_QSCRATCH2.B16(), OAK_QSCRATCH2.B16(), src_q.B16());

	oakAsm->CMGT(OAK_QSCRATCH2.S4(), OAK_QSCRATCH2.S4(), OAK_QSCRATCH.S4());
	oakAsm->AND(dest_q.B16(), dest_q.B16(), OAK_QSCRATCH2.B16());
	oakAsm->BIC(OAK_QSCRATCH2.B16(), src_q.B16(), OAK_QSCRATCH2.B16());
	oakAsm->ORR(dest_q.B16(), dest_q.B16(), OAK_QSCRATCH2.B16());
}

static __fi void mVUUpperMiniVector_oaknut(int dest, int src, int temp1, int temp2)
{
	const oak::QReg dest_q = oakQRegister(dest);
	const oak::QReg src_q = oakQRegister(src);
	const oak::QReg temp1_q = oakQRegister(temp1);
	const oak::QReg temp2_q = oakQRegister(temp2);
	oakAsm->MOV(temp1_q.B16(), dest_q.B16());
	oakAsm->SSHR(temp1_q.S4(), temp1_q.S4(), 31);
	oakAsm->USHR(temp1_q.S4(), temp1_q.S4(), 1);
	oakAsm->EOR(temp1_q.B16(), temp1_q.B16(), dest_q.B16());

	oakAsm->MOV(temp2_q.B16(), src_q.B16());
	oakAsm->SSHR(temp2_q.S4(), temp2_q.S4(), 31);
	oakAsm->USHR(temp2_q.S4(), temp2_q.S4(), 1);
	oakAsm->EOR(temp2_q.B16(), temp2_q.B16(), src_q.B16());

	oakAsm->CMGT(temp2_q.S4(), temp2_q.S4(), temp1_q.S4());
	oakAsm->AND(dest_q.B16(), dest_q.B16(), temp2_q.B16());
	oakAsm->BIC(temp2_q.B16(), src_q.B16(), temp2_q.B16());
	oakAsm->ORR(dest_q.B16(), dest_q.B16(), temp2_q.B16());
}

static __fi void mVUUpperGetQreg_oaknut(int dest, int qInstance)
{
	mVUUpperDupLane_oaknut(dest, VU_HOST_XMMPQ, qInstance);
}

static constexpr int VU_HOST_NO_XMM = -1;

enum class mVUExactVu0AccOp
{
	MAdd,
	MSub,
};

enum class mVUExactVu0FtMode
{
	PerLane,
	X,
	Y,
	Z,
	W,
	I,
};

static __fi OakMemOperand mVUExactVu0CpuMem(s64 offset)
{
	return mVUAllocOakCpuMem(offset);
}

static __fi s64 mVUExactVu0AccLaneOffset(int vuIndex, int lane)
{
	return static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[0].ACC.UL)) +
		vuIndex * static_cast<s64>(sizeof(VURegs)) + lane * static_cast<s64>(sizeof(u32));
}

static __fi s64 mVUExactVu0VfLaneOffset(int vuIndex, int vf, int lane)
{
	return static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[0].VF[0].UL)) +
		vuIndex * static_cast<s64>(sizeof(VURegs)) +
		vf * static_cast<s64>(sizeof(VECTOR)) + lane * static_cast<s64>(sizeof(u32));
}

static __fi s64 mVUExactVu0ViOffset(int vuIndex, int vi)
{
	return static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[0].VI[0].UL)) +
		vuIndex * static_cast<s64>(sizeof(VURegs)) + vi * static_cast<s64>(sizeof(REG_VI));
}

static __fi s64 mVUExactVu0MacFlagOffset(int vuIndex)
{
	return static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[0].macflag)) +
		vuIndex * static_cast<s64>(sizeof(VURegs));
}

static __fi s64 mVUExactVu0StatusFlagOffset(int vuIndex)
{
	return static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[0].statusflag)) +
		vuIndex * static_cast<s64>(sizeof(VURegs));
}

static __fi int mVUExactVu0LaneShift(int lane)
{
	return 3 - lane;
}

static __fi int mVUExactVu0FtLane(mVUExactVu0FtMode mode, int lane)
{
	switch (mode)
	{
		case mVUExactVu0FtMode::X:
			return 0;
		case mVUExactVu0FtMode::Y:
			return 1;
		case mVUExactVu0FtMode::Z:
			return 2;
		case mVUExactVu0FtMode::W:
			return 3;
		default:
			return lane;
	}
}

static __fi bool mVUNeedsVu0MicroAccExactPath(const microVU& mVU, mVUExactVu0AccOp op, mVUExactVu0FtMode ftMode)
{
	if (!isVU0 || isCOP2)
		return false;

	switch (ftMode)
	{
		case mVUExactVu0FtMode::I:
			return op == mVUExactVu0AccOp::MAdd;
		case mVUExactVu0FtMode::Y:
		case mVUExactVu0FtMode::Z:
			return true;
		case mVUExactVu0FtMode::PerLane:
			return op == mVUExactVu0AccOp::MSub;
		case mVUExactVu0FtMode::W:
			return true;
		default:
			return false;
	}
}

static __fi void mVUExactVu0AndImm_oaknut(const oak::WReg& dst, const oak::WReg& src, u32 imm)
{
	oakAsm->MOV(oak::util::W2, imm);
	oakAsm->AND(dst, src, oak::util::W2);
}

static __fi void mVUExactVu0OrrImm_oaknut(const oak::WReg& dst, const oak::WReg& src, u32 imm)
{
	oakAsm->MOV(oak::util::W2, imm);
	oakAsm->ORR(dst, src, oak::util::W2);
}

static __fi void mVUExactVu0TstImm_oaknut(const oak::WReg& src, u32 imm)
{
	oakAsm->MOV(oak::util::W2, imm);
	oakAsm->TST(src, oak::util::W2);
}

static __fi void mVUExactVu0VuDoubleScalar_oaknut(const oak::SReg& reg, int vuIndex)
{
	oak::Label done;
	oak::Label check_overflow;

	oakAsm->FMOV(oak::util::W0, reg);
	oakAsm->MOV(oak::util::W1, 0x7f800000);
	oakAsm->AND(oak::util::W1, oak::util::W0, oak::util::W1);
	oakAsm->CBNZ(oak::util::W1, check_overflow);

	mVUExactVu0AndImm_oaknut(oak::util::W0, oak::util::W0, 0x80000000u);
	oakAsm->FMOV(reg, oak::util::W0);
	oakAsm->B(done);

	oakAsm->l(check_overflow);
	if (CHECK_VU_OVERFLOW(vuIndex))
	{
		oakAsm->MOV(oak::util::W0, 0x7f800000);
		oakAsm->CMP(oak::util::W1, oak::util::W0);
		oakAsm->B(oak::util::NE, done);
		oakAsm->FMOV(oak::util::W0, reg);
		mVUExactVu0AndImm_oaknut(oak::util::W0, oak::util::W0, 0x80000000u);
		oakAsm->MOV(oak::util::W1, 0x7f7fffff);
		oakAsm->ORR(oak::util::W0, oak::util::W0, oak::util::W1);
		oakAsm->FMOV(reg, oak::util::W0);
	}

	oakAsm->l(done);
}

static __fi void mVUExactVu0LoadVuDoubleLane_oaknut(const oak::SReg& dst, s64 offset, int vuIndex)
{
	oakLoad32(oak::util::W0, mVUExactVu0CpuMem(offset));
	oakAsm->FMOV(dst, oak::util::W0);
	mVUExactVu0VuDoubleScalar_oaknut(dst, vuIndex);
}

static __fi void mVUExactVu0ClearMacLane_oaknut(const oak::WReg& mac, int lane)
{
	const u32 clear_mask = 0x1111u << mVUExactVu0LaneShift(lane);
	mVUExactVu0AndImm_oaknut(mac, mac, static_cast<u32>(~clear_mask));
}

static __fi void mVUExactVu0StoreResultWord_oaknut(s64 resultOffset, const oak::WReg& value)
{
	if (resultOffset >= 0)
		oakStore32(value, mVUExactVu0CpuMem(resultOffset));
}

static __fi void mVUExactVu0StoreMacResultLaneTo_oaknut(int vuIndex, int lane, const oak::SReg& result, const oak::WReg& mac, s64 resultOffset)
{
	const int shift = mVUExactVu0LaneShift(lane);
	const u32 sign_mask = 0x0010u << shift;
	const u32 zero_clear_mask = 0x1100u << shift;
	const u32 denorm_clear_mask = 0x1000u << shift;
	const u32 denorm_set_mask = 0x0101u << shift;
	const u32 inf_clear_mask = 0x0101u << shift;
	const u32 inf_set_mask = 0x1000u << shift;
	const u32 normal_clear_mask = 0x1101u << shift;

	oak::Label sign_done;
	oak::Label positive;
	oak::Label not_zero;
	oak::Label not_denorm;
	oak::Label normal;
	oak::Label done;

	oakAsm->FMOV(oak::util::W0, result);
	mVUExactVu0TstImm_oaknut(oak::util::W0, 0x80000000u);
	oakAsm->B(oak::util::EQ, positive);
	mVUExactVu0OrrImm_oaknut(mac, mac, sign_mask);
	oakAsm->B(sign_done);

	oakAsm->l(positive);
	mVUExactVu0AndImm_oaknut(mac, mac, static_cast<u32>(~sign_mask));

	oakAsm->l(sign_done);
	oakAsm->FMOV(oak::util::W0, result);
	mVUExactVu0AndImm_oaknut(oak::util::W1, oak::util::W0, 0x7fffffffu);
	oakAsm->CBNZ(oak::util::W1, not_zero);
	mVUExactVu0AndImm_oaknut(mac, mac, static_cast<u32>(~zero_clear_mask));
	mVUExactVu0OrrImm_oaknut(mac, mac, 0x0001u << shift);
	mVUExactVu0StoreResultWord_oaknut(resultOffset, oak::util::W0);
	oakAsm->B(done);

	oakAsm->l(not_zero);
	oakAsm->FMOV(oak::util::W0, result);
	oakAsm->LSR(oak::util::W1, oak::util::W0, 23);
	oakAsm->AND(oak::util::W1, oak::util::W1, 0xff);
	oakAsm->CBNZ(oak::util::W1, not_denorm);
	mVUExactVu0AndImm_oaknut(mac, mac, static_cast<u32>(~denorm_clear_mask));
	mVUExactVu0OrrImm_oaknut(mac, mac, denorm_set_mask);
	mVUExactVu0AndImm_oaknut(oak::util::W0, oak::util::W0, 0x80000000u);
	mVUExactVu0StoreResultWord_oaknut(resultOffset, oak::util::W0);
	oakAsm->B(done);

	oakAsm->l(not_denorm);
	oakAsm->CMP(oak::util::W1, 255);
	oakAsm->B(oak::util::NE, normal);
	mVUExactVu0AndImm_oaknut(mac, mac, static_cast<u32>(~inf_clear_mask));
	mVUExactVu0OrrImm_oaknut(mac, mac, inf_set_mask);
	oakAsm->FMOV(oak::util::W0, result);
	if (CHECK_VU_OVERFLOW(vuIndex))
	{
		mVUExactVu0AndImm_oaknut(oak::util::W0, oak::util::W0, 0x80000000u);
		oakAsm->MOV(oak::util::W1, 0x7f7fffff);
		oakAsm->ORR(oak::util::W0, oak::util::W0, oak::util::W1);
	}
	mVUExactVu0StoreResultWord_oaknut(resultOffset, oak::util::W0);
	oakAsm->B(done);

	oakAsm->l(normal);
	mVUExactVu0AndImm_oaknut(mac, mac, static_cast<u32>(~normal_clear_mask));
	oakAsm->FMOV(oak::util::W0, result);
	mVUExactVu0StoreResultWord_oaknut(resultOffset, oak::util::W0);

	oakAsm->l(done);
}

static __fi void mVUExactVu0StoreMacResultLane_oaknut(int vuIndex, int lane, const oak::SReg& result, const oak::WReg& mac)
{
	mVUExactVu0StoreMacResultLaneTo_oaknut(vuIndex, lane, result, mac, mVUExactVu0AccLaneOffset(vuIndex, lane));
}

static __fi void mVUExactVu0EmitAccLane_oaknut(int vuIndex, int lane, int fs, int ft, mVUExactVu0AccOp op, mVUExactVu0FtMode ftMode, const oak::WReg& mac)
{
	mVUExactVu0LoadVuDoubleLane_oaknut(OAK_SSCRATCH, mVUExactVu0AccLaneOffset(vuIndex, lane), vuIndex);
	mVUExactVu0LoadVuDoubleLane_oaknut(OAK_SSCRATCH2, mVUExactVu0VfLaneOffset(vuIndex, fs, lane), vuIndex);
	if (ftMode == mVUExactVu0FtMode::I)
		mVUExactVu0LoadVuDoubleLane_oaknut(OAK_SSCRATCH3, mVUExactVu0ViOffset(vuIndex, REG_I), vuIndex);
	else
		mVUExactVu0LoadVuDoubleLane_oaknut(OAK_SSCRATCH3, mVUExactVu0VfLaneOffset(vuIndex, ft, mVUExactVu0FtLane(ftMode, lane)), vuIndex);

	oakAsm->FMUL(OAK_SSCRATCH2, OAK_SSCRATCH2, OAK_SSCRATCH3);
	if (op == mVUExactVu0AccOp::MAdd)
		oakAsm->FADD(OAK_SSCRATCH, OAK_SSCRATCH, OAK_SSCRATCH2);
	else
		oakAsm->FSUB(OAK_SSCRATCH, OAK_SSCRATCH, OAK_SSCRATCH2);

	mVUExactVu0StoreMacResultLane_oaknut(vuIndex, lane, OAK_SSCRATCH, mac);
}

static __fi void mVUExactVu0EmitDestLane_oaknut(int vuIndex, int lane, int fd, int fs, int ft, mVUExactVu0AccOp op, mVUExactVu0FtMode ftMode, const oak::WReg& mac)
{
	mVUExactVu0LoadVuDoubleLane_oaknut(OAK_SSCRATCH, mVUExactVu0AccLaneOffset(vuIndex, lane), vuIndex);
	mVUExactVu0LoadVuDoubleLane_oaknut(OAK_SSCRATCH2, mVUExactVu0VfLaneOffset(vuIndex, fs, lane), vuIndex);
	if (ftMode == mVUExactVu0FtMode::I)
		mVUExactVu0LoadVuDoubleLane_oaknut(OAK_SSCRATCH3, mVUExactVu0ViOffset(vuIndex, REG_I), vuIndex);
	else
		mVUExactVu0LoadVuDoubleLane_oaknut(OAK_SSCRATCH3, mVUExactVu0VfLaneOffset(vuIndex, ft, mVUExactVu0FtLane(ftMode, lane)), vuIndex);

	oakAsm->FMUL(OAK_SSCRATCH2, OAK_SSCRATCH2, OAK_SSCRATCH3);
	if (op == mVUExactVu0AccOp::MAdd)
		oakAsm->FADD(OAK_SSCRATCH, OAK_SSCRATCH, OAK_SSCRATCH2);
	else
		oakAsm->FSUB(OAK_SSCRATCH, OAK_SSCRATCH, OAK_SSCRATCH2);

	const s64 resultOffset = (fd != 0) ? mVUExactVu0VfLaneOffset(vuIndex, fd, lane) : -1;
	mVUExactVu0StoreMacResultLaneTo_oaknut(vuIndex, lane, OAK_SSCRATCH, mac, resultOffset);
}

static __fi void mVUExactVu0SetStatusBitIf_oaknut(const oak::WReg& status, const oak::WReg& mac, u32 test_mask, u32 status_bit)
{
	oak::Label skip;
	mVUExactVu0TstImm_oaknut(mac, test_mask);
	oakAsm->B(oak::util::EQ, skip);
	mVUExactVu0OrrImm_oaknut(status, status, status_bit);
	oakAsm->l(skip);
}

static __fi void mVUExactVu0StatUpdate_oaknut(int vuIndex, const oak::WReg& mac)
{
	oakAsm->MOV(oak::util::W0, 0);
	mVUExactVu0SetStatusBitIf_oaknut(oak::util::W0, mac, 0x000fu, 0x1);
	mVUExactVu0SetStatusBitIf_oaknut(oak::util::W0, mac, 0x00f0u, 0x2);
	mVUExactVu0SetStatusBitIf_oaknut(oak::util::W0, mac, 0x0f00u, 0x4);
	mVUExactVu0SetStatusBitIf_oaknut(oak::util::W0, mac, 0xf000u, 0x8);
	oakStore32(oak::util::W0, mVUExactVu0CpuMem(mVUExactVu0StatusFlagOffset(vuIndex)));
}

static void mVUExactVu0AccOp_emit_oaknut(mP, mVUExactVu0AccOp op, mVUExactVu0FtMode ftMode)
{
	mVU.regAlloc->flushAll();
	const int vuIndex = mVU.index;

	recBeginOaknutEmit();
	oakLoad32(OAK_WSCRATCH2, mVUExactVu0CpuMem(mVUExactVu0MacFlagOffset(vuIndex)));

	if (_X)
		mVUExactVu0EmitAccLane_oaknut(vuIndex, 0, _Fs_, _Ft_, op, ftMode, OAK_WSCRATCH2);
	else
		mVUExactVu0ClearMacLane_oaknut(OAK_WSCRATCH2, 0);

	if (_Y)
		mVUExactVu0EmitAccLane_oaknut(vuIndex, 1, _Fs_, _Ft_, op, ftMode, OAK_WSCRATCH2);
	else
		mVUExactVu0ClearMacLane_oaknut(OAK_WSCRATCH2, 1);

	if (_Z)
		mVUExactVu0EmitAccLane_oaknut(vuIndex, 2, _Fs_, _Ft_, op, ftMode, OAK_WSCRATCH2);
	else
		mVUExactVu0ClearMacLane_oaknut(OAK_WSCRATCH2, 2);

	if (_W)
		mVUExactVu0EmitAccLane_oaknut(vuIndex, 3, _Fs_, _Ft_, op, ftMode, OAK_WSCRATCH2);
	else
		mVUExactVu0ClearMacLane_oaknut(OAK_WSCRATCH2, 3);

	oakStore32(OAK_WSCRATCH2, mVUExactVu0CpuMem(mVUExactVu0MacFlagOffset(vuIndex)));
	mVUExactVu0StatUpdate_oaknut(vuIndex, OAK_WSCRATCH2);
	recEndOaknutEmit();
}

static void mVUExactVu0DestOp_emit_oaknut(mP, mVUExactVu0AccOp op, mVUExactVu0FtMode ftMode)
{
	mVU.regAlloc->flushAll();
	const int vuIndex = mVU.index;

	recBeginOaknutEmit();
	oakLoad32(OAK_WSCRATCH2, mVUExactVu0CpuMem(mVUExactVu0MacFlagOffset(vuIndex)));

	if (_X)
		mVUExactVu0EmitDestLane_oaknut(vuIndex, 0, _Fd_, _Fs_, _Ft_, op, ftMode, OAK_WSCRATCH2);
	else
		mVUExactVu0ClearMacLane_oaknut(OAK_WSCRATCH2, 0);

	if (_Y)
		mVUExactVu0EmitDestLane_oaknut(vuIndex, 1, _Fd_, _Fs_, _Ft_, op, ftMode, OAK_WSCRATCH2);
	else
		mVUExactVu0ClearMacLane_oaknut(OAK_WSCRATCH2, 1);

	if (_Z)
		mVUExactVu0EmitDestLane_oaknut(vuIndex, 2, _Fd_, _Fs_, _Ft_, op, ftMode, OAK_WSCRATCH2);
	else
		mVUExactVu0ClearMacLane_oaknut(OAK_WSCRATCH2, 2);

	if (_W)
		mVUExactVu0EmitDestLane_oaknut(vuIndex, 3, _Fd_, _Fs_, _Ft_, op, ftMode, OAK_WSCRATCH2);
	else
		mVUExactVu0ClearMacLane_oaknut(OAK_WSCRATCH2, 3);

	oakStore32(OAK_WSCRATCH2, mVUExactVu0CpuMem(mVUExactVu0MacFlagOffset(vuIndex)));
	mVUExactVu0StatUpdate_oaknut(vuIndex, OAK_WSCRATCH2);
	recEndOaknutEmit();
}

static void mVUupdateFlags_oaknut(mV, int reg, int regT1in = VU_HOST_NO_XMM, int regT2in = VU_HOST_NO_XMM, bool modXYZW = true)
{
	const int mReg = VU_HOST_T1;
	const int sReg = getFlagRegId(sFLAG.write);
	bool regT1b = regT1in == VU_HOST_NO_XMM, regT2b = false;

	if (!sFLAG.doFlag && !mFLAG.doFlag)
		return;

	const int regT1 = regT1b ? mVU.regAlloc->allocRegId() : regT1in;
	int regT2 = reg;
	if (mFLAG.doFlag && !(_XYZW_SS && modXYZW))
	{
		regT2 = regT2in;
		if (regT2 == VU_HOST_NO_XMM)
		{
			regT2 = mVU.regAlloc->allocRegId();
			regT2b = true;
		}
		recBeginOaknutEmit();
		mVUUpperPshufd_oaknut(regT2, reg, 0x1B);
		recEndOaknutEmit();
	}

	if (sFLAG.doFlag)
		mVUallocSFLAGa(sReg, sFLAG.lastWrite);

	const oak::QReg t2_q = oakQRegister(regT2);
	const oak::QReg t1_q = oakQRegister(regT1);
	const oak::WReg mac_w = oakWRegister(mReg);
	const oak::WReg status_w = oakWRegister(sReg);
	const oak::WReg temp_w = oakWRegister(VU_HOST_T2);

	recBeginOaknutEmit();
	if (sFLAG.doFlag && sFLAG.doNonSticky)
	{
		oakAsm->MOV(OAK_WSCRATCH, 0xfffc00ff);
		oakAsm->AND(status_w, status_w, OAK_WSCRATCH);
	}

	mVUUpperMovmskps_oaknut(mac_w, t2_q);
	oakAsm->MOVI(t1_q.B16(), 0);
	oakAsm->FCMEQ(t1_q.S4(), t1_q.S4(), t2_q.S4());
	mVUUpperMovmskps_oaknut(temp_w, t1_q);
	oakAsm->MOV(OAK_WSCRATCH, AND_XYZW);
	oakAsm->AND(mac_w, mac_w, OAK_WSCRATCH);
	oakAsm->LSL(mac_w, mac_w, 4);
	oakAsm->AND(temp_w, temp_w, OAK_WSCRATCH);
	oakAsm->ORR(mac_w, mac_w, temp_w);

	if (sFLAG.doFlag && CHECK_VUOVERFLOWHACK)
	{
		oak::Label no_overflow;
		oakAsm->MOV(t1_q.B16(), t2_q.B16());
		oakLoad128(OAK_QSCRATCH3, mVUUpperOakSs4Mem(offsetof(mVU_SSE4, sse4_compvals[1][0])));
		oakAsm->AND(t1_q.B16(), t1_q.B16(), OAK_QSCRATCH3.B16());
		oakLoad128(OAK_QSCRATCH3, mVUUpperOakSs4Mem(offsetof(mVU_SSE4, sse4_compvals[0][0])));
		oakAsm->FCMEQ(t1_q.S4(), t1_q.S4(), OAK_QSCRATCH3.S4());
		mVUUpperMovmskps_oaknut(temp_w, t1_q);
		oakAsm->MOV(OAK_WSCRATCH, AND_XYZW);
		oakAsm->AND(temp_w, temp_w, OAK_WSCRATCH);
		oakAsm->CBZ(temp_w, no_overflow);
		oakAsm->MOV(OAK_WSCRATCH, 0x820000);
		oakAsm->ORR(status_w, status_w, OAK_WSCRATCH);
		if (mFLAG.doFlag)
		{
			oakAsm->LSL(temp_w, temp_w, 12);
			oakAsm->ORR(mac_w, mac_w, temp_w);
		}
		oakAsm->l(no_overflow);
	}

	if (_XYZW_SS && modXYZW && !_W)
		oakAsm->LSL(mac_w, mac_w, ADD_XYZW);
	recEndOaknutEmit();

	if (mFLAG.doFlag)
		mVUallocMFLAGb(mVU, mReg, mFLAG.write);

	if (sFLAG.doFlag)
	{
		recBeginOaknutEmit();
		oakAsm->MOV(OAK_WSCRATCH, 0xff);
		oakAsm->AND(mac_w, mac_w, OAK_WSCRATCH);
		oakAsm->ORR(status_w, status_w, mac_w);
		if (sFLAG.doNonSticky)
		{
			oakAsm->LSL(mac_w, mac_w, 8);
			oakAsm->ORR(status_w, status_w, mac_w);
		}
		recEndOaknutEmit();
	}

	if (regT1b)
		mVU.regAlloc->clearNeededXmmId(regT1);
	if (regT2b)
		mVU.regAlloc->clearNeededXmmId(regT2);
}

//------------------------------------------------------------------
// Helper Macros and Functions
//------------------------------------------------------------------

// ADD Opcode
static void mVU_ADD_direct_emit_oaknut(mP)
{
	int Ft;
	int tempFt;
	const bool needsFullFtForClamp = clampE;

	if (_XYZW_SS2)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, _X_Y_Z_W);
		tempFt = Ft;
	}
	else if (needsFullFtForClamp)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, 0xf);
		tempFt = Ft;
	}
	else
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_);
		tempFt = VU_HOST_NO_XMM;
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);
	const bool needsCOP2VuDouble = isVU0 && isCOP2 && _X_Y_Z_W == 0xe;
	int FtDouble = Ft;
	int t1 = VU_HOST_NO_XMM;
	int t2 = VU_HOST_NO_XMM;
	if (needsCOP2VuDouble)
	{
		FtDouble = mVU.regAlloc->allocRegId();
		t1 = mVU.regAlloc->allocRegId();
		t2 = mVU.regAlloc->allocRegId();
	}

	recBeginOaknutEmit();
	if (needsCOP2VuDouble)
	{
		oakAsm->MOV(oakQRegister(FtDouble).B16(), oakQRegister(Ft).B16());
		mVUUpperVuDoubleVector_oaknut(Fs, t1, t2);
		mVUUpperVuDoubleVector_oaknut(FtDouble, t1, t2);
	}
	if (_XYZW_SS)
		mVUUpperAddSs_oaknut(mVU, Fs, FtDouble);
	else
		mVUUpperAddPs_oaknut(mVU, Fs, FtDouble);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, tempFt);

	mVU.regAlloc->clearNeededXmmId(Fs);
	if (needsCOP2VuDouble)
	{
		mVU.regAlloc->clearNeededXmmId(t2);
		mVU.regAlloc->clearNeededXmmId(t1);
		mVU.regAlloc->clearNeededXmmId(FtDouble);
	}
	mVU.regAlloc->clearNeededXmmId(Ft);
}

static void mVU_ADD_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		mVU_ADD_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("ADD");
		mVUlogFd();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// ADDA Opcode
static void mVU_ADDA_direct_emit_oaknut(mP)
{
	int Ft;
	int tempFt;
	if (_XYZW_SS2)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, _X_Y_Z_W);
		tempFt = Ft;
	}
	else if (clampE)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, 0xf);
		tempFt = Ft;
	}
	else
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_);
		tempFt = VU_HOST_NO_XMM;
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
		mVUUpperAddSs_oaknut(mVU, Fs, Ft);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFt);
		recBeginOaknutEmit();
		const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
		oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
		oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}
	else
	{
		recBeginOaknutEmit();
		if (_XYZW_SS)
		{
			mVUUpperAddSs_oaknut(mVU, Fs, Ft);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperAddPs_oaknut(mVU, Fs, Ft);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFt);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Ft);
}

static void mVU_ADDA_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_ADDA_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("ADDA");
		mVUlogACC();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// ADDAi Opcode
static void mVU_ADDAi_direct_emit_oaknut(mP)
{
	const int Fi = mVU.regAlloc->allocRegId(33, 0, _X_Y_Z_W);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
		mVUUpperAddSs_oaknut(mVU, Fs, Fi);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, Fi);
		recBeginOaknutEmit();
		const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
		oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
		oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}
	else
	{
		recBeginOaknutEmit();
		if (_XYZW_SS)
		{
			mVUUpperAddSs_oaknut(mVU, Fs, Fi);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperAddPs_oaknut(mVU, Fs, Fi);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, Fi);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fi);
}

static void mVU_ADDAi_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2 { mVU_ADDAi_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("ADDAi");
		mVUlogACC();
		mVUlogI();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// ADDAq Opcode
static void mVU_ADDAq_direct_emit_oaknut(mP)
{
	int Fq;
	int tempFq;
	if (!clampE && _XYZW_SS && !mVUinfo.readQ)
	{
		Fq = VU_HOST_XMMPQ;
		tempFq = VU_HOST_NO_XMM;
	}
	else
	{
		Fq = mVU.regAlloc->allocRegId();
		tempFq = Fq;
		recBeginOaknutEmit();
		mVUUpperGetQreg_oaknut(Fq, mVUinfo.readQ);
		recEndOaknutEmit();
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
		mVUUpperAddSs_oaknut(mVU, Fs, Fq);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFq);
		recBeginOaknutEmit();
		const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
		oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
		oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}
	else
	{
		recBeginOaknutEmit();
		if (_XYZW_SS)
		{
			mVUUpperAddSs_oaknut(mVU, Fs, Fq);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperAddPs_oaknut(mVU, Fs, Fq);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFq);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fq);
}

static void mVU_ADDAq_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2 { mVU_ADDAq_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("ADDAq");
		mVUlogACC();
		mVUlogQ();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_ADDA_lane_direct_emit_oaknut(microVU& mVU, int recPass, int lane)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtL = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtL, FtRaw, lane);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
		mVUUpperAddSs_oaknut(mVU, Fs, FtL);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtL);
		recBeginOaknutEmit();
		const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
		oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
		oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}
	else
	{
		recBeginOaknutEmit();
		if (_XYZW_SS)
		{
			mVUUpperAddSs_oaknut(mVU, Fs, FtL);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperAddPs_oaknut(mVU, Fs, FtL);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtL);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtL);
}

static void mVU_ADDAx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_ADDA_lane_direct_emit_oaknut(mVU, recPass, 0); }
	pass3 { mVUlog("ADDA"); mVUlogACC(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_ADDAy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_ADDA_lane_direct_emit_oaknut(mVU, recPass, 1); }
	pass3 { mVUlog("ADDA"); mVUlogACC(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_ADDAz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_ADDA_lane_direct_emit_oaknut(mVU, recPass, 2); }
	pass3 { mVUlog("ADDA"); mVUlogACC(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_ADDAw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_ADDA_lane_direct_emit_oaknut(mVU, recPass, 3); }
	pass3 { mVUlog("ADDA"); mVUlogACC(); mVUlog(", vf%02dw", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MADDAi Opcode
static void mVU_MADDAi_direct_emit_oaknut(mP)
{
	const int Fi = mVU.regAlloc->allocRegId(33, 0, _X_Y_Z_W);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId(32, 32, 0xf, false);

	if (_XYZW_SS || _X_Y_Z_W == 0xf)
	{
		if (_XYZW_SS2)
		{
			const int tempACC = mVU.regAlloc->allocRegId();
			recBeginOaknutEmit();
			oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
			mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, Fi);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, Fi);
			recBeginOaknutEmit();
			oakAsm->MOV(oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))], oakQRegister(ACC).Selem()[0]);
			oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
			recEndOaknutEmit();
			mVU.regAlloc->clearNeededXmmId(tempACC);
		}
		else
		{
			recBeginOaknutEmit();
			if (_XYZW_SS)
				mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, Fi);
			else
				mVUUpperFmlaPs_oaknut(mVU, ACC, Fs, Fi);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, Fi);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperFmlaPs_oaknut(mVU, tempACC, Fs, Fi);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, Fi);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fi);
}

static void mVU_MADDAi_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2
	{
		if (mVUNeedsVu0MicroAccExactPath(mVU, mVUExactVu0AccOp::MAdd, mVUExactVu0FtMode::I))
			mVUExactVu0AccOp_emit_oaknut(mVU, recPass, mVUExactVu0AccOp::MAdd, mVUExactVu0FtMode::I);
		else
			mVU_MADDAi_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("MADDAi");
		mVUlogACC();
		mVUlogI();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MADDA Opcode
static void mVU_MADDA_direct_emit_oaknut(mP)
{
	int Ft;
	int tempFt;
	if (_XYZW_SS2)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, _X_Y_Z_W);
		tempFt = Ft;
	}
	else if (clampE)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, 0xf);
		tempFt = Ft;
	}
	else
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_);
		tempFt = VU_HOST_NO_XMM;
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId(32, 32, 0xf, false);

	if (_XYZW_SS || _X_Y_Z_W == 0xf)
	{
		if (_XYZW_SS2)
		{
			const int tempACC = mVU.regAlloc->allocRegId();
			recBeginOaknutEmit();
			oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
			mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, Ft);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFt);
			recBeginOaknutEmit();
			const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
			oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
			oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
			recEndOaknutEmit();
			mVU.regAlloc->clearNeededXmmId(tempACC);
		}
		else
		{
			recBeginOaknutEmit();
			if (_XYZW_SS)
				mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, Ft);
			else
				mVUUpperFmlaPs_oaknut(mVU, ACC, Fs, Ft);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFt);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperFmlaPs_oaknut(mVU, tempACC, Fs, Ft);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFt);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Ft);
}

static void mVU_MADDA_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MADDA_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MADDA");
		mVUlogACC();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MADDAq Opcode
static void mVU_MADDAq_direct_emit_oaknut(mP)
{
	int Fq;
	int tempFq;
	if (!clampE && _XYZW_SS && !mVUinfo.readQ)
	{
		Fq = VU_HOST_XMMPQ;
		tempFq = VU_HOST_NO_XMM;
	}
	else
	{
		Fq = mVU.regAlloc->allocRegId();
		tempFq = Fq;
		recBeginOaknutEmit();
		mVUUpperGetQreg_oaknut(Fq, mVUinfo.readQ);
		recEndOaknutEmit();
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId(32, 32, 0xf, false);

	if (_XYZW_SS || _X_Y_Z_W == 0xf)
	{
		if (_XYZW_SS2)
		{
			const int tempACC = mVU.regAlloc->allocRegId();
			recBeginOaknutEmit();
			oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
			mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, Fq);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFq);
			recBeginOaknutEmit();
			const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
			oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
			oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
			recEndOaknutEmit();
			mVU.regAlloc->clearNeededXmmId(tempACC);
		}
		else
		{
			recBeginOaknutEmit();
			if (_XYZW_SS)
				mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, Fq);
			else
				mVUUpperFmlaPs_oaknut(mVU, ACC, Fs, Fq);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFq);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperFmlaPs_oaknut(mVU, tempACC, Fs, Fq);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFq);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fq);
}

static void mVU_MADDAq_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2 { mVU_MADDAq_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MADDAq");
		mVUlogACC();
		mVUlogQ();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MADDA lane Opcodes
static void mVU_MADDA_lane_direct_emit_oaknut(microVU& mVU, int recPass, int lane)
{
	const bool useFtLane = !clampE;
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	int FtL = VU_HOST_NO_XMM;
	if (!useFtLane)
	{
		FtL = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperDupLane_oaknut(FtL, FtRaw, lane);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(FtRaw);
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId(32, 32, 0xf, false);

	if (_XYZW_SS || _X_Y_Z_W == 0xf)
	{
		if (_XYZW_SS2)
		{
			const int tempACC = mVU.regAlloc->allocRegId();
			recBeginOaknutEmit();
			oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
			mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
			if (useFtLane)
				mVUUpperFmlaSsLane_oaknut(ACC, Fs, FtRaw, lane);
			else
				mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, FtL);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, useFtLane ? VU_HOST_NO_XMM : FtL);
			recBeginOaknutEmit();
			const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
			oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
			oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
			recEndOaknutEmit();
			mVU.regAlloc->clearNeededXmmId(tempACC);
		}
		else
		{
			recBeginOaknutEmit();
			if (_XYZW_SS)
				mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
			else
				mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
			if (_XYZW_SS)
			{
				if (useFtLane)
					mVUUpperFmlaSsLane_oaknut(ACC, Fs, FtRaw, lane);
				else
					mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, FtL);
			}
			else
			{
				if (useFtLane)
					mVUUpperFmlaPsLane_oaknut(ACC, Fs, FtRaw, lane);
				else
					mVUUpperFmlaPs_oaknut(mVU, ACC, Fs, FtL);
			}
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, useFtLane ? VU_HOST_NO_XMM : FtL);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		if (useFtLane)
			mVUUpperFmlaPsLane_oaknut(tempACC, Fs, FtRaw, lane);
		else
			mVUUpperFmlaPs_oaknut(mVU, tempACC, Fs, FtL);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, useFtLane ? VU_HOST_NO_XMM : FtL);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	if (useFtLane)
		mVU.regAlloc->clearNeededXmmId(FtRaw);
	else
		mVU.regAlloc->clearNeededXmmId(FtL);
}

static __fi bool mVUNeedsVu1MaddaExactFlagsPath(const microVU& mVU)
{
	// The VU1 MADDA memory-exact path is expensive; only use it when VU1 clamping accuracy is enabled and flags are consumed.
	return isVU1 && CHECK_VU_OVERFLOW(1) && (sFLAG.doFlag || mFLAG.doFlag);
}

static void mVU_MADDAx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { if (mVUNeedsVu1MaddaExactFlagsPath(mVU)) mVUExactVu0AccOp_emit_oaknut(mVU, recPass, mVUExactVu0AccOp::MAdd, mVUExactVu0FtMode::X); else mVU_MADDA_lane_direct_emit_oaknut(mVU, recPass, 0); }
	pass3 { mVUlog("MADDA"); mVUlogACC(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MADDAy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { if (mVUNeedsVu1MaddaExactFlagsPath(mVU) || mVUNeedsVu0MicroAccExactPath(mVU, mVUExactVu0AccOp::MAdd, mVUExactVu0FtMode::Y)) mVUExactVu0AccOp_emit_oaknut(mVU, recPass, mVUExactVu0AccOp::MAdd, mVUExactVu0FtMode::Y); else mVU_MADDA_lane_direct_emit_oaknut(mVU, recPass, 1); }
	pass3 { mVUlog("MADDA"); mVUlogACC(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MADDAz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { if (mVUNeedsVu1MaddaExactFlagsPath(mVU) || mVUNeedsVu0MicroAccExactPath(mVU, mVUExactVu0AccOp::MAdd, mVUExactVu0FtMode::Z)) mVUExactVu0AccOp_emit_oaknut(mVU, recPass, mVUExactVu0AccOp::MAdd, mVUExactVu0FtMode::Z); else mVU_MADDA_lane_direct_emit_oaknut(mVU, recPass, 2); }
	pass3 { mVUlog("MADDA"); mVUlogACC(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MADDAw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		if (mVUNeedsVu0MicroAccExactPath(mVU, mVUExactVu0AccOp::MAdd, mVUExactVu0FtMode::W))
		{
			mVUExactVu0AccOp_emit_oaknut(mVU, recPass, mVUExactVu0AccOp::MAdd, mVUExactVu0FtMode::W);
		}
		else if (isVU0 && isCOP2)
		{
			// COP2 VMADDAw needs the per-lane VU0 exact path, but keeping it scoped here avoids the wider VU1 MADDA cost.
			iFlushCall(FLUSH_FREE_VU0);
			mVUExactVu0AccOp_emit_oaknut(mVU, recPass, mVUExactVu0AccOp::MAdd, mVUExactVu0FtMode::W);
		}
		else
		{
			mVU_MADDA_lane_direct_emit_oaknut(mVU, recPass, 3);
		}
	}
	pass3 { mVUlog("MADDA"); mVUlogACC(); mVUlog(", vf%02dw", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MADD Opcode
static void mVU_MADD_direct_emit_oaknut(mP)
{
	int Ft;
	int tempFt;
	if (_XYZW_SS2)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, _X_Y_Z_W);
		tempFt = Ft;
	}
	else if (clampE)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, 0xf);
		tempFt = Ft;
	}
	else
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_);
		tempFt = VU_HOST_NO_XMM;
	}

	const int ACC = mVU.regAlloc->allocRegId(32);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);
	const int tempFs = mVU.regAlloc->allocRegId();

	recBeginOaknutEmit();
	oakAsm->MOV(oakQRegister(tempFs).B16(), oakQRegister(Fs).B16());
	oakAsm->MOV(oakQRegister(Fs).B16(), oakQRegister(ACC).B16());
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(Fs, Fs, shuffleSS(_X_Y_Z_W));
	if (_XYZW_SS)
		mVUUpperFmlaSs_oaknut(mVU, Fs, tempFs, Ft);
	else
		mVUUpperFmlaPs_oaknut(mVU, Fs, tempFs, Ft);
	recEndOaknutEmit();

	mVU.regAlloc->clearNeededXmmId(tempFs);

	mVUupdateFlags_oaknut(mVU, Fs, tempFt);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Ft);
	mVU.regAlloc->clearNeededXmmId(ACC);
}

static void mVU_MADD_cop2_emit_oaknut(mP)
{
	int FtRaw;
	if (_XYZW_SS2)
		FtRaw = mVU.regAlloc->allocRegId(_Ft_, 0, _X_Y_Z_W);
	else
		FtRaw = mVU.regAlloc->allocRegId(_Ft_);

	const int Ft = mVU.regAlloc->allocRegId();
	const int ACC = mVU.regAlloc->allocRegId(32);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);
	const int tempFs = mVU.regAlloc->allocRegId();
	const int t1 = mVU.regAlloc->allocRegId();
	const int t2 = mVU.regAlloc->allocRegId();

	recBeginOaknutEmit();
	oakAsm->MOV(oakQRegister(Ft).B16(), oakQRegister(FtRaw).B16());
	oakAsm->MOV(oakQRegister(tempFs).B16(), oakQRegister(Fs).B16());
	mVUUpperVuDoubleVector_oaknut(Ft, t1, t2);
	mVUUpperVuDoubleVector_oaknut(tempFs, t1, t2);
	oakAsm->MOV(oakQRegister(Fs).B16(), oakQRegister(ACC).B16());
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(Fs, Fs, shuffleSS(_X_Y_Z_W));
	mVUUpperVuDoubleVector_oaknut(Fs, t1, t2);
	if (_XYZW_SS)
	{
		oakAsm->FMUL(OAK_SSCRATCH, oakSRegister(tempFs), oakQRegister(Ft).Selem()[0]);
		oakAsm->FADD(oakSRegister(Fs), oakSRegister(Fs), OAK_SSCRATCH);
	}
	else
	{
		oakAsm->FMUL(OAK_QSCRATCH.S4(), oakQRegister(tempFs).S4(), oakQRegister(Ft).S4());
		oakAsm->FADD(oakQRegister(Fs).S4(), oakQRegister(Fs).S4(), OAK_QSCRATCH.S4());
	}
	recEndOaknutEmit();

	mVU.regAlloc->clearNeededXmmId(tempFs);
	mVUupdateFlags_oaknut(mVU, Fs, Ft);

	mVU.regAlloc->clearNeededXmmId(t2);
	mVU.regAlloc->clearNeededXmmId(t1);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Ft);
	mVU.regAlloc->clearNeededXmmId(FtRaw);
	mVU.regAlloc->clearNeededXmmId(ACC);
}

static void mVU_MADD_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { if (isVU0 && isCOP2) mVU_MADD_cop2_emit_oaknut(mVU, recPass); else mVU_MADD_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MADD");
		mVUlogFd();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MADDi Opcode
static void mVU_MADDi_direct_emit_oaknut(mP)
{
	const int Fi = mVU.regAlloc->allocRegId(33, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId(32);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);
	const int tempFs = mVU.regAlloc->allocRegId();

	recBeginOaknutEmit();
	oakAsm->MOV(oakQRegister(tempFs).B16(), oakQRegister(Fs).B16());
	oakAsm->MOV(oakQRegister(Fs).B16(), oakQRegister(ACC).B16());
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(Fs, Fs, shuffleSS(_X_Y_Z_W));
	if (_XYZW_SS)
		mVUUpperFmlaSs_oaknut(mVU, Fs, tempFs, Fi);
	else
		mVUUpperFmlaPs_oaknut(mVU, Fs, tempFs, Fi);
	recEndOaknutEmit();

	mVU.regAlloc->clearNeededXmmId(tempFs);

	mVUupdateFlags_oaknut(mVU, Fs, Fi);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fi);
	mVU.regAlloc->clearNeededXmmId(ACC);
}

static void mVU_MADDi_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, 0); }
	pass2 { mVU_MADDi_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MADDi");
		mVUlogFd();
		mVUlogI();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MADDq Opcode
static void mVU_MADDq_direct_emit_oaknut(mP)
{
	int Fq;
	int tempFq;
	if (!clampE && _XYZW_SS && !mVUinfo.readQ)
	{
		Fq = VU_HOST_XMMPQ;
		tempFq = VU_HOST_NO_XMM;
	}
	else
	{
		Fq = mVU.regAlloc->allocRegId();
		tempFq = Fq;
		recBeginOaknutEmit();
		mVUUpperGetQreg_oaknut(Fq, mVUinfo.readQ);
		recEndOaknutEmit();
	}

	const int ACC = mVU.regAlloc->allocRegId(32);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);
	const int tempFs = mVU.regAlloc->allocRegId();

	recBeginOaknutEmit();
	oakAsm->MOV(oakQRegister(tempFs).B16(), oakQRegister(Fs).B16());
	oakAsm->MOV(oakQRegister(Fs).B16(), oakQRegister(ACC).B16());
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(Fs, Fs, shuffleSS(_X_Y_Z_W));
	if (_XYZW_SS)
		mVUUpperFmlaSs_oaknut(mVU, Fs, tempFs, Fq);
	else
		mVUUpperFmlaPs_oaknut(mVU, Fs, tempFs, Fq);
	recEndOaknutEmit();

	mVU.regAlloc->clearNeededXmmId(tempFs);

	mVUupdateFlags_oaknut(mVU, Fs, tempFq);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fq);
	mVU.regAlloc->clearNeededXmmId(ACC);
}

static void mVU_MADDq_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, 0); }
	pass2 { mVU_MADDq_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MADDq");
		mVUlogFd();
		mVUlogQ();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MADD lane Opcodes
static void mVU_MADD_lane_direct_emit_oaknut(microVU& mVU, int recPass, int lane)
{
	const bool useFtLane = !clampE;
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	int FtL = VU_HOST_NO_XMM;
	if (!useFtLane)
	{
		FtL = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperDupLane_oaknut(FtL, FtRaw, lane);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(FtRaw);
	}

	const int ACC = mVU.regAlloc->allocRegId(32);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);
	const int tempFs = mVU.regAlloc->allocRegId();

	recBeginOaknutEmit();
	oakAsm->MOV(oakQRegister(tempFs).B16(), oakQRegister(Fs).B16());
	oakAsm->MOV(oakQRegister(Fs).B16(), oakQRegister(ACC).B16());
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(Fs, Fs, shuffleSS(_X_Y_Z_W));
	if (_XYZW_SS)
	{
		if (useFtLane)
			mVUUpperFmlaSsLane_oaknut(Fs, tempFs, FtRaw, lane);
		else
			mVUUpperFmlaSs_oaknut(mVU, Fs, tempFs, FtL);
	}
	else
	{
		if (useFtLane)
			mVUUpperFmlaPsLane_oaknut(Fs, tempFs, FtRaw, lane);
		else
			mVUUpperFmlaPs_oaknut(mVU, Fs, tempFs, FtL);
	}
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, useFtLane ? tempFs : FtL);

	mVU.regAlloc->clearNeededXmmId(tempFs);
	mVU.regAlloc->clearNeededXmmId(Fs);
	if (useFtLane)
		mVU.regAlloc->clearNeededXmmId(FtRaw);
	else
		mVU.regAlloc->clearNeededXmmId(FtL);
	mVU.regAlloc->clearNeededXmmId(ACC);
}

static void mVU_MADDx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MADD_lane_direct_emit_oaknut(mVU, recPass, 0); }
	pass3 { mVUlog("MADD"); mVUlogFd(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MADDy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MADD_lane_direct_emit_oaknut(mVU, recPass, 1); }
	pass3 { mVUlog("MADD"); mVUlogFd(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MADDz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MADD_lane_direct_emit_oaknut(mVU, recPass, 2); }
	pass3 { mVUlog("MADD"); mVUlogFd(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MADDw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MADD_lane_direct_emit_oaknut(mVU, recPass, 3); }
	pass3 { mVUlog("MADD"); mVUlogFd(); mVUlog(", vf%02dw", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MSUB Opcode
static void mVU_MSUB_direct_emit_oaknut(mP)
{
	int Ft;
	int tempFt;
	if (_XYZW_SS2)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, _X_Y_Z_W);
		tempFt = Ft;
	}
	else if (clampE)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, 0xf);
		tempFt = Ft;
	}
	else
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_);
		tempFt = VU_HOST_NO_XMM;
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int Fd = mVU.regAlloc->allocRegId(32, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (isCOP2)
	{
		if (_XYZW_SS)
			mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		else
			mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
	}
	if (_XYZW_SS)
		mVUUpperFmlsSs_oaknut(mVU, Fd, Fs, Ft);
	else
		mVUUpperFmlsPs_oaknut(mVU, Fd, Fs, Ft);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fd, Fs, tempFt);

	mVU.regAlloc->clearNeededXmmId(Fd);
	mVU.regAlloc->clearNeededXmmId(Ft);
	mVU.regAlloc->clearNeededXmmId(Fs);
}

static void mVU_MSUB_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MSUB_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MSUB");
		mVUlogFd();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MSUBi Opcode
static void mVU_MSUBi_direct_emit_oaknut(mP)
{
	const int Fi = mVU.regAlloc->allocRegId(33, 0, _X_Y_Z_W);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int Fd = mVU.regAlloc->allocRegId(32, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
		mVUUpperFmlsSs_oaknut(mVU, Fd, Fs, Fi);
	else
		mVUUpperFmlsPs_oaknut(mVU, Fd, Fs, Fi);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fd, Fs, Fi);

	mVU.regAlloc->clearNeededXmmId(Fd);
	mVU.regAlloc->clearNeededXmmId(Fi);
	mVU.regAlloc->clearNeededXmmId(Fs);
}

static void mVU_MSUBi_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, 0); }
	pass2 { mVU_MSUBi_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MSUBi");
		mVUlogFd();
		mVUlogI();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MSUBq Opcode
static void mVU_MSUBq_direct_emit_oaknut(mP)
{
	int Fq;
	int tempFq;
	if (!clampE && _XYZW_SS && !mVUinfo.readQ)
	{
		Fq = VU_HOST_XMMPQ;
		tempFq = VU_HOST_NO_XMM;
	}
	else
	{
		Fq = mVU.regAlloc->allocRegId();
		tempFq = Fq;
		recBeginOaknutEmit();
		mVUUpperGetQreg_oaknut(Fq, mVUinfo.readQ);
		recEndOaknutEmit();
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int Fd = mVU.regAlloc->allocRegId(32, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
		mVUUpperFmlsSs_oaknut(mVU, Fd, Fs, Fq);
	else
		mVUUpperFmlsPs_oaknut(mVU, Fd, Fs, Fq);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fd, Fs, tempFq);

	mVU.regAlloc->clearNeededXmmId(Fd);
	mVU.regAlloc->clearNeededXmmId(Fq);
	mVU.regAlloc->clearNeededXmmId(Fs);
}

static void mVU_MSUBq_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, 0); }
	pass2 { mVU_MSUBq_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MSUBq");
		mVUlogFd();
		mVUlogQ();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MSUB_lane_direct_emit_oaknut(microVU& mVU, int recPass, int lane)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtL = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtL, FtRaw, lane);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int Fd = mVU.regAlloc->allocRegId(32, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
		mVUUpperFmlsSs_oaknut(mVU, Fd, Fs, FtL);
	else
		mVUUpperFmlsPs_oaknut(mVU, Fd, Fs, FtL);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fd, Fs, FtL);

	mVU.regAlloc->clearNeededXmmId(Fd);
	mVU.regAlloc->clearNeededXmmId(FtL);
	mVU.regAlloc->clearNeededXmmId(Fs);
}

static void mVU_MSUBx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MSUB_lane_direct_emit_oaknut(mVU, recPass, 0); }
	pass3 { mVUlog("MSUB"); mVUlogFd(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MSUBy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MSUB_lane_direct_emit_oaknut(mVU, recPass, 1); }
	pass3 { mVUlog("MSUB"); mVUlogFd(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MSUBz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MSUB_lane_direct_emit_oaknut(mVU, recPass, 2); }
	pass3 { mVUlog("MSUB"); mVUlogFd(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MSUBw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MSUB_lane_direct_emit_oaknut(mVU, recPass, 3); }
	pass3 { mVUlog("MSUB"); mVUlogFd(); mVUlog(", vf%02dw", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MSUBA Opcode
static void mVU_MSUBA_direct_emit_oaknut(mP)
{
	int Ft;
	int tempFt;
	if (_XYZW_SS2)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, _X_Y_Z_W);
		tempFt = Ft;
	}
	else if (clampE)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, 0xf);
		tempFt = Ft;
	}
	else
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_);
		tempFt = VU_HOST_NO_XMM;
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId(32, 32, 0xf, false);

	if (_XYZW_SS || _X_Y_Z_W == 0xf)
	{
		if (_XYZW_SS2)
		{
			const int tempACC = mVU.regAlloc->allocRegId();
			recBeginOaknutEmit();
			if (isCOP2)
				mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
			oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
			mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, Ft);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFt);
			recBeginOaknutEmit();
			const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
			oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
			oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
			recEndOaknutEmit();
			mVU.regAlloc->clearNeededXmmId(tempACC);
		}
		else
		{
			recBeginOaknutEmit();
			if (isCOP2)
			{
				if (_XYZW_SS)
					mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
				else
					mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
			}
			if (_XYZW_SS)
				mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, Ft);
			else
				mVUUpperFmlsPs_oaknut(mVU, ACC, Fs, Ft);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFt);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		if (isCOP2)
			mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperFmlsPs_oaknut(mVU, tempACC, Fs, Ft);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFt);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Ft);
}

static void mVU_MSUBA_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, _Ft_); }
	pass2 { if (mVUNeedsVu0MicroAccExactPath(mVU, mVUExactVu0AccOp::MSub, mVUExactVu0FtMode::PerLane)) mVUExactVu0AccOp_emit_oaknut(mVU, recPass, mVUExactVu0AccOp::MSub, mVUExactVu0FtMode::PerLane); else mVU_MSUBA_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MSUBA");
		mVUlogACC();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MSUBAi Opcode
static void mVU_MSUBAi_direct_emit_oaknut(mP)
{
	const int Fi = mVU.regAlloc->allocRegId(33, 0, _X_Y_Z_W);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId(32, 32, 0xf, false);

	if (_XYZW_SS || _X_Y_Z_W == 0xf)
	{
		if (_XYZW_SS2)
		{
			const int tempACC = mVU.regAlloc->allocRegId();
			recBeginOaknutEmit();
			oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
			mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, Fi);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, Fi);
			recBeginOaknutEmit();
			const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
			oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
			oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
			recEndOaknutEmit();
			mVU.regAlloc->clearNeededXmmId(tempACC);
		}
		else
		{
			recBeginOaknutEmit();
			if (_XYZW_SS)
				mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, Fi);
			else
				mVUUpperFmlsPs_oaknut(mVU, ACC, Fs, Fi);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, Fi);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperFmlsPs_oaknut(mVU, tempACC, Fs, Fi);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, Fi);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fi);
}

static void mVU_MSUBAi_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2 { mVU_MSUBAi_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MSUBAi");
		mVUlogACC();
		mVUlogI();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MSUBAq Opcode
static void mVU_MSUBAq_direct_emit_oaknut(mP)
{
	int Fq;
	int tempFq;
	if (!clampE && _XYZW_SS && !mVUinfo.readQ)
	{
		Fq = VU_HOST_XMMPQ;
		tempFq = VU_HOST_NO_XMM;
	}
	else
	{
		Fq = mVU.regAlloc->allocRegId();
		tempFq = Fq;
		recBeginOaknutEmit();
		mVUUpperGetQreg_oaknut(Fq, mVUinfo.readQ);
		recEndOaknutEmit();
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId(32, 32, 0xf, false);

	if (_XYZW_SS || _X_Y_Z_W == 0xf)
	{
		if (_XYZW_SS2)
		{
			const int tempACC = mVU.regAlloc->allocRegId();
			recBeginOaknutEmit();
			oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
			mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, Fq);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFq);
			recBeginOaknutEmit();
			const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
			oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
			oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
			recEndOaknutEmit();
			mVU.regAlloc->clearNeededXmmId(tempACC);
		}
		else
		{
			recBeginOaknutEmit();
			if (_XYZW_SS)
				mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, Fq);
			else
				mVUUpperFmlsPs_oaknut(mVU, ACC, Fs, Fq);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFq);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperFmlsPs_oaknut(mVU, tempACC, Fs, Fq);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFq);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fq);
}

static void mVU_MSUBAq_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2 { mVU_MSUBAq_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MSUBAq");
		mVUlogACC();
		mVUlogQ();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MSUBA_lane_direct_emit_oaknut(microVU& mVU, int recPass, int lane)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtL = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtL, FtRaw, lane);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId(32, 32, 0xf, false);

	if (_XYZW_SS || _X_Y_Z_W == 0xf)
	{
		if (_XYZW_SS2)
		{
			const int tempACC = mVU.regAlloc->allocRegId();
			recBeginOaknutEmit();
			oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
			mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, FtL);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtL);
			recBeginOaknutEmit();
			const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
			oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
			oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
			recEndOaknutEmit();
			mVU.regAlloc->clearNeededXmmId(tempACC);
		}
		else
		{
			recBeginOaknutEmit();
			if (_XYZW_SS)
				mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, FtL);
			else
				mVUUpperFmlsPs_oaknut(mVU, ACC, Fs, FtL);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtL);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperFmlsPs_oaknut(mVU, tempACC, Fs, FtL);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtL);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtL);
}

static void mVU_MSUBAx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MSUBA_lane_direct_emit_oaknut(mVU, recPass, 0); }
	pass3 { mVUlog("MSUBA"); mVUlogACC(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MSUBAy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { if (mVUNeedsVu0MicroAccExactPath(mVU, mVUExactVu0AccOp::MSub, mVUExactVu0FtMode::Y)) mVUExactVu0AccOp_emit_oaknut(mVU, recPass, mVUExactVu0AccOp::MSub, mVUExactVu0FtMode::Y); else mVU_MSUBA_lane_direct_emit_oaknut(mVU, recPass, 1); }
	pass3 { mVUlog("MSUBA"); mVUlogACC(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MSUBAz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { if (mVUNeedsVu0MicroAccExactPath(mVU, mVUExactVu0AccOp::MSub, mVUExactVu0FtMode::Z)) mVUExactVu0AccOp_emit_oaknut(mVU, recPass, mVUExactVu0AccOp::MSub, mVUExactVu0FtMode::Z); else mVU_MSUBA_lane_direct_emit_oaknut(mVU, recPass, 2); }
	pass3 { mVUlog("MSUBA"); mVUlogACC(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MSUBAw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { if (mVUNeedsVu0MicroAccExactPath(mVU, mVUExactVu0AccOp::MSub, mVUExactVu0FtMode::W)) mVUExactVu0AccOp_emit_oaknut(mVU, recPass, mVUExactVu0AccOp::MSub, mVUExactVu0FtMode::W); else mVU_MSUBA_lane_direct_emit_oaknut(mVU, recPass, 3); }
	pass3 { mVUlog("MSUBA"); mVUlogACC(); mVUlog(", vf%02dw", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MAX Opcode
static void mVU_MAX_direct_emit_oaknut(mP)
{
	int Ft;
	if (_XYZW_SS2)
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, _X_Y_Z_W);
	else if (clampE)
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, 0xf);
	else
		Ft = mVU.regAlloc->allocRegId(_Ft_);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	if (_XYZW_SS)
	{
		const int t1 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMaxScalar_oaknut(Fs, Ft, t1);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
	}
	else
	{
		const int t1 = mVU.regAlloc->allocRegId();
		const int t2 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMaxVector_oaknut(Fs, Ft, t1, t2);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
		mVU.regAlloc->clearNeededXmmId(t2);
	}

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Ft);
}

static void mVU_MAX_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MAX_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MAX");
		mVUlogFd();
		mVUlogFt();
	}
}

// MAXi Opcode
static void mVU_MAXi_direct_emit_oaknut(mP)
{
	const int Fi = mVU.regAlloc->allocRegId(33, 0, _X_Y_Z_W);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	if (_XYZW_SS)
	{
		const int t1 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMaxScalar_oaknut(Fs, Fi, t1);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
	}
	else
	{
		const int t1 = mVU.regAlloc->allocRegId();
		const int t2 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMaxVector_oaknut(Fs, Fi, t1, t2);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
		mVU.regAlloc->clearNeededXmmId(t2);
	}

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fi);
}

static void mVU_MAXi_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, 0);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MAXi_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MAXi");
		mVUlogFd();
		mVUlogI();
	}
}

static void mVU_MAX_lane_direct_emit_oaknut(microVU& mVU, int recPass, int lane)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtL = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtL, FtRaw, lane);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	if (_XYZW_SS)
	{
		const int t1 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMaxScalar_oaknut(Fs, FtL, t1);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
	}
	else
	{
		const int t1 = mVU.regAlloc->allocRegId();
		const int t2 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMaxVector_oaknut(Fs, FtL, t1, t2);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
		mVU.regAlloc->clearNeededXmmId(t2);
	}

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtL);
}

static void mVU_MAXx_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MAX_lane_direct_emit_oaknut(mVU, recPass, 0); }
	pass3 { mVUlog("MAX"); mVUlogFd(); mVUlog(", vf%02dx", _Ft_); }
}

static void mVU_MAXy_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MAX_lane_direct_emit_oaknut(mVU, recPass, 1); }
	pass3 { mVUlog("MAX"); mVUlogFd(); mVUlog(", vf%02dy", _Ft_); }
}

static void mVU_MAXz_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MAX_lane_direct_emit_oaknut(mVU, recPass, 2); }
	pass3 { mVUlog("MAX"); mVUlogFd(); mVUlog(", vf%02dz", _Ft_); }
}

static void mVU_MAXw_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MAX_lane_direct_emit_oaknut(mVU, recPass, 3); }
	pass3 { mVUlog("MAX"); mVUlogFd(); mVUlog(", vf%02dw", _Ft_); }
}

static void mVU_MINI_lane_direct_emit_oaknut(microVU& mVU, int recPass, int lane)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtL = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtL, FtRaw, lane);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	if (_XYZW_SS)
	{
		const int t1 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMiniScalar_oaknut(Fs, FtL, t1);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
	}
	else
	{
		const int t1 = mVU.regAlloc->allocRegId();
		const int t2 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMiniVector_oaknut(Fs, FtL, t1, t2);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
		mVU.regAlloc->clearNeededXmmId(t2);
	}

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtL);
}

static void mVU_MINIw_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MINI_lane_direct_emit_oaknut(mVU, recPass, 3); }
	pass3 { mVUlog("MINI"); mVUlogFd(); mVUlog(", vf%02dw", _Ft_); }
}

// MINI Opcode
static void mVU_MINI_direct_emit_oaknut(mP)
{
	int Ft;
	if (_XYZW_SS2)
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, _X_Y_Z_W);
	else if (clampE)
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, 0xf);
	else
		Ft = mVU.regAlloc->allocRegId(_Ft_);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	if (_XYZW_SS)
	{
		const int t1 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMiniScalar_oaknut(Fs, Ft, t1);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
	}
	else
	{
		const int t1 = mVU.regAlloc->allocRegId();
		const int t2 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMiniVector_oaknut(Fs, Ft, t1, t2);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
		mVU.regAlloc->clearNeededXmmId(t2);
	}

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Ft);
}

static void mVU_MINI_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MINI_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MINI");
		mVUlogFd();
		mVUlogFt();
	}
}

// MINIi Opcode
static void mVU_MINIi_direct_emit_oaknut(mP)
{
	const int Fi = mVU.regAlloc->allocRegId(33, 0, _X_Y_Z_W);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	if (_XYZW_SS)
	{
		const int t1 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMiniScalar_oaknut(Fs, Fi, t1);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
	}
	else
	{
		const int t1 = mVU.regAlloc->allocRegId();
		const int t2 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMiniVector_oaknut(Fs, Fi, t1, t2);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
		mVU.regAlloc->clearNeededXmmId(t2);
	}

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fi);
}

static void mVU_MINIi_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, 0);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MINIi_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MINIi");
		mVUlogFd();
		mVUlogI();
	}
}

static void mVU_MINIx_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MINI_lane_direct_emit_oaknut(mVU, recPass, 0); }
	pass3 { mVUlog("MINI"); mVUlogFd(); mVUlog(", vf%02dx", _Ft_); }
}

static void mVU_MINIy_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MINI_lane_direct_emit_oaknut(mVU, recPass, 1); }
	pass3 { mVUlog("MINI"); mVUlogFd(); mVUlog(", vf%02dy", _Ft_); }
}

static void mVU_MINIz_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MINI_lane_direct_emit_oaknut(mVU, recPass, 2); }
	pass3 { mVUlog("MINI"); mVUlogFd(); mVUlog(", vf%02dz", _Ft_); }
}

// ADDi Opcode
static void mVU_ADDi_triace_hack_oaknut(int to, int from)
{
	const oak::QReg to_q = oakQRegister(to);
	const oak::QReg from_q = oakQRegister(from);
	const oak::WReg to_bits = OAK_WSCRATCH;
	const oak::WReg from_bits = OAK_WSCRATCH2;
	oak::Label case_neg_big;
	oak::Label case_end1;
	oak::Label case_end2;

	oakAsm->FMOV(to_bits, oakSRegister(to));
	oakAsm->FMOV(from_bits, oakSRegister(from));
	oakAsm->LSR(to_bits, to_bits, 23);
	oakAsm->LSR(from_bits, from_bits, 23);
	oakAsm->AND(to_bits, to_bits, 0xff);
	oakAsm->AND(from_bits, from_bits, 0xff);
	oakAsm->SUB(from_bits, from_bits, to_bits);

	oakAsm->CMN(from_bits, 25);
	oakAsm->B(oak::Cond::LE, case_neg_big);
	oakAsm->CMP(from_bits, 25);
	oakAsm->B(oak::Cond::LT, case_end1);

	oakLoad128(OAK_QSCRATCH3, mVUUpperOakSs4Mem(offsetof(mVU_SSE4, sseMasks.ADD_SS)));
	oakAsm->AND(to_q.B16(), to_q.B16(), OAK_QSCRATCH3.B16());
	oakAsm->B(case_end2);

	oakAsm->l(case_neg_big);
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakSs4Mem(offsetof(mVU_SSE4, sseMasks.ADD_SS)));
	oakAsm->AND(from_q.B16(), from_q.B16(), OAK_QSCRATCH3.B16());

	oakAsm->l(case_end1);
	oakAsm->l(case_end2);
	oakAsm->FADD(oakSRegister(to), oakSRegister(to), oakSRegister(from));
}

static void mVU_ADDi_direct_emit_oaknut(mP)
{
	const int Fi = mVU.regAlloc->allocRegId(33, 0, _X_Y_Z_W);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
	{
		if (!CHECK_VUADDSUBHACK)
			mVUUpperAddSs_oaknut(mVU, Fs, Fi);
		else
			mVU_ADDi_triace_hack_oaknut(Fs, Fi);
	}
	else
	{
		mVUUpperAddPs_oaknut(mVU, Fs, Fi);
	}
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, Fi);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fi);
}

static void mVU_ADDi_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, 0); }
	pass2
	{
		mVU_ADDi_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("ADDi");
		mVUlogFd();
		mVUlogI();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// ADDq Opcode
static void mVU_ADDq_direct_emit_oaknut(mP)
{
	int Fq;
	int tempFq;
	if (!clampE && _XYZW_SS && !mVUinfo.readQ)
	{
		Fq = VU_HOST_XMMPQ;
		tempFq = VU_HOST_NO_XMM;
	}
	else
	{
		Fq = mVU.regAlloc->allocRegId();
		tempFq = Fq;
		recBeginOaknutEmit();
		mVUUpperGetQreg_oaknut(Fq, mVUinfo.readQ);
		recEndOaknutEmit();
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
		mVUUpperAddSs_oaknut(mVU, Fs, Fq);
	else
		mVUUpperAddPs_oaknut(mVU, Fs, Fq);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, tempFq);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fq);
}

static void mVU_ADDq_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, 0); }
	pass2
	{
		mVU_ADDq_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("ADDq");
		mVUlogFd();
		mVUlogQ();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_ADD_lane_direct_emit_oaknut(microVU& mVU, int recPass, int lane)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtL = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtL, FtRaw, lane);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);
	recBeginOaknutEmit();
	if (_XYZW_SS)
		mVUUpperAddSs_oaknut(mVU, Fs, FtL);
	else
		mVUUpperAddPs_oaknut(mVU, Fs, FtL);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, FtL);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtL);
}

static void mVU_ADDx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_ADD_lane_direct_emit_oaknut(mVU, recPass, 0); }
	pass3 { mVUlog("ADD"); mVUlogFd(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_ADDy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_ADD_lane_direct_emit_oaknut(mVU, recPass, 1); }
	pass3 { mVUlog("ADD"); mVUlogFd(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_ADDz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_ADD_lane_direct_emit_oaknut(mVU, recPass, 2); }
	pass3 { mVUlog("ADD"); mVUlogFd(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_ADDw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_ADD_lane_direct_emit_oaknut(mVU, recPass, 3); }
	pass3 { mVUlog("ADD"); mVUlogFd(); mVUlog(", vf%02dw", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// SUB Opcode
static void mVU_SUB_direct_emit_oaknut(mP)
{
	if (_Ft_ == _Fs_)
	{
		const int Fs = mVU.regAlloc->allocRegId(-1, _Fd_, _X_Y_Z_W);
		recBeginOaknutEmit();
		oakAsm->EOR(oakQRegister(Fs).B16(), oakQRegister(Fs).B16(), oakQRegister(Fs).B16());
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, Fs);
		mVU.regAlloc->clearNeededXmmId(Fs);
		return;
	}

	int Ft;
	int tempFt;
	const bool needsFullFtForClamp = clampE || _XYZW_PS;

	if (_XYZW_SS2)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, _X_Y_Z_W);
		tempFt = Ft;
	}
	else if (needsFullFtForClamp)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, 0xf);
		tempFt = Ft;
	}
	else
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_);
		tempFt = VU_HOST_NO_XMM;
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_PS)
	{
		if (_XYZW_SS)
		{
			mVUUpperClamp2Scalar_oaknut(mVU, Ft, false);
			mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		}
		else
		{
			mVUUpperClamp2Vector_oaknut(mVU, Ft, false);
			mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		}
	}
	if (_XYZW_SS)
		mVUUpperSubSs_oaknut(mVU, Fs, Ft);
	else
		mVUUpperSubPs_oaknut(mVU, Fs, Ft);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, tempFt);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Ft);
}

static void mVU_SUB_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		mVU_SUB_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("SUB");
		mVUlogFd();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// SUBi Opcode
static void mVU_SUBi_direct_emit_oaknut(mP)
{
	const int Fi = mVU.regAlloc->allocRegId(33, 0, _X_Y_Z_W);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_PS)
	{
		if (_XYZW_SS)
		{
			mVUUpperClamp2Scalar_oaknut(mVU, Fi, false);
			mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		}
		else
		{
			mVUUpperClamp2Vector_oaknut(mVU, Fi, false);
			mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		}
	}
	if (_XYZW_SS)
		mVUUpperSubSs_oaknut(mVU, Fs, Fi);
	else
		mVUUpperSubPs_oaknut(mVU, Fs, Fi);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, Fi);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fi);
}

static void mVU_SUBi_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, 0); }
	pass2
	{
		mVU_SUBi_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("SUBi");
		mVUlogFd();
		mVUlogI();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// SUBq Opcode
static void mVU_SUBq_direct_emit_oaknut(mP)
{
	int Fq;
	int tempFq;
	if (!clampE && _XYZW_SS && !mVUinfo.readQ)
	{
		Fq = VU_HOST_XMMPQ;
		tempFq = VU_HOST_NO_XMM;
	}
	else
	{
		Fq = mVU.regAlloc->allocRegId();
		tempFq = Fq;
		recBeginOaknutEmit();
		mVUUpperGetQreg_oaknut(Fq, mVUinfo.readQ);
		recEndOaknutEmit();
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_PS)
	{
		if (_XYZW_SS)
		{
			mVUUpperClamp2Scalar_oaknut(mVU, Fq, false);
			mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		}
		else
		{
			mVUUpperClamp2Vector_oaknut(mVU, Fq, false);
			mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		}
	}
	if (_XYZW_SS)
		mVUUpperSubSs_oaknut(mVU, Fs, Fq);
	else
		mVUUpperSubPs_oaknut(mVU, Fs, Fq);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, tempFq);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fq);
}

static void mVU_SUBq_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, 0); }
	pass2
	{
		mVU_SUBq_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("SUBq");
		mVUlogFd();
		mVUlogQ();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_SUB_lane_direct_emit_oaknut(microVU& mVU, int recPass, int lane)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtL = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtL, FtRaw, lane);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_PS)
	{
		if (_XYZW_SS)
		{
			mVUUpperClamp2Scalar_oaknut(mVU, FtL, false);
			mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		}
		else
		{
			mVUUpperClamp2Vector_oaknut(mVU, FtL, false);
			mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		}
	}
	if (_XYZW_SS)
		mVUUpperSubSs_oaknut(mVU, Fs, FtL);
	else
		mVUUpperSubPs_oaknut(mVU, Fs, FtL);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, FtL);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtL);
}

static void mVU_SUBx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_SUB_lane_direct_emit_oaknut(mVU, recPass, 0); }
	pass3 { mVUlog("SUB"); mVUlogFd(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_SUBy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_SUB_lane_direct_emit_oaknut(mVU, recPass, 1); }
	pass3 { mVUlog("SUB"); mVUlogFd(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_SUBz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_SUB_lane_direct_emit_oaknut(mVU, recPass, 2); }
	pass3 { mVUlog("SUB"); mVUlogFd(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_SUBw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_SUB_lane_direct_emit_oaknut(mVU, recPass, 3); }
	pass3 { mVUlog("SUB"); mVUlogFd(); mVUlog(", vf%02dw", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// SUBA Opcode
static void mVU_SUBA_direct_emit_oaknut(mP)
{
	if (_Ft_ == _Fs_)
	{
		const int Fs = mVU.regAlloc->allocRegId(-1, 32, _X_Y_Z_W);
		recBeginOaknutEmit();
		oakAsm->EOR(oakQRegister(Fs).B16(), oakQRegister(Fs).B16(), oakQRegister(Fs).B16());
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, Fs);
		mVU.regAlloc->clearNeededXmmId(Fs);
		return;
	}

	int Ft;
	int tempFt;
	if (_XYZW_SS2)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, _X_Y_Z_W);
		tempFt = Ft;
	}
	else if (clampE)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, 0xf);
		tempFt = Ft;
	}
	else
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_);
		tempFt = VU_HOST_NO_XMM;
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
		mVUUpperSubSs_oaknut(mVU, Fs, Ft);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFt);
		recBeginOaknutEmit();
		const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
		oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
		oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}
	else
	{
		recBeginOaknutEmit();
		if (_XYZW_SS)
		{
			mVUUpperSubSs_oaknut(mVU, Fs, Ft);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperSubPs_oaknut(mVU, Fs, Ft);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFt);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Ft);
}

static void mVU_SUBA_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_SUBA_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("SUBA");
		mVUlogACC();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// SUBAi Opcode
static void mVU_SUBAi_direct_emit_oaknut(mP)
{
	const int Fi = mVU.regAlloc->allocRegId(33, 0, _X_Y_Z_W);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
		mVUUpperSubSs_oaknut(mVU, Fs, Fi);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, Fi);
		recBeginOaknutEmit();
		const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
		oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
		oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}
	else
	{
		recBeginOaknutEmit();
		if (_XYZW_SS)
		{
			mVUUpperSubSs_oaknut(mVU, Fs, Fi);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperSubPs_oaknut(mVU, Fs, Fi);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, Fi);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fi);
}

static void mVU_SUBAi_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2 { mVU_SUBAi_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("SUBAi");
		mVUlogACC();
		mVUlogI();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// SUBAq Opcode
static void mVU_SUBAq_direct_emit_oaknut(mP)
{
	int Fq;
	int tempFq;
	if (!clampE && _XYZW_SS && !mVUinfo.readQ)
	{
		Fq = VU_HOST_XMMPQ;
		tempFq = VU_HOST_NO_XMM;
	}
	else
	{
		Fq = mVU.regAlloc->allocRegId();
		tempFq = Fq;
		recBeginOaknutEmit();
		mVUUpperGetQreg_oaknut(Fq, mVUinfo.readQ);
		recEndOaknutEmit();
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
		mVUUpperSubSs_oaknut(mVU, Fs, Fq);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFq);
		recBeginOaknutEmit();
		const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
		oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
		oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}
	else
	{
		recBeginOaknutEmit();
		if (_XYZW_SS)
		{
			mVUUpperSubSs_oaknut(mVU, Fs, Fq);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperSubPs_oaknut(mVU, Fs, Fq);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFq);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fq);
}

static void mVU_SUBAq_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2 { mVU_SUBAq_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("SUBAq");
		mVUlogACC();
		mVUlogQ();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_SUBA_lane_direct_emit_oaknut(microVU& mVU, int recPass, int lane)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtL = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtL, FtRaw, lane);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
		mVUUpperSubSs_oaknut(mVU, Fs, FtL);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtL);
		recBeginOaknutEmit();
		const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
		oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
		oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}
	else
	{
		recBeginOaknutEmit();
		if (_XYZW_SS)
		{
			mVUUpperSubSs_oaknut(mVU, Fs, FtL);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperSubPs_oaknut(mVU, Fs, FtL);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtL);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtL);
}

static void mVU_SUBAx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_SUBA_lane_direct_emit_oaknut(mVU, recPass, 0); }
	pass3 { mVUlog("SUBA"); mVUlogACC(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_SUBAy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_SUBA_lane_direct_emit_oaknut(mVU, recPass, 1); }
	pass3 { mVUlog("SUBA"); mVUlogACC(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_SUBAz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_SUBA_lane_direct_emit_oaknut(mVU, recPass, 2); }
	pass3 { mVUlog("SUBA"); mVUlogACC(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_SUBAw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_SUBA_lane_direct_emit_oaknut(mVU, recPass, 3); }
	pass3 { mVUlog("SUBA"); mVUlogACC(); mVUlog(", vf%02dw", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MUL Opcode
static void mVU_MUL_direct_emit_oaknut(mP)
{
	int Ft;
	int tempFt;
	const bool clampFt = _XYZW_PS;
	const bool willClampFt = clampE || (clampFt && !clampE && (CHECK_VU_OVERFLOW(mVU.index) || CHECK_VU_SIGN_OVERFLOW(mVU.index)));

	if (_XYZW_SS2)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, _X_Y_Z_W);
		tempFt = Ft;
	}
	else if (willClampFt)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, 0xf);
		tempFt = Ft;
	}
	else
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_);
		tempFt = VU_HOST_NO_XMM;
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
	{
		if (clampFt)
			mVUUpperClamp2Scalar_oaknut(mVU, Ft, false);
		mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		mVUUpperMulSs_oaknut(mVU, Fs, Ft);
	}
	else
	{
		if (clampFt)
			mVUUpperClamp2Vector_oaknut(mVU, Ft, false);
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		mVUUpperMulPs_oaknut(mVU, Fs, Ft);
	}
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, tempFt);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Ft);
}

static void mVU_MUL_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		mVU_MUL_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("MUL");
		mVUlogFd();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MULA Opcode
static void mVU_MULA_direct_emit_oaknut(mP)
{
	int Ft;
	int tempFt;
	if (_XYZW_SS2)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, _X_Y_Z_W);
		tempFt = Ft;
	}
	else if (clampE)
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_, 0, 0xf);
		tempFt = Ft;
	}
	else
	{
		Ft = mVU.regAlloc->allocRegId(_Ft_);
		tempFt = VU_HOST_NO_XMM;
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperMulSs_oaknut(mVU, Fs, Ft);
		oakAsm->MOV(oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))], oakQRegister(Fs).Selem()[0]);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
		oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFt);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}
	else
	{
		recBeginOaknutEmit();
		if (_XYZW_SS)
		{
			mVUUpperMulSs_oaknut(mVU, Fs, Ft);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperMulPs_oaknut(mVU, Fs, Ft);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFt);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Ft);
}

static void mVU_MULA_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MULA_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MULA");
		mVUlogACC();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MULAi Opcode
static void mVU_MULAi_direct_emit_oaknut(mP)
{
	const int Fi = mVU.regAlloc->allocRegId(33, 0, _X_Y_Z_W);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperMulSs_oaknut(mVU, Fs, Fi);
		oakAsm->MOV(oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))], oakQRegister(Fs).Selem()[0]);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
		oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, Fi);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}
	else
	{
		recBeginOaknutEmit();
		if (_XYZW_SS)
		{
			mVUUpperMulSs_oaknut(mVU, Fs, Fi);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperMulPs_oaknut(mVU, Fs, Fi);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, Fi);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fi);
}

static void mVU_MULAi_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2 { mVU_MULAi_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MULAi");
		mVUlogACC();
		mVUlogI();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MULAq Opcode
static void mVU_MULAq_direct_emit_oaknut(mP)
{
	int Fq;
	int tempFq;
	if (!clampE && _XYZW_SS && !mVUinfo.readQ)
	{
		Fq = VU_HOST_XMMPQ;
		tempFq = VU_HOST_NO_XMM;
	}
	else
	{
		Fq = mVU.regAlloc->allocRegId();
		tempFq = Fq;
		recBeginOaknutEmit();
		mVUUpperGetQreg_oaknut(Fq, mVUinfo.readQ);
		recEndOaknutEmit();
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperMulSs_oaknut(mVU, Fs, Fq);
		oakAsm->MOV(oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))], oakQRegister(Fs).Selem()[0]);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
		oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFq);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}
	else
	{
		recBeginOaknutEmit();
		if (_XYZW_SS)
		{
			mVUUpperMulSs_oaknut(mVU, Fs, Fq);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperMulPs_oaknut(mVU, Fs, Fq);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, tempFq);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fq);
}

static void mVU_MULAq_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2 { mVU_MULAq_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("MULAq");
		mVUlogACC();
		mVUlogQ();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MULA lane Opcodes
static void mVU_MULA_lane_direct_emit_oaknut(microVU& mVU, int recPass, int lane)
{
	const bool useFtLane = !clampE;
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	int FtL = VU_HOST_NO_XMM;
	if (!useFtLane)
	{
		FtL = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperDupLane_oaknut(FtL, FtRaw, lane);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(FtRaw);
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		if (useFtLane)
			mVUUpperMulSsLane_oaknut(Fs, FtRaw, lane);
		else
			mVUUpperMulSs_oaknut(mVU, Fs, FtL);
		oakAsm->MOV(oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))], oakQRegister(Fs).Selem()[0]);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(tempACC).Selem()[(_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)))]);
		oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, useFtLane ? VU_HOST_NO_XMM : FtL);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}
	else
	{
		recBeginOaknutEmit();
		if (_XYZW_SS)
			mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		else
			mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		if (_XYZW_SS)
		{
			if (useFtLane)
				mVUUpperMulSsLane_oaknut(Fs, FtRaw, lane);
			else
				mVUUpperMulSs_oaknut(mVU, Fs, FtL);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			if (useFtLane)
				mVUUpperMulPsLane_oaknut(Fs, FtRaw, lane);
			else
				mVUUpperMulPs_oaknut(mVU, Fs, FtL);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, useFtLane ? VU_HOST_NO_XMM : FtL);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	if (useFtLane)
		mVU.regAlloc->clearNeededXmmId(FtRaw);
	else
		mVU.regAlloc->clearNeededXmmId(FtL);
}

static void mVU_MULAx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MULA_lane_direct_emit_oaknut(mVU, recPass, 0); }
	pass3 { mVUlog("MULA"); mVUlogACC(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MULAy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MULA_lane_direct_emit_oaknut(mVU, recPass, 1); }
	pass3 { mVUlog("MULA"); mVUlogACC(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MULAz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MULA_lane_direct_emit_oaknut(mVU, recPass, 2); }
	pass3 { mVUlog("MULA"); mVUlogACC(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MULAw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MULA_lane_direct_emit_oaknut(mVU, recPass, 3); }
	pass3 { mVUlog("MULA"); mVUlogACC(); mVUlog(", vf%02dw", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MULi Opcode
static void mVU_MULi_direct_emit_oaknut(mP)
{
	const int Fi = mVU.regAlloc->allocRegId(33, 0, _X_Y_Z_W);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
	{
		if (_XYZW_PS)
			mVUUpperClamp2Scalar_oaknut(mVU, Fi, false);
		mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		mVUUpperMulSs_oaknut(mVU, Fs, Fi);
	}
	else
	{
		if (_XYZW_PS)
			mVUUpperClamp2Vector_oaknut(mVU, Fi, false);
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		mVUUpperMulPs_oaknut(mVU, Fs, Fi);
	}
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, Fi);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fi);
}

static void mVU_MULi_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, 0); }
	pass2
	{
		mVU_MULi_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("MULi");
		mVUlogFd();
		mVUlogI();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MULq Opcode
static void mVU_MULq_direct_emit_oaknut(mP)
{
	const bool useQLane = !clampE && !_XYZW_PS;
	int Fq = VU_HOST_NO_XMM;
	int tempFq = VU_HOST_NO_XMM;
	if (!useQLane && !clampE && _XYZW_SS && !mVUinfo.readQ)
	{
		Fq = VU_HOST_XMMPQ;
		tempFq = VU_HOST_NO_XMM;
	}
	else if (!useQLane)
	{
		Fq = mVU.regAlloc->allocRegId();
		tempFq = Fq;
		recBeginOaknutEmit();
		mVUUpperGetQreg_oaknut(Fq, mVUinfo.readQ);
		recEndOaknutEmit();
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
	{
		if (_XYZW_PS && !useQLane)
			mVUUpperClamp2Scalar_oaknut(mVU, Fq, false);
		mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		if (useQLane)
			oakAsm->FMUL(oakSRegister(Fs), oakSRegister(Fs), oakQRegister(VU_HOST_XMMPQ).Selem()[mVUinfo.readQ]);
		else
			mVUUpperMulSs_oaknut(mVU, Fs, Fq);
	}
	else
	{
		if (_XYZW_PS && !useQLane)
			mVUUpperClamp2Vector_oaknut(mVU, Fq, false);
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		if (useQLane)
			oakAsm->FMUL(oakQRegister(Fs).S4(), oakQRegister(Fs).S4(), oakQRegister(VU_HOST_XMMPQ).Selem()[mVUinfo.readQ]);
		else
			mVUUpperMulPs_oaknut(mVU, Fs, Fq);
	}
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, useQLane ? VU_HOST_NO_XMM : tempFq);

	mVU.regAlloc->clearNeededXmmId(Fs);
	if (Fq != VU_HOST_NO_XMM)
		mVU.regAlloc->clearNeededXmmId(Fq);
}

static void mVU_MULq_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, 0); }
	pass2
	{
		mVU_MULq_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("MULq");
		mVUlogFd();
		mVUlogQ();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MUL lane Opcodes
static void mVU_MUL_lane_direct_emit_oaknut(microVU& mVU, int recPass, int lane)
{
	const bool useFtLane = !clampE;
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	int FtL = VU_HOST_NO_XMM;
	if (!useFtLane)
	{
		FtL = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperDupLane_oaknut(FtL, FtRaw, lane);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(FtRaw);
	}

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
	{
		if (!useFtLane) {
			if (_XYZW_PS)
				mVUUpperClamp2Scalar_oaknut(mVU, FtL, false);
		}
		mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		if (useFtLane)
			mVUUpperMulSsLane_oaknut(Fs, FtRaw, lane);
		else
			mVUUpperMulSs_oaknut(mVU, Fs, FtL);
	}
	else
	{
		if (!useFtLane) {
			if (_XYZW_PS)
				mVUUpperClamp2Vector_oaknut(mVU, FtL, false);
		}
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		if (useFtLane)
			mVUUpperMulPsLane_oaknut(Fs, FtRaw, lane);
		else
			mVUUpperMulPs_oaknut(mVU, Fs, FtL);
	}
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, useFtLane ? VU_HOST_NO_XMM : FtL);

	mVU.regAlloc->clearNeededXmmId(Fs);
	if (useFtLane)
		mVU.regAlloc->clearNeededXmmId(FtRaw);
	else
		mVU.regAlloc->clearNeededXmmId(FtL);
}

static void mVU_MULx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MUL_lane_direct_emit_oaknut(mVU, recPass, 0); }
	pass3 { mVUlog("MUL"); mVUlogFd(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MULy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MUL_lane_direct_emit_oaknut(mVU, recPass, 1); }
	pass3 { mVUlog("MUL"); mVUlogFd(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MULz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MUL_lane_direct_emit_oaknut(mVU, recPass, 2); }
	pass3 { mVUlog("MUL"); mVUlogFd(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

static void mVU_MULw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MUL_lane_direct_emit_oaknut(mVU, recPass, 3); }
	pass3 { mVUlog("MUL"); mVUlogFd(); mVUlog(", vf%02dw", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// ABS Opcode
static void mVU_ABS_direct_emit_oaknut(mP)
{
	if (!_Ft_)
		return;
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Ft_, _X_Y_Z_W, !((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));
	recBeginOaknutEmit();
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakGlobMem(offsetof(mVU_Globals, absclip)));
	oakAsm->AND(oakQRegister(Fs).B16(), oakQRegister(Fs).B16(), OAK_QSCRATCH3.B16());
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(Fs);
}

static void mVU_ABS_emit(mP)
{
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2
	{
		mVU_ABS_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("ABS");
		mVUlogFtFs();
	}
}

// CLIP Opcode
static void mVU_CLIP_pblendw55_oaknut(const oak::QReg& dst, const oak::QReg& src)
{
	oakAsm->MOV(OAK_QSCRATCH.B16(), dst.B16());
	oakAsm->MOV(OAK_WSCRATCH, 0xffff0000);
	oakAsm->DUP(dst.S4(), OAK_WSCRATCH);
	oakAsm->BSL(dst.B16(), OAK_QSCRATCH.B16(), src.B16());
}

static void mVU_CLIP_packsswb_self_oaknut(const oak::QReg& reg)
{
	oakAsm->MOV(OAK_QSCRATCH.B16(), reg.B16());
	oakAsm->SQXTN(oakDRegister(reg.index()).B8(), OAK_QSCRATCH.H8());
	oakAsm->SQXTN2(reg.B16(), OAK_QSCRATCH.H8());
}

static void mVU_CLIP_movemask6_oaknut(const oak::WReg& dst, const oak::QReg& src)
{
	oakAsm->UMOV(OAK_WSCRATCH2, src.Selem()[0]);
	oakAsm->LSR(dst, OAK_WSCRATCH2, 7);
	oakAsm->AND(dst, dst, 0x1);
	oakAsm->LSR(OAK_WSCRATCH, OAK_WSCRATCH2, 14);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x2);
	oakAsm->ORR(dst, dst, OAK_WSCRATCH);
	oakAsm->LSR(OAK_WSCRATCH, OAK_WSCRATCH2, 21);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x4);
	oakAsm->ORR(dst, dst, OAK_WSCRATCH);
	oakAsm->LSR(OAK_WSCRATCH, OAK_WSCRATCH2, 28);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x8);
	oakAsm->ORR(dst, dst, OAK_WSCRATCH);

	oakAsm->UMOV(OAK_WSCRATCH2, src.Selem()[1]);
	oakAsm->LSR(OAK_WSCRATCH, OAK_WSCRATCH2, 3);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x10);
	oakAsm->ORR(dst, dst, OAK_WSCRATCH);
	oakAsm->LSR(OAK_WSCRATCH, OAK_WSCRATCH2, 10);
	oakAsm->AND(OAK_WSCRATCH, OAK_WSCRATCH, 0x20);
	oakAsm->ORR(dst, dst, OAK_WSCRATCH);
}

static void mVU_CLIP_direct_emit_oaknut(mP)
{
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, 0xf);
	const int Ft = mVU.regAlloc->allocRegId(_Ft_, 0, 0x1);
	const int t1 = mVU.regAlloc->allocRegId();
	const int t2 = mVU.regAlloc->allocRegId();

	mVUallocCFLAGa(mVU, VU_HOST_T1, cFLAG.lastWrite);

	const oak::QReg fs_q = oakQRegister(Fs);
	const oak::QReg ft_q = oakQRegister(Ft);
	const oak::QReg t1_q = oakQRegister(t1);
	const oak::QReg t2_q = oakQRegister(t2);
	const oak::WReg clip_w = oakWRegister(VU_HOST_T1);
	const oak::WReg mask_w = oakWRegister(VU_HOST_T2);

	recBeginOaknutEmit();
	oakAsm->DUP(ft_q.S4(), ft_q.Selem()[0]);
	oakAsm->LSL(clip_w, clip_w, 6);

	oakLoad128(t1_q, mVUUpperOakGlobMem(offsetof(mVU_Globals, exponent)));
	oakAsm->AND(t1_q.B16(), t1_q.B16(), fs_q.B16());
	oakAsm->MOVI(t2_q.B16(), 0);
	oakAsm->CMEQ(t1_q.S4(), t1_q.S4(), t2_q.S4());
	oakAsm->BIC(t1_q.B16(), fs_q.B16(), t1_q.B16());
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakGlobMem(offsetof(mVU_Globals, absclip)));
	oakAsm->AND(ft_q.B16(), ft_q.B16(), OAK_QSCRATCH3.B16());
	oakLoad128(t2_q, mVUUpperOakGlobMem(offsetof(mVU_Globals, exponent)));
	oakAsm->AND(t2_q.B16(), t2_q.B16(), ft_q.B16());
	oakAsm->MOVI(OAK_QSCRATCH.B16(), 0);
	oakAsm->CMEQ(t2_q.S4(), t2_q.S4(), OAK_QSCRATCH.S4());
	oakAsm->MOV(OAK_WSCRATCH, 0x007fffff);
	oakAsm->DUP(OAK_QSCRATCH.S4(), OAK_WSCRATCH);
	oakAsm->BSL(t2_q.B16(), OAK_QSCRATCH.B16(), ft_q.B16());
	oakAsm->MOV(ft_q.B16(), t2_q.B16());

	oakLoad128(fs_q, mVUUpperOakGlobMem(offsetof(mVU_Globals, signbit)));
	oakAsm->EOR(fs_q.B16(), fs_q.B16(), t1_q.B16());
	oakAsm->CMGT(t1_q.S4(), t1_q.S4(), ft_q.S4());
	oakAsm->CMGT(fs_q.S4(), fs_q.S4(), ft_q.S4());
	mVU_CLIP_pblendw55_oaknut(fs_q, t1_q);
	mVU_CLIP_packsswb_self_oaknut(fs_q);
	mVU_CLIP_movemask6_oaknut(mask_w, fs_q);
	oakAsm->AND(mask_w, mask_w, 0x3f);
	oakAsm->AND(clip_w, clip_w, 0xffffff);
	oakAsm->ORR(clip_w, clip_w, mask_w);
	recEndOaknutEmit();

	mVUallocCFLAGb(mVU, VU_HOST_T1, cFLAG.write);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Ft);
	mVU.regAlloc->clearNeededXmmId(t1);
	mVU.regAlloc->clearNeededXmmId(t2);
}

static void mVU_CLIP_emit(mP)
{
	pass1 { mVUanalyzeFMAC4(mVU, _Fs_, _Ft_); }
	pass2
	{
		mVU_CLIP_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("CLIP");
		mVUlogCLIP();
	}
}

// OPMULA Opcode
static void mVU_OPMULA_direct_emit_oaknut(mP)
{
	const u32 original_code = mVU.code;
	mVU.code = (mVU.code & ~(0xfu << 21)) | (0xeu << 21);

	const int Ft = mVU.regAlloc->allocRegId(_Ft_, 0, _X_Y_Z_W);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 32, _X_Y_Z_W);

	recBeginOaknutEmit();
	mVUUpperPshufd_oaknut(Fs, Fs, 0xC9);
	mVUUpperPshufd_oaknut(Ft, Ft, 0xD2);
	mVUUpperMulPs_oaknut(mVU, Fs, Ft);
	recEndOaknutEmit();

	mVU.regAlloc->clearNeededXmmId(Ft);
	mVUupdateFlags_oaknut(mVU, Fs);
	mVU.regAlloc->clearNeededXmmId(Fs);

	mVU.code = original_code;
}

static void mVU_OPMULA_emit(mP)
{
	pass1
	{
		const u32 original_code = mVU.code;
		mVU.code = (mVU.code & ~(0xfu << 21)) | (0xeu << 21);
		mVUanalyzeFMAC1(mVU, 0, _Fs_, _Ft_);
		mVU.code = original_code;
	}
	pass2 { mVU_OPMULA_direct_emit_oaknut(mVU, recPass); }
	pass3
	{
		mVUlog("OPMULA");
		mVUlogACC();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}


// OPMSUB Opcode
static void mVU_OPMSUB_direct_emit_oaknut(mP)
{
	const u32 original_code = mVU.code;
	mVU.code = (mVU.code & ~(0xfu << 21)) | (0xeu << 21);

	const int Ft = mVU.regAlloc->allocRegId(_Ft_, 0, _X_Y_Z_W);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId(32, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	mVUUpperPshufd_oaknut(Fs, Fs, 0xC9);
	mVUUpperPshufd_oaknut(Ft, Ft, 0xD2);
	mVUUpperMulPs_oaknut(mVU, Fs, Ft);
	mVUUpperSubPs_oaknut(mVU, ACC, Fs);
	recEndOaknutEmit();

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Ft);
	mVUupdateFlags_oaknut(mVU, ACC);
	mVU.regAlloc->clearNeededXmmId(ACC);

	mVU.code = original_code;
}

static void mVU_OPMSUB_emit(mP)
{
	pass1
	{
		const u32 original_code = mVU.code;
		mVU.code = (mVU.code & ~(0xfu << 21)) | (0xeu << 21);
		mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, _Ft_);
		mVU.code = original_code;
	}
	pass2
	{
		mVU_OPMSUB_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("OPMSUB");
		mVUlogFd();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}


// FTOI0 Opcode
static void mVU_FTOI_saturate_oaknut(int Fs, int t1, int t2)
{
	const oak::QReg fs_q = oakQRegister(Fs);
	const oak::QReg t1_q = oakQRegister(t1);
	const oak::QReg t2_q = oakQRegister(t2);

	oakAsm->MOV(t1_q.B16(), fs_q.B16());
	oakAsm->MOV(t2_q.B16(), fs_q.B16());
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakGlobMem(offsetof(mVU_Globals, absclip)));
	oakAsm->AND(t1_q.B16(), t1_q.B16(), OAK_QSCRATCH3.B16());
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakGlobMem(offsetof(mVU_Globals, I32MAXF)));
	oakAsm->CMGT(t1_q.S4(), t1_q.S4(), OAK_QSCRATCH3.S4());
	oakAsm->MOVI(OAK_QSCRATCH.B16(), 0);
	oakAsm->CMGT(t2_q.S4(), OAK_QSCRATCH.S4(), t2_q.S4());
	oakLoad128(OAK_QSCRATCH, mVUUpperOakGlobMem(offsetof(mVU_Globals, absclip)));
	oakLoad128(OAK_QSCRATCH2, mVUUpperOakGlobMem(offsetof(mVU_Globals, signbit)));
	oakAsm->BSL(t2_q.B16(), OAK_QSCRATCH2.B16(), OAK_QSCRATCH.B16());
	oakAsm->FCVTZS(fs_q.S4(), fs_q.S4());
	oakAsm->BSL(t1_q.B16(), t2_q.B16(), fs_q.B16());
	oakAsm->MOV(fs_q.B16(), t1_q.B16());
}

static void mVU_FTOI0_direct_emit_oaknut(mP)
{
	if (!_Ft_)
		return;
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Ft_, _X_Y_Z_W, !((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));
	const int t1 = mVU.regAlloc->allocRegId();
	const int t2 = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVU_FTOI_saturate_oaknut(Fs, t1, t2);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(t1);
	mVU.regAlloc->clearNeededXmmId(t2);
}

static void mVU_FTOI0_emit(mP)
{
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2
	{
		mVU_FTOI0_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("FTOI0");
		mVUlogFtFs();
	}
}

// FTOI4 Opcode
static void mVU_FTOI4_direct_emit_oaknut(mP)
{
	if (!_Ft_)
		return;
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Ft_, _X_Y_Z_W, !((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));
	const int t1 = mVU.regAlloc->allocRegId();
	const int t2 = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakGlobMem(offsetof(mVU_Globals, FTOI_4)));
	oakAsm->FMUL(oakQRegister(Fs).S4(), oakQRegister(Fs).S4(), OAK_QSCRATCH3.S4());
	mVU_FTOI_saturate_oaknut(Fs, t1, t2);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(t1);
	mVU.regAlloc->clearNeededXmmId(t2);
}

static void mVU_FTOI4_emit(mP)
{
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2
	{
		mVU_FTOI4_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("FTOI4");
		mVUlogFtFs();
	}
}

// FTOI12 Opcode
static void mVU_FTOI12_direct_emit_oaknut(mP)
{
	if (!_Ft_)
		return;
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Ft_, _X_Y_Z_W, !((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));
	const int t1 = mVU.regAlloc->allocRegId();
	const int t2 = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakGlobMem(offsetof(mVU_Globals, FTOI_12)));
	oakAsm->FMUL(oakQRegister(Fs).S4(), oakQRegister(Fs).S4(), OAK_QSCRATCH3.S4());
	mVU_FTOI_saturate_oaknut(Fs, t1, t2);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(t1);
	mVU.regAlloc->clearNeededXmmId(t2);
}

static void mVU_FTOI12_emit(mP)
{
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2
	{
		mVU_FTOI12_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("FTOI12");
		mVUlogFtFs();
	}
}

// FTOI15 Opcode
static void mVU_FTOI15_direct_emit_oaknut(mP)
{
	if (!_Ft_)
		return;
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Ft_, _X_Y_Z_W, !((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));
	const int t1 = mVU.regAlloc->allocRegId();
	const int t2 = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakGlobMem(offsetof(mVU_Globals, FTOI_15)));
	oakAsm->FMUL(oakQRegister(Fs).S4(), oakQRegister(Fs).S4(), OAK_QSCRATCH3.S4());
	mVU_FTOI_saturate_oaknut(Fs, t1, t2);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(t1);
	mVU.regAlloc->clearNeededXmmId(t2);
}

static void mVU_FTOI15_emit(mP)
{
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2
	{
		mVU_FTOI15_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("FTOI15");
		mVUlogFtFs();
	}
}

// ITOF0 Opcode
static void mVU_ITOF0_direct_emit_oaknut(mP)
{
	if (!_Ft_)
		return;
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Ft_, _X_Y_Z_W, !((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));
	recBeginOaknutEmit();
	oakAsm->SCVTF(oakQRegister(Fs).S4(), oakQRegister(Fs).S4());
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(Fs);
}

static void mVU_ITOF0_emit(mP)
{
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2
	{
		mVU_ITOF0_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("ITOF0");
		mVUlogFtFs();
	}
}

// ITOF4 Opcode
static void mVU_ITOF4_direct_emit_oaknut(mP)
{
	if (!_Ft_)
		return;
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Ft_, _X_Y_Z_W, !((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));
	recBeginOaknutEmit();
	oakAsm->SCVTF(oakQRegister(Fs).S4(), oakQRegister(Fs).S4());
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakGlobMem(offsetof(mVU_Globals, ITOF_4)));
	oakAsm->FMUL(oakQRegister(Fs).S4(), oakQRegister(Fs).S4(), OAK_QSCRATCH3.S4());
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(Fs);
}

static void mVU_ITOF4_emit(mP)
{
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2
	{
		mVU_ITOF4_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("ITOF4");
		mVUlogFtFs();
	}
}

// ITOF12 Opcode
static void mVU_ITOF12_direct_emit_oaknut(mP)
{
	if (!_Ft_)
		return;
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Ft_, _X_Y_Z_W, !((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));
	recBeginOaknutEmit();
	oakAsm->SCVTF(oakQRegister(Fs).S4(), oakQRegister(Fs).S4());
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakGlobMem(offsetof(mVU_Globals, ITOF_12)));
	oakAsm->FMUL(oakQRegister(Fs).S4(), oakQRegister(Fs).S4(), OAK_QSCRATCH3.S4());
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(Fs);
}

static void mVU_ITOF12_emit(mP)
{
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2
	{
		mVU_ITOF12_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("ITOF12");
		mVUlogFtFs();
	}
}

// ITOF15 Opcode
static void mVU_ITOF15_direct_emit_oaknut(mP)
{
	if (!_Ft_)
		return;
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Ft_, _X_Y_Z_W, !((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));
	recBeginOaknutEmit();
	oakAsm->SCVTF(oakQRegister(Fs).S4(), oakQRegister(Fs).S4());
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakGlobMem(offsetof(mVU_Globals, ITOF_15)));
	oakAsm->FMUL(oakQRegister(Fs).S4(), oakQRegister(Fs).S4(), OAK_QSCRATCH3.S4());
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(Fs);
}

static void mVU_ITOF15_emit(mP)
{
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2
	{
		mVU_ITOF15_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("ITOF15");
		mVUlogFtFs();
	}
}

// NOP Opcode
static void mVU_NOP_emit(mP)
{
	pass3 { mVUlog("NOP"); }
}

//------------------------------------------------------------------
// Micro VU Micromode Upper instructions
//------------------------------------------------------------------
