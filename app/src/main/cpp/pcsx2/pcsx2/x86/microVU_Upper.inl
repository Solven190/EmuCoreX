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
	oakAsm->UMOV(dst, src.Selem()[0]);
	oakAsm->LSR(dst, dst, 31);
	oakAsm->UMOV(OAK_WSCRATCH, src.Selem()[1]);
	oakAsm->LSR(OAK_WSCRATCH, OAK_WSCRATCH, 31);
	oakAsm->BFI(dst, OAK_WSCRATCH, 1, 1);
	oakAsm->UMOV(OAK_WSCRATCH, src.Selem()[2]);
	oakAsm->LSR(OAK_WSCRATCH, OAK_WSCRATCH, 31);
	oakAsm->BFI(dst, OAK_WSCRATCH, 2, 1);
	oakAsm->UMOV(OAK_WSCRATCH, src.Selem()[3]);
	oakAsm->LSR(OAK_WSCRATCH, OAK_WSCRATCH, 31);
	oakAsm->BFI(dst, OAK_WSCRATCH, 3, 1);
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
}

static __fi void mVUUpperClamp4Vector_oaknut(mV, int reg)
{
	if (clampE && !CHECK_VU_SIGN_OVERFLOW(mVU.index) && mVU.regAlloc->checkVFClamp(reg))
		mVUUpperClamp1Vector_oaknut(mVU, reg, true);
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
}

static __fi void mVUUpperClamp4Scalar_oaknut(mV, int reg)
{
	if (clampE && !CHECK_VU_SIGN_OVERFLOW(mVU.index) && mVU.regAlloc->checkVFClamp(reg))
		mVUUpperClamp1Scalar_oaknut(mVU, reg, true);
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

static __fi void mVUUpperMaxScalar_oaknut(int dest, int src, int temp)
{
	const oak::QReg dest_q = oakQRegister(dest);
	const oak::QReg temp_q = oakQRegister(temp);
	mVUUpperShufPsImm0_oaknut(dest, src);
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakSs4Mem(offsetof(mVU_SSE4, sseMasks.MIN_MAX_1)));
	oakAsm->AND(dest_q.B16(), dest_q.B16(), OAK_QSCRATCH3.B16());
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakSs4Mem(offsetof(mVU_SSE4, sseMasks.MIN_MAX_2)));
	oakAsm->ORR(dest_q.B16(), dest_q.B16(), OAK_QSCRATCH3.B16());
	oakAsm->MOV(temp_q.Delem()[0], dest_q.Delem()[1]);
	oakAsm->MOV(temp_q.Delem()[1], dest_q.Delem()[1]);
	oakAsm->FMAXNM(dest_q.D2(), dest_q.D2(), temp_q.D2());
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

static __fi void mVUUpperMiniScalar_oaknut(int dest, int src, int temp)
{
	const oak::QReg dest_q = oakQRegister(dest);
	const oak::QReg temp_q = oakQRegister(temp);
	mVUUpperShufPsImm0_oaknut(dest, src);
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakSs4Mem(offsetof(mVU_SSE4, sseMasks.MIN_MAX_1)));
	oakAsm->AND(dest_q.B16(), dest_q.B16(), OAK_QSCRATCH3.B16());
	oakLoad128(OAK_QSCRATCH3, mVUUpperOakSs4Mem(offsetof(mVU_SSE4, sseMasks.MIN_MAX_2)));
	oakAsm->ORR(dest_q.B16(), dest_q.B16(), OAK_QSCRATCH3.B16());
	oakAsm->MOV(temp_q.Delem()[0], dest_q.Delem()[1]);
	oakAsm->MOV(temp_q.Delem()[1], dest_q.Delem()[1]);
	oakAsm->FMINNM(dest_q.D2(), dest_q.D2(), temp_q.D2());
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

	recBeginOaknutEmit();
	if (_XYZW_SS)
		mVUUpperAddSs_oaknut(mVU, Fs, Ft);
	else
		mVUUpperAddPs_oaknut(mVU, Fs, Ft);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, tempFt);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Ft);
	mVU.profiler.EmitOp(opADD);
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
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
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
	mVU.profiler.EmitOp(opADDA);
}

static void mVU_ADDA_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_ADDA_direct_emit_oaknut(mVU, recPass);
	}
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
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
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
	mVU.profiler.EmitOp(opADDAi);
}

static void mVU_ADDAi_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2
	{
		mVU_ADDAi_direct_emit_oaknut(mVU, recPass);
	}
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
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
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
	mVU.profiler.EmitOp(opADDAq);
}

static void mVU_ADDAq_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2
	{
		mVU_ADDAq_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("ADDAq");
		mVUlogACC();
		mVUlogQ();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// ADDAx Opcode
static void mVU_ADDAx_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtX = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtX, FtRaw, 0);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		mVUUpperAddSs_oaknut(mVU, Fs, FtX);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtX);
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
			mVUUpperAddSs_oaknut(mVU, Fs, FtX);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperAddPs_oaknut(mVU, Fs, FtX);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtX);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtX);
	mVU.profiler.EmitOp(opADDAx);
}

static void mVU_ADDAx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_ADDAx_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("ADDA"); mVUlogACC(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// ADDAy Opcode
static void mVU_ADDAy_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtY = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtY, FtRaw, 1);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		mVUUpperAddSs_oaknut(mVU, Fs, FtY);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtY);
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
			mVUUpperAddSs_oaknut(mVU, Fs, FtY);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperAddPs_oaknut(mVU, Fs, FtY);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtY);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtY);
	mVU.profiler.EmitOp(opADDAy);
}

