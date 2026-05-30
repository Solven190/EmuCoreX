// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

//------------------------------------------------------------------
// Micro VU - Pass 2 Functions
//------------------------------------------------------------------

//------------------------------------------------------------------
// Flag Allocators
//------------------------------------------------------------------

__fi static int getFlagRegId(uint fInst)
{
	static constexpr int gprFlags[4] = {VU_HOST_F0, VU_HOST_F1, VU_HOST_F2, VU_HOST_F3};
	pxAssert(fInst < 4);
	return gprFlags[fInst];
}

__fi static OakMemOperand mVUAllocOakCpuMem(s64 offset)
{
	return {oak::util::X27, offset};
}

__fi static OakMemOperand mVUAllocOakMvuMem(s64 offset)
{
	return {oak::util::X28, offset};
}

__fi static void mVUSetBitSFLAG_emit_oaknut(oak::WReg dst, oak::WReg src, u32 bitTest, u32 bitSet)
{
	oak::Label skip;
	oakAsm->TST(src, bitTest);
	oakAsm->B(oak::Cond::EQ, skip);
	oakAsm->ORR(dst, dst, bitSet);
	oakAsm->l(skip);
}

__fi void setBitSFLAG(int reg, int regT, int bitTest, int bitSet)
{
	recBeginOaknutEmit();
	const oak::WReg dst = oakWRegister(reg);
	const oak::WReg src = oakWRegister(regT);
//	skip.SetTarget();
	mVUSetBitSFLAG_emit_oaknut(dst, src, static_cast<u32>(bitTest), static_cast<u32>(bitSet));
	recEndOaknutEmit();
}

__fi void setBitFSEQ(int reg, int bitX)
{
	recBeginOaknutEmit();
	const oak::WReg dst = oakWRegister(reg);
	oak::Label skip;
	oakAsm->TST(dst, static_cast<u32>(bitX));
	oakAsm->B(oak::Cond::EQ, skip);
	oakAsm->ORR(dst, dst, static_cast<u32>(bitX));
	oakAsm->l(skip);
	recEndOaknutEmit();
}

__fi void mVUallocSFLAGa(int reg, int fInstance)
{
	recBeginOaknutEmit();
	oakAsm->MOV(oakWRegister(reg), oakWRegister(getFlagRegId(fInstance)));
	recEndOaknutEmit();
}

__fi void mVUallocSFLAGb(int reg, int fInstance)
{
	recBeginOaknutEmit();
	oakAsm->MOV(oakWRegister(getFlagRegId(fInstance)), oakWRegister(reg));
	recEndOaknutEmit();
}

// Normalize Status Flag
__ri void mVUallocSFLAGc(int reg, int regT, int fInstance)
{
	recBeginOaknutEmit();
	const oak::WReg dst = oakWRegister(reg);
	const oak::WReg tmp = oakWRegister(regT);
	oakAsm->EOR(dst, dst, dst);
	oakAsm->MOV(tmp, oakWRegister(getFlagRegId(fInstance)));

	mVUSetBitSFLAG_emit_oaknut(dst, tmp, 0x0f00, 0x0001);
	mVUSetBitSFLAG_emit_oaknut(dst, tmp, 0xf000, 0x0002);
	mVUSetBitSFLAG_emit_oaknut(dst, tmp, 0x000f, 0x0040);
	mVUSetBitSFLAG_emit_oaknut(dst, tmp, 0x00f0, 0x0080);
	oakAsm->AND(tmp, tmp, 0xffff0000);
	oakAsm->LSR(tmp, tmp, 14);
	oakAsm->ORR(dst, dst, tmp);
	recEndOaknutEmit();
}

__fi void mVUallocMFLAGa(mV, int reg, int fInstance)
{
	recBeginOaknutEmit();
	oakLoad16(oakWRegister(reg),
		mVUAllocOakMvuMem(static_cast<s64>(offsetof(vuRegistersPack, microVU[mVU.index].macFlag[fInstance]))));
	recEndOaknutEmit();
}

__fi void mVUallocMFLAGb(mV, int reg, int fInstance)
{
	recBeginOaknutEmit();
    if (fInstance < 4) {
		oakStore32(oakWRegister(reg),
			mVUAllocOakMvuMem(static_cast<s64>(offsetof(vuRegistersPack, microVU[mVU.index].macFlag[fInstance]))));
    }
    else               {
		oakStore32(oakWRegister(reg),
			mVUAllocOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].VI[REG_MAC_FLAG].UL))));
    }
	recEndOaknutEmit();
}

__fi void mVUallocCFLAGa(mV, int reg, int fInstance)
{
	recBeginOaknutEmit();
    if (fInstance < 4) {
		oakLoad32(oakWRegister(reg),
			mVUAllocOakMvuMem(static_cast<s64>(offsetof(vuRegistersPack, microVU[mVU.index].clipFlag[fInstance]))));
    }
    else               {
		oakLoad32(oakWRegister(reg),
			mVUAllocOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].VI[REG_CLIP_FLAG].UL))));
    }
	recEndOaknutEmit();
}

__fi void mVUallocCFLAGb(mV, int reg, int fInstance)
{
	recBeginOaknutEmit();
    if (fInstance < 4) {
		oakStore32(oakWRegister(reg),
			mVUAllocOakMvuMem(static_cast<s64>(offsetof(vuRegistersPack, microVU[mVU.index].clipFlag[fInstance]))));
    }
    else               {
		oakStore32(oakWRegister(reg),
			mVUAllocOakCpuMem(static_cast<s64>(offsetof(cpuRegistersPack, vuRegs[mVU.index].VI[REG_CLIP_FLAG].UL))));
    }
	recEndOaknutEmit();
}

//------------------------------------------------------------------
// VI Reg Allocators
//------------------------------------------------------------------

void microRegAlloc::writeVIBackup(int reg)
{
	microVU& mVU = index ? microVU1 : microVU0;
	recBeginOaknutEmit();
	oakStore32(oakWRegister(reg), {oak::util::X28, static_cast<s64>(offsetof(vuRegistersPack, microVU[mVU.index].VIbackup))});
	recEndOaknutEmit();
}

//------------------------------------------------------------------
// P/Q Reg Allocators
//------------------------------------------------------------------

__ri void writeQreg(int reg, int qInstance)
{
	recBeginOaknutEmit();
	const oak::QReg dst = oakQRegister(VU_HOST_XMMPQ);
	const oak::QReg src = oakQRegister(reg);
    if (qInstance) {
		oakAsm->MOV(dst.Selem()[1], src.Selem()[0]);
    }
    else {
		oakAsm->MOV(dst.Selem()[0], src.Selem()[0]);
    }
	recEndOaknutEmit();
}