static void mVU_ADDAy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_ADDAy_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("ADDA"); mVUlogACC(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// ADDAz Opcode
static void mVU_ADDAz_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtZ = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtZ, FtRaw, 2);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		mVUUpperAddSs_oaknut(mVU, Fs, FtZ);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtZ);
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
			mVUUpperAddSs_oaknut(mVU, Fs, FtZ);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperAddPs_oaknut(mVU, Fs, FtZ);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtZ);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtZ);
	mVU.profiler.EmitOp(opADDAz);
}

static void mVU_ADDAz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_ADDAz_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("ADDA"); mVUlogACC(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// ADDAw Opcode
static void mVU_ADDAw_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtW = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtW, FtRaw, 3);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		mVUUpperAddSs_oaknut(mVU, Fs, FtW);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtW);
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
			mVUUpperAddSs_oaknut(mVU, Fs, FtW);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperAddPs_oaknut(mVU, Fs, FtW);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtW);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtW);
	mVU.profiler.EmitOp(opADDAw);
}

static void mVU_ADDAw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_ADDAw_direct_emit_oaknut(mVU, recPass);
	}
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
		recBeginOaknutEmit();
		if (_XYZW_SS2)
			mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		if (_XYZW_SS)
			mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, Fi);
		else
			mVUUpperFmlaPs_oaknut(mVU, ACC, Fs, Fi);
		recEndOaknutEmit();

		mVUupdateFlags_oaknut(mVU, ACC, Fs, Fi);
		if (_XYZW_SS && _X_Y_Z_W != 8)
		{
			recBeginOaknutEmit();
			mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
			recEndOaknutEmit();
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
	mVU.profiler.EmitOp(opMADDAi);
}

static void mVU_MADDAi_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2 { mVU_MADDAi_direct_emit_oaknut(mVU, recPass); }
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
			mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
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
	mVU.profiler.EmitOp(opMADDA);
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
			mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
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
	mVU.profiler.EmitOp(opMADDAq);
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

// MADDAx Opcode
static void mVU_MADDAx_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtX = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtX, FtRaw, 0);
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
			mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
			mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
			mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, FtX);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtX);
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
				mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, FtX);
			else
				mVUUpperFmlaPs_oaknut(mVU, ACC, Fs, FtX);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtX);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperFmlaPs_oaknut(mVU, tempACC, Fs, FtX);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtX);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtX);
	mVU.profiler.EmitOp(opMADDAx);
}

static void mVU_MADDAx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MADDAx_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MADDA"); mVUlogACC(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MADDAy Opcode
static void mVU_MADDAy_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtY = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtY, FtRaw, 1);
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
			mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
			mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
			mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, FtY);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtY);
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
				mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, FtY);
			else
				mVUUpperFmlaPs_oaknut(mVU, ACC, Fs, FtY);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtY);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperFmlaPs_oaknut(mVU, tempACC, Fs, FtY);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtY);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtY);
	mVU.profiler.EmitOp(opMADDAy);
}

static void mVU_MADDAy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MADDAy_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MADDA"); mVUlogACC(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MADDAz Opcode
static void mVU_MADDAz_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtZ = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtZ, FtRaw, 2);
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
			mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
			mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
			mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, FtZ);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtZ);
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
				mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, FtZ);
			else
				mVUUpperFmlaPs_oaknut(mVU, ACC, Fs, FtZ);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtZ);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperFmlaPs_oaknut(mVU, tempACC, Fs, FtZ);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtZ);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtZ);
	mVU.profiler.EmitOp(opMADDAz);
}

static void mVU_MADDAz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MADDAz_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MADDA"); mVUlogACC(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MADDAw Opcode
static void mVU_MADDAw_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtW = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtW, FtRaw, 3);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId(32, 32, 0xf, false);

	if (_XYZW_SS || _X_Y_Z_W == 0xf)
	{
		if (_XYZW_SS2)
		{
			// scalar, але не .x: треба зберегти оригінал ACC,
			// бо forward-shuffle псує решту lanes
			const int tempACC = mVU.regAlloc->allocRegId();
			recBeginOaknutEmit();
			oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
			mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
			mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
			mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, FtW);
			recEndOaknutEmit();

			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtW);

			recBeginOaknutEmit();
			// Записуємо результат лише в target lane, решта з оригіналу tempACC
			const int laneIdx = (_X ? 0 : (_Y ? 1 : (_Z ? 2 : 3)));
			oakAsm->MOV(oakQRegister(tempACC).Selem()[laneIdx], oakQRegister(ACC).Selem()[0]);
			oakAsm->MOV(oakQRegister(ACC).B16(), oakQRegister(tempACC).B16());
			recEndOaknutEmit();
			mVU.regAlloc->clearNeededXmmId(tempACC);
		}
		else
		{
			// .x або full vector (.xyzw)
			recBeginOaknutEmit();
			if (_XYZW_SS)
				mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
			else
				mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
			if (_XYZW_SS)
				mVUUpperFmlaSs_oaknut(mVU, ACC, Fs, FtW);
			else
				mVUUpperFmlaPs_oaknut(mVU, ACC, Fs, FtW);
			recEndOaknutEmit();

			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtW);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperFmlaPs_oaknut(mVU, tempACC, Fs, FtW);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtW);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtW);
	mVU.profiler.EmitOp(opMADDAw);
}

static void mVU_MADDAw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MADDAw_direct_emit_oaknut(mVU, recPass); }
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
	mVU.profiler.EmitOp(opMADD);
}

static void mVU_MADD_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MADD_direct_emit_oaknut(mVU, recPass); }
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
	mVU.profiler.EmitOp(opMADDi);
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
	mVU.profiler.EmitOp(opMADDq);
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

// MADDx Opcode
static void mVU_MADDx_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtX = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtX, FtRaw, 0);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int ACC = mVU.regAlloc->allocRegId(32);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);
	const int tempFs = mVU.regAlloc->allocRegId();

	recBeginOaknutEmit();
	if (_XYZW_SS)
		mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
	else
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
	oakAsm->MOV(oakQRegister(tempFs).B16(), oakQRegister(Fs).B16());
	oakAsm->MOV(oakQRegister(Fs).B16(), oakQRegister(ACC).B16());
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(Fs, Fs, shuffleSS(_X_Y_Z_W));
	if (_XYZW_SS)
		mVUUpperFmlaSs_oaknut(mVU, Fs, tempFs, FtX);
	else
		mVUUpperFmlaPs_oaknut(mVU, Fs, tempFs, FtX);
	recEndOaknutEmit();

	mVU.regAlloc->clearNeededXmmId(tempFs);

	mVUupdateFlags_oaknut(mVU, Fs, FtX);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtX);
	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.profiler.EmitOp(opMADDx);
}

static void mVU_MADDx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MADDx_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MADD"); mVUlogFd(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MADDy Opcode
static void mVU_MADDy_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtY = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtY, FtRaw, 1);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int ACC = mVU.regAlloc->allocRegId(32);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);
	const int tempFs = mVU.regAlloc->allocRegId();

	recBeginOaknutEmit();
	oakAsm->MOV(oakQRegister(tempFs).B16(), oakQRegister(Fs).B16());
	oakAsm->MOV(oakQRegister(Fs).B16(), oakQRegister(ACC).B16());
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(Fs, Fs, shuffleSS(_X_Y_Z_W));
	if (_XYZW_SS)
		mVUUpperFmlaSs_oaknut(mVU, Fs, tempFs, FtY);
	else
		mVUUpperFmlaPs_oaknut(mVU, Fs, tempFs, FtY);
	recEndOaknutEmit();

	mVU.regAlloc->clearNeededXmmId(tempFs);

	mVUupdateFlags_oaknut(mVU, Fs, FtY);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtY);
	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.profiler.EmitOp(opMADDy);
}

static void mVU_MADDy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MADDy_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MADD"); mVUlogFd(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MADDz Opcode
static void mVU_MADDz_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtZ = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtZ, FtRaw, 2);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int ACC = mVU.regAlloc->allocRegId(32);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);
	const int tempFs = mVU.regAlloc->allocRegId();

	recBeginOaknutEmit();
	oakAsm->MOV(oakQRegister(tempFs).B16(), oakQRegister(Fs).B16());
	oakAsm->MOV(oakQRegister(Fs).B16(), oakQRegister(ACC).B16());
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(Fs, Fs, shuffleSS(_X_Y_Z_W));
	if (_XYZW_SS)
		mVUUpperFmlaSs_oaknut(mVU, Fs, tempFs, FtZ);
	else
		mVUUpperFmlaPs_oaknut(mVU, Fs, tempFs, FtZ);
	recEndOaknutEmit();

	mVU.regAlloc->clearNeededXmmId(tempFs);

	mVUupdateFlags_oaknut(mVU, Fs, FtZ);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtZ);
	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.profiler.EmitOp(opMADDz);
}

static void mVU_MADDz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MADDz_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MADD"); mVUlogFd(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MADDw Opcode
static void mVU_MADDw_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtW = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtW, FtRaw, 3);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int ACC = mVU.regAlloc->allocRegId(32);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);
	const int tempFs = mVU.regAlloc->allocRegId();

	recBeginOaknutEmit();
	oakAsm->MOV(oakQRegister(tempFs).B16(), oakQRegister(Fs).B16());
	oakAsm->MOV(oakQRegister(Fs).B16(), oakQRegister(ACC).B16());
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(Fs, Fs, shuffleSS(_X_Y_Z_W));
	if (_XYZW_SS)
		mVUUpperFmlaSs_oaknut(mVU, Fs, tempFs, FtW);
	else
		mVUUpperFmlaPs_oaknut(mVU, Fs, tempFs, FtW);
	recEndOaknutEmit();

	mVU.regAlloc->clearNeededXmmId(tempFs);

	mVUupdateFlags_oaknut(mVU, Fs, FtW);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtW);
	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.profiler.EmitOp(opMADDw);
}

static void mVU_MADDw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MADDw_direct_emit_oaknut(mVU, recPass); }
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
	mVU.profiler.EmitOp(opMSUB);
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
	mVU.profiler.EmitOp(opMSUBi);
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
	mVU.profiler.EmitOp(opMSUBq);
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

// MSUBx Opcode
static void mVU_MSUBx_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtX = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtX, FtRaw, 0);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int Fd = mVU.regAlloc->allocRegId(32, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
		mVUUpperFmlsSs_oaknut(mVU, Fd, Fs, FtX);
	else
		mVUUpperFmlsPs_oaknut(mVU, Fd, Fs, FtX);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fd, Fs, FtX);

	mVU.regAlloc->clearNeededXmmId(Fd);
	mVU.regAlloc->clearNeededXmmId(FtX);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.profiler.EmitOp(opMSUBx);
}

static void mVU_MSUBx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MSUBx_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MSUB"); mVUlogFd(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MSUBy Opcode
static void mVU_MSUBy_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtY = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtY, FtRaw, 1);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int Fd = mVU.regAlloc->allocRegId(32, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
		mVUUpperFmlsSs_oaknut(mVU, Fd, Fs, FtY);
	else
		mVUUpperFmlsPs_oaknut(mVU, Fd, Fs, FtY);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fd, Fs, FtY);

	mVU.regAlloc->clearNeededXmmId(Fd);
	mVU.regAlloc->clearNeededXmmId(FtY);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.profiler.EmitOp(opMSUBy);
}

static void mVU_MSUBy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MSUBy_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MSUB"); mVUlogFd(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MSUBz Opcode
static void mVU_MSUBz_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtZ = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtZ, FtRaw, 2);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int Fd = mVU.regAlloc->allocRegId(32, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
		mVUUpperFmlsSs_oaknut(mVU, Fd, Fs, FtZ);
	else
		mVUUpperFmlsPs_oaknut(mVU, Fd, Fs, FtZ);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fd, Fs, FtZ);

	mVU.regAlloc->clearNeededXmmId(Fd);
	mVU.regAlloc->clearNeededXmmId(FtZ);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.profiler.EmitOp(opMSUBz);
}

static void mVU_MSUBz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MSUBz_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MSUB"); mVUlogFd(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MSUBw Opcode
static void mVU_MSUBw_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtW = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtW, FtRaw, 3);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int Fd = mVU.regAlloc->allocRegId(32, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
		mVUUpperFmlsSs_oaknut(mVU, Fd, Fs, FtW);
	else
		mVUUpperFmlsPs_oaknut(mVU, Fd, Fs, FtW);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fd, Fs, FtW);

	mVU.regAlloc->clearNeededXmmId(Fd);
	mVU.regAlloc->clearNeededXmmId(FtW);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.profiler.EmitOp(opMSUBw);
}

static void mVU_MSUBw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2 { mVU_MSUBw_direct_emit_oaknut(mVU, recPass); }
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
			mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
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
	mVU.profiler.EmitOp(opMSUBA);
}

static void mVU_MSUBA_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MSUBA_direct_emit_oaknut(mVU, recPass); }
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
			mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
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
	mVU.profiler.EmitOp(opMSUBAi);
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
			mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
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
	mVU.profiler.EmitOp(opMSUBAq);
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

// MSUBAx Opcode
static void mVU_MSUBAx_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtX = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtX, FtRaw, 0);
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
			mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
			mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, FtX);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtX);
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
				mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, FtX);
			else
				mVUUpperFmlsPs_oaknut(mVU, ACC, Fs, FtX);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtX);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperFmlsPs_oaknut(mVU, tempACC, Fs, FtX);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtX);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtX);
	mVU.profiler.EmitOp(opMSUBAx);
}

static void mVU_MSUBAx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MSUBAx_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MSUBA"); mVUlogACC(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MSUBAy Opcode
static void mVU_MSUBAy_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtY = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtY, FtRaw, 1);
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
			mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
			mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, FtY);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtY);
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
				mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, FtY);
			else
				mVUUpperFmlsPs_oaknut(mVU, ACC, Fs, FtY);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtY);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperFmlsPs_oaknut(mVU, tempACC, Fs, FtY);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtY);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtY);
	mVU.profiler.EmitOp(opMSUBAy);
}

static void mVU_MSUBAy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MSUBAy_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MSUBA"); mVUlogACC(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MSUBAz Opcode
static void mVU_MSUBAz_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtZ = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtZ, FtRaw, 2);
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
			mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
			mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, FtZ);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtZ);
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
				mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, FtZ);
			else
				mVUUpperFmlsPs_oaknut(mVU, ACC, Fs, FtZ);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtZ);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperFmlsPs_oaknut(mVU, tempACC, Fs, FtZ);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtZ);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtZ);
	mVU.profiler.EmitOp(opMSUBAz);
}

static void mVU_MSUBAz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MSUBAz_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MSUBA"); mVUlogACC(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MSUBAw Opcode
static void mVU_MSUBAw_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtW = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtW, FtRaw, 3);
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
			mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
			mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, FtW);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtW);
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
				mVUUpperFmlsSs_oaknut(mVU, ACC, Fs, FtW);
			else
				mVUUpperFmlsPs_oaknut(mVU, ACC, Fs, FtW);
			recEndOaknutEmit();
			mVUupdateFlags_oaknut(mVU, ACC, Fs, FtW);
		}
	}
	else
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperFmlsPs_oaknut(mVU, tempACC, Fs, FtW);
		mVUUpperMergeRegs_oaknut(ACC, tempACC, _X_Y_Z_W);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtW);
		mVU.regAlloc->clearNeededXmmId(tempACC);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtW);
	mVU.profiler.EmitOp(opMSUBAw);
}

static void mVU_MSUBAw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2 { mVU_MSUBAw_direct_emit_oaknut(mVU, recPass); }
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
	mVU.profiler.EmitOp(opMAX);
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
	mVU.profiler.EmitOp(opMAXi);
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

// MAXx Opcode
static void mVU_MAXx_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtX = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtX, FtRaw, 0);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	if (_XYZW_SS)
	{
		const int t1 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMaxScalar_oaknut(Fs, FtX, t1);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
	}
	else
	{
		const int t1 = mVU.regAlloc->allocRegId();
		const int t2 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMaxVector_oaknut(Fs, FtX, t1, t2);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
		mVU.regAlloc->clearNeededXmmId(t2);
	}

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtX);
	mVU.profiler.EmitOp(opMAXx);
}

static void mVU_MAXx_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MAXx_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MAX"); mVUlogFd(); mVUlog(", vf%02dx", _Ft_); }
}

// MAXy Opcode
static void mVU_MAXy_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtY = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtY, FtRaw, 1);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	if (_XYZW_SS)
	{
		const int t1 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMaxScalar_oaknut(Fs, FtY, t1);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
	}
	else
	{
		const int t1 = mVU.regAlloc->allocRegId();
		const int t2 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMaxVector_oaknut(Fs, FtY, t1, t2);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
		mVU.regAlloc->clearNeededXmmId(t2);
	}

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtY);
	mVU.profiler.EmitOp(opMAXy);
}

static void mVU_MAXy_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MAXy_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MAX"); mVUlogFd(); mVUlog(", vf%02dy", _Ft_); }
}

// MAXz Opcode
static void mVU_MAXz_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtZ = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtZ, FtRaw, 2);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	if (_XYZW_SS)
	{
		const int t1 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMaxScalar_oaknut(Fs, FtZ, t1);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
	}
	else
	{
		const int t1 = mVU.regAlloc->allocRegId();
		const int t2 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMaxVector_oaknut(Fs, FtZ, t1, t2);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
		mVU.regAlloc->clearNeededXmmId(t2);
	}

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtZ);
	mVU.profiler.EmitOp(opMAXz);
}

static void mVU_MAXz_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MAXz_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MAX"); mVUlogFd(); mVUlog(", vf%02dz", _Ft_); }
}

// MAXw Opcode
static void mVU_MAXw_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtW = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtW, FtRaw, 3);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	if (_XYZW_SS)
	{
		const int t1 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMaxScalar_oaknut(Fs, FtW, t1);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
	}
	else
	{
		const int t1 = mVU.regAlloc->allocRegId();
		const int t2 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMaxVector_oaknut(Fs, FtW, t1, t2);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
		mVU.regAlloc->clearNeededXmmId(t2);
	}

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtW);
	mVU.profiler.EmitOp(opMAXw);
}

static void mVU_MAXw_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MAXw_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MAX"); mVUlogFd(); mVUlog(", vf%02dw", _Ft_); }
}

// MINIw Opcode
static void mVU_MINIw_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtW = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtW, FtRaw, 3);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	if (_XYZW_SS)
	{
		const int t1 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMiniScalar_oaknut(Fs, FtW, t1);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
	}
	else
	{
		const int t1 = mVU.regAlloc->allocRegId();
		const int t2 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMiniVector_oaknut(Fs, FtW, t1, t2);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
		mVU.regAlloc->clearNeededXmmId(t2);
	}

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtW);
	mVU.profiler.EmitOp(opMINIw);
}

static void mVU_MINIw_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MINIw_direct_emit_oaknut(mVU, recPass); }
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
	mVU.profiler.EmitOp(opMINI);
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
	mVU.profiler.EmitOp(opMINIi);
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

// MINIx Opcode
static void mVU_MINIx_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtX = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtX, FtRaw, 0);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	if (_XYZW_SS)
	{
		const int t1 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMiniScalar_oaknut(Fs, FtX, t1);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
	}
	else
	{
		const int t1 = mVU.regAlloc->allocRegId();
		const int t2 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMiniVector_oaknut(Fs, FtX, t1, t2);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
		mVU.regAlloc->clearNeededXmmId(t2);
	}

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtX);
	mVU.profiler.EmitOp(opMINIx);
}

static void mVU_MINIx_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MINIx_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MINI"); mVUlogFd(); mVUlog(", vf%02dx", _Ft_); }
}

// MINIy Opcode
static void mVU_MINIy_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtY = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtY, FtRaw, 1);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	if (_XYZW_SS)
	{
		const int t1 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMiniScalar_oaknut(Fs, FtY, t1);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
	}
	else
	{
		const int t1 = mVU.regAlloc->allocRegId();
		const int t2 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMiniVector_oaknut(Fs, FtY, t1, t2);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
		mVU.regAlloc->clearNeededXmmId(t2);
	}

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtY);
	mVU.profiler.EmitOp(opMINIy);
}

static void mVU_MINIy_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MINIy_direct_emit_oaknut(mVU, recPass); }
	pass3 { mVUlog("MINI"); mVUlogFd(); mVUlog(", vf%02dy", _Ft_); }
}

// MINIz Opcode
static void mVU_MINIz_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtZ = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtZ, FtRaw, 2);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	if (_XYZW_SS)
	{
		const int t1 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMiniScalar_oaknut(Fs, FtZ, t1);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
	}
	else
	{
		const int t1 = mVU.regAlloc->allocRegId();
		const int t2 = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		mVUUpperMiniVector_oaknut(Fs, FtZ, t1, t2);
		recEndOaknutEmit();
		mVU.regAlloc->clearNeededXmmId(t1);
		mVU.regAlloc->clearNeededXmmId(t2);
	}

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtZ);
	mVU.profiler.EmitOp(opMINIz);
}

static void mVU_MINIz_emit(mP)
{
	pass1
	{
		mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_);
		sFLAG.doFlag = false;
	}
	pass2 { mVU_MINIz_direct_emit_oaknut(mVU, recPass); }
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
	mVU.profiler.EmitOp(opADDi);
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
	mVU.profiler.EmitOp(opADDq);
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

// ADDx Opcode
static void mVU_ADDx_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtX = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtX, FtRaw, 0);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);
	recBeginOaknutEmit();
	if (_XYZW_SS)
		mVUUpperAddSs_oaknut(mVU, Fs, FtX);
	else
		mVUUpperAddPs_oaknut(mVU, Fs, FtX);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, FtX);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtX);
	mVU.profiler.EmitOp(opADDx);
}

static void mVU_ADDx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		mVU_ADDx_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("ADD"); mVUlogFd(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// ADDy Opcode
static void mVU_ADDy_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtY = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtY, FtRaw, 1);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);
	recBeginOaknutEmit();
	if (_XYZW_SS)
		mVUUpperAddSs_oaknut(mVU, Fs, FtY);
	else
		mVUUpperAddPs_oaknut(mVU, Fs, FtY);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, FtY);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtY);
	mVU.profiler.EmitOp(opADDy);
}

static void mVU_ADDy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		mVU_ADDy_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("ADD"); mVUlogFd(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// ADDz Opcode
static void mVU_ADDz_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtZ = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtZ, FtRaw, 2);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);
	recBeginOaknutEmit();
	if (_XYZW_SS)
		mVUUpperAddSs_oaknut(mVU, Fs, FtZ);
	else
		mVUUpperAddPs_oaknut(mVU, Fs, FtZ);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, FtZ);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtZ);
	mVU.profiler.EmitOp(opADDz);
}

static void mVU_ADDz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		mVU_ADDz_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("ADD"); mVUlogFd(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// ADDw Opcode
static void mVU_ADDw_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtW = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtW, FtRaw, 3);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);
	recBeginOaknutEmit();
	if (_XYZW_SS)
		mVUUpperAddSs_oaknut(mVU, Fs, FtW);
	else
		mVUUpperAddPs_oaknut(mVU, Fs, FtW);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, FtW);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtW);
	mVU.profiler.EmitOp(opADDw);
}

static void mVU_ADDw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		mVU_ADDw_direct_emit_oaknut(mVU, recPass);
	}
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
	mVU.profiler.EmitOp(opSUB);
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
	mVU.profiler.EmitOp(opSUBi);
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
	mVU.profiler.EmitOp(opSUBq);
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

// SUBx Opcode
static void mVU_SUBx_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtX = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtX, FtRaw, 0);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_PS)
	{
		if (_XYZW_SS)
		{
			mVUUpperClamp2Scalar_oaknut(mVU, FtX, false);
			mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		}
		else
		{
			mVUUpperClamp2Vector_oaknut(mVU, FtX, false);
			mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		}
	}
	if (_XYZW_SS)
		mVUUpperSubSs_oaknut(mVU, Fs, FtX);
	else
		mVUUpperSubPs_oaknut(mVU, Fs, FtX);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, FtX);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtX);
	mVU.profiler.EmitOp(opSUBx);
}

static void mVU_SUBx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		mVU_SUBx_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("SUB"); mVUlogFd(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// SUBy Opcode
static void mVU_SUBy_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtY = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtY, FtRaw, 1);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_PS)
	{
		if (_XYZW_SS)
		{
			mVUUpperClamp2Scalar_oaknut(mVU, FtY, false);
			mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		}
		else
		{
			mVUUpperClamp2Vector_oaknut(mVU, FtY, false);
			mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		}
	}
	if (_XYZW_SS)
		mVUUpperSubSs_oaknut(mVU, Fs, FtY);
	else
		mVUUpperSubPs_oaknut(mVU, Fs, FtY);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, FtY);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtY);
	mVU.profiler.EmitOp(opSUBy);
}

static void mVU_SUBy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		mVU_SUBy_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("SUB"); mVUlogFd(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// SUBz Opcode
static void mVU_SUBz_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtZ = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtZ, FtRaw, 2);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_PS)
	{
		if (_XYZW_SS)
		{
			mVUUpperClamp2Scalar_oaknut(mVU, FtZ, false);
			mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		}
		else
		{
			mVUUpperClamp2Vector_oaknut(mVU, FtZ, false);
			mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		}
	}
	if (_XYZW_SS)
		mVUUpperSubSs_oaknut(mVU, Fs, FtZ);
	else
		mVUUpperSubPs_oaknut(mVU, Fs, FtZ);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, FtZ);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtZ);
	mVU.profiler.EmitOp(opSUBz);
}

static void mVU_SUBz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		mVU_SUBz_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("SUB"); mVUlogFd(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// SUBw Opcode
static void mVU_SUBw_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtW = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtW, FtRaw, 3);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_PS)
	{
		if (_XYZW_SS)
		{
			mVUUpperClamp2Scalar_oaknut(mVU, FtW, false);
			mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		}
		else
		{
			mVUUpperClamp2Vector_oaknut(mVU, FtW, false);
			mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		}
	}
	if (_XYZW_SS)
		mVUUpperSubSs_oaknut(mVU, Fs, FtW);
	else
		mVUUpperSubPs_oaknut(mVU, Fs, FtW);
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, FtW);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtW);
	mVU.profiler.EmitOp(opSUBw);
}

static void mVU_SUBw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		mVU_SUBw_direct_emit_oaknut(mVU, recPass);
	}
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
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
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
	mVU.profiler.EmitOp(opSUBA);
}

static void mVU_SUBA_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_SUBA_direct_emit_oaknut(mVU, recPass);
	}
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
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
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
	mVU.profiler.EmitOp(opSUBAi);
}

static void mVU_SUBAi_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2
	{
		mVU_SUBAi_direct_emit_oaknut(mVU, recPass);
	}
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
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
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
	mVU.profiler.EmitOp(opSUBAq);
}

static void mVU_SUBAq_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2
	{
		mVU_SUBAq_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("SUBAq");
		mVUlogACC();
		mVUlogQ();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// SUBAx Opcode
static void mVU_SUBAx_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtX = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtX, FtRaw, 0);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		mVUUpperSubSs_oaknut(mVU, Fs, FtX);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtX);
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
			mVUUpperSubSs_oaknut(mVU, Fs, FtX);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperSubPs_oaknut(mVU, Fs, FtX);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtX);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtX);
	mVU.profiler.EmitOp(opSUBAx);
}

static void mVU_SUBAx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_SUBAx_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("SUBA"); mVUlogACC(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// SUBAy Opcode
static void mVU_SUBAy_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtY = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtY, FtRaw, 1);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		mVUUpperSubSs_oaknut(mVU, Fs, FtY);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtY);
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
			mVUUpperSubSs_oaknut(mVU, Fs, FtY);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperSubPs_oaknut(mVU, Fs, FtY);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtY);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtY);
	mVU.profiler.EmitOp(opSUBAy);
}

static void mVU_SUBAy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_SUBAy_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("SUBA"); mVUlogACC(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// SUBAz Opcode
static void mVU_SUBAz_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtZ = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtZ, FtRaw, 2);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		mVUUpperSubSs_oaknut(mVU, Fs, FtZ);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtZ);
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
			mVUUpperSubSs_oaknut(mVU, Fs, FtZ);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperSubPs_oaknut(mVU, Fs, FtZ);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtZ);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtZ);
	mVU.profiler.EmitOp(opSUBAz);
}

static void mVU_SUBAz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_SUBAz_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("SUBA"); mVUlogACC(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// SUBAw Opcode
static void mVU_SUBAw_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtW = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtW, FtRaw, 3);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	if (_XYZW_SS2)
	{
		const int tempACC = mVU.regAlloc->allocRegId();
		recBeginOaknutEmit();
		oakAsm->MOV(oakQRegister(tempACC).B16(), oakQRegister(ACC).B16());
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		mVUUpperSubSs_oaknut(mVU, Fs, FtW);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtW);
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
			mVUUpperSubSs_oaknut(mVU, Fs, FtW);
			oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
		}
		else
		{
			mVUUpperSubPs_oaknut(mVU, Fs, FtW);
			mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
		}
		recEndOaknutEmit();
		mVUupdateFlags_oaknut(mVU, ACC, Fs, FtW);
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtW);
	mVU.profiler.EmitOp(opSUBAw);
}

static void mVU_SUBAw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_SUBAw_direct_emit_oaknut(mVU, recPass);
	}
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
	mVU.profiler.EmitOp(opMUL);
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

	recBeginOaknutEmit();
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));

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

	if (_XYZW_SS2)
	{
		recBeginOaknutEmit();
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		recEndOaknutEmit();
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Ft);
	mVU.profiler.EmitOp(opMULA);
}

static void mVU_MULA_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_MULA_direct_emit_oaknut(mVU, recPass);
	}
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

	recBeginOaknutEmit();
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));

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

	if (_XYZW_SS2)
	{
		recBeginOaknutEmit();
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		recEndOaknutEmit();
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fi);
	mVU.profiler.EmitOp(opMULAi);
}

static void mVU_MULAi_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2
	{
		mVU_MULAi_direct_emit_oaknut(mVU, recPass);
	}
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

	recBeginOaknutEmit();
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));

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

	if (_XYZW_SS2)
	{
		recBeginOaknutEmit();
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		recEndOaknutEmit();
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fq);
	mVU.profiler.EmitOp(opMULAq);
}

static void mVU_MULAq_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, 0); }
	pass2
	{
		mVU_MULAq_direct_emit_oaknut(mVU, recPass);
	}
	pass3
	{
		mVUlog("MULAq");
		mVUlogACC();
		mVUlogQ();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MULAx Opcode
static void mVU_MULAx_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtX = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtX, FtRaw, 0);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	recBeginOaknutEmit();
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));

	if (_XYZW_SS)
		mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
	else
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);

	if (_XYZW_SS)
	{
		mVUUpperMulSs_oaknut(mVU, Fs, FtX);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
	}
	else
	{
		mVUUpperMulPs_oaknut(mVU, Fs, FtX);
		mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
	}
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, ACC, Fs, FtX);

	if (_XYZW_SS2)
	{
		recBeginOaknutEmit();
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		recEndOaknutEmit();
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtX);
	mVU.profiler.EmitOp(opMULAx);
}

static void mVU_MULAx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_MULAx_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("MULA"); mVUlogACC(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MULAy Opcode
static void mVU_MULAy_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtY = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtY, FtRaw, 1);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	recBeginOaknutEmit();
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));

	if (_XYZW_SS)
		mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
	else
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);

	if (_XYZW_SS)
	{
		mVUUpperMulSs_oaknut(mVU, Fs, FtY);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
	}
	else
	{
		mVUUpperMulPs_oaknut(mVU, Fs, FtY);
		mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
	}
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, ACC, Fs, FtY);

	if (_XYZW_SS2)
	{
		recBeginOaknutEmit();
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		recEndOaknutEmit();
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtY);
	mVU.profiler.EmitOp(opMULAy);
}

static void mVU_MULAy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_MULAy_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("MULA"); mVUlogACC(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MULAz Opcode
static void mVU_MULAz_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtZ = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtZ, FtRaw, 2);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	recBeginOaknutEmit();
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));

	if (_XYZW_SS)
		mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
	else
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);

	if (_XYZW_SS)
	{
		mVUUpperMulSs_oaknut(mVU, Fs, FtZ);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
	}
	else
	{
		mVUUpperMulPs_oaknut(mVU, Fs, FtZ);
		mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
	}
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, ACC, Fs, FtZ);

	if (_XYZW_SS2)
	{
		recBeginOaknutEmit();
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		recEndOaknutEmit();
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtZ);
	mVU.profiler.EmitOp(opMULAz);
}

static void mVU_MULAz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_MULAz_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("MULA"); mVUlogACC(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MULAw Opcode
static void mVU_MULAw_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtW = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtW, FtRaw, 3);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, _X_Y_Z_W);
	const int ACC = mVU.regAlloc->allocRegId((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);

	recBeginOaknutEmit();
	if (_XYZW_SS2)
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));

	if (_XYZW_PS)
		mVUUpperClamp2Vector_oaknut(mVU, FtW, false);
	if (_XYZW_SS)
		mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
	else
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);

	if (_XYZW_SS)
	{
		mVUUpperMulSs_oaknut(mVU, Fs, FtW);
		oakAsm->MOV(oakQRegister(ACC).Selem()[0], oakQRegister(Fs).Selem()[0]);
	}
	else
	{
		mVUUpperMulPs_oaknut(mVU, Fs, FtW);
		mVUUpperMergeRegs_oaknut(ACC, Fs, _X_Y_Z_W);
	}
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, ACC, Fs, FtW);

	if (_XYZW_SS2)
	{
		recBeginOaknutEmit();
		mVUUpperPshufd_oaknut(ACC, ACC, shuffleSS(_X_Y_Z_W));
		recEndOaknutEmit();
	}

	mVU.regAlloc->clearNeededXmmId(ACC);
	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtW);
	mVU.profiler.EmitOp(opMULAw);
}

static void mVU_MULAw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_MULAw_direct_emit_oaknut(mVU, recPass);
	}
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
	mVU.profiler.EmitOp(opMULi);
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
	{
		if (_XYZW_PS)
			mVUUpperClamp2Scalar_oaknut(mVU, Fq, false);
		mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		mVUUpperMulSs_oaknut(mVU, Fs, Fq);
	}
	else
	{
		if (_XYZW_PS)
			mVUUpperClamp2Vector_oaknut(mVU, Fq, false);
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		mVUUpperMulPs_oaknut(mVU, Fs, Fq);
	}
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, tempFq);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(Fq);
	mVU.profiler.EmitOp(opMULq);
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

// MULx Opcode
static void mVU_MULx_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtX = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtX, FtRaw, 0);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
	{
		if (_XYZW_PS)
			mVUUpperClamp2Scalar_oaknut(mVU, FtX, false);
		mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		mVUUpperMulSs_oaknut(mVU, Fs, FtX);
	}
	else
	{
		if (_XYZW_PS)
			mVUUpperClamp2Vector_oaknut(mVU, FtX, false);
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		mVUUpperMulPs_oaknut(mVU, Fs, FtX);
	}
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, FtX);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtX);
	mVU.profiler.EmitOp(opMULx);
}

static void mVU_MULx_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		mVU_MULx_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("MUL"); mVUlogFd(); mVUlog(", vf%02dx", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MULy Opcode
static void mVU_MULy_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtY = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtY, FtRaw, 1);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
	{
		if (_XYZW_PS)
			mVUUpperClamp2Scalar_oaknut(mVU, FtY, false);
		mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		mVUUpperMulSs_oaknut(mVU, Fs, FtY);
	}
	else
	{
		if (_XYZW_PS)
			mVUUpperClamp2Vector_oaknut(mVU, FtY, false);
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		mVUUpperMulPs_oaknut(mVU, Fs, FtY);
	}
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, FtY);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtY);
	mVU.profiler.EmitOp(opMULy);
}

static void mVU_MULy_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		mVU_MULy_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("MUL"); mVUlogFd(); mVUlog(", vf%02dy", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MULz Opcode
static void mVU_MULz_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtZ = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtZ, FtRaw, 2);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
	{
		if (_XYZW_PS)
			mVUUpperClamp2Scalar_oaknut(mVU, FtZ, false);
		mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		mVUUpperMulSs_oaknut(mVU, Fs, FtZ);
	}
	else
	{
		if (_XYZW_PS)
			mVUUpperClamp2Vector_oaknut(mVU, FtZ, false);
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		mVUUpperMulPs_oaknut(mVU, Fs, FtZ);
	}
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, FtZ);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtZ);
	mVU.profiler.EmitOp(opMULz);
}

static void mVU_MULz_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		mVU_MULz_direct_emit_oaknut(mVU, recPass);
	}
	pass3 { mVUlog("MUL"); mVUlogFd(); mVUlog(", vf%02dz", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MULw Opcode
static void mVU_MULw_direct_emit_oaknut(mP)
{
	const int FtRaw = mVU.regAlloc->allocRegId(_Ft_);
	const int FtW = mVU.regAlloc->allocRegId();
	recBeginOaknutEmit();
	mVUUpperDupLane_oaknut(FtW, FtRaw, 3);
	recEndOaknutEmit();
	mVU.regAlloc->clearNeededXmmId(FtRaw);

	const int Fs = mVU.regAlloc->allocRegId(_Fs_, _Fd_, _X_Y_Z_W);

	recBeginOaknutEmit();
	if (_XYZW_SS)
	{
		if (_XYZW_PS)
			mVUUpperClamp2Scalar_oaknut(mVU, FtW, false);
		mVUUpperClamp2Scalar_oaknut(mVU, Fs, false);
		mVUUpperMulSs_oaknut(mVU, Fs, FtW);
	}
	else
	{
		if (_XYZW_PS)
			mVUUpperClamp2Vector_oaknut(mVU, FtW, false);
		mVUUpperClamp2Vector_oaknut(mVU, Fs, false);
		mVUUpperMulPs_oaknut(mVU, Fs, FtW);
	}
	recEndOaknutEmit();

	mVUupdateFlags_oaknut(mVU, Fs, FtW);

	mVU.regAlloc->clearNeededXmmId(Fs);
	mVU.regAlloc->clearNeededXmmId(FtW);
	mVU.profiler.EmitOp(opMULw);
}

static void mVU_MULw_emit(mP)
{
	pass1 { mVUanalyzeFMAC3(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		mVU_MULw_direct_emit_oaknut(mVU, recPass);
	}
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
	mVU.profiler.EmitOp(opABS);
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
	mVU.profiler.EmitOp(opCLIP);
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
	mVU.profiler.EmitOp(opOPMULA);
}

static void mVU_OPMULA_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		mVU_OPMULA_direct_emit_oaknut(mVU, recPass);
	}
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
	const int Ft = mVU.regAlloc->allocRegId(_Ft_, 0, 0xf);
	const int Fs = mVU.regAlloc->allocRegId(_Fs_, 0, 0xf);
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
	mVU.profiler.EmitOp(opOPMSUB);
}

static void mVU_OPMSUB_emit(mP)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, _Ft_); }
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
	mVU.profiler.EmitOp(opFTOI0);
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
	mVU.profiler.EmitOp(opFTOI4);
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
	mVU.profiler.EmitOp(opFTOI12);
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
	mVU.profiler.EmitOp(opFTOI15);
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
	mVU.profiler.EmitOp(opITOF0);
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
	mVU.profiler.EmitOp(opITOF4);
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
	mVU.profiler.EmitOp(opITOF12);
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
	mVU.profiler.EmitOp(opITOF15);
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
	pass2 { mVU.profiler.EmitOp(opNOP); }
	pass3 { mVUlog("NOP"); }
}

//------------------------------------------------------------------
// Micro VU Micromode Upper instructions
//------------------------------------------------------------------
