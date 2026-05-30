// SPDX-FileCopyrightText: 2026 EmuCoreX Team
// SPDX-License-Identifier: GPL-3.0

#include "OaknutHelpers.h"

#include "common/Assertions.h"
#include "common/HostSys.h"

#include <bit>
#include <cstring>

static bool oakIsSigned9(s64 value)
{
	return value >= -256 && value <= 255;
}

static bool oakIsUnsignedScaled(s64 value, u32 scale, u32 bits)
{
	if (value < 0 || (value & (static_cast<s64>(scale) - 1)) != 0)
		return false;

	const u64 encoded = static_cast<u64>(value) >> std::countr_zero(scale);
	return encoded < (1ull << bits);
}

oak::WReg oakWRegister(int n)
{
	pxAssert(static_cast<unsigned>(n) < 32);
	return oak::WReg(n);
}

oak::XReg oakXRegister(int n)
{
	pxAssert(static_cast<unsigned>(n) < 32);
	return oak::XReg(n);
}

oak::SReg oakSRegister(int n)
{
	pxAssert(static_cast<unsigned>(n) < 32);
	return oak::SReg(n);
}

oak::DReg oakDRegister(int n)
{
	pxAssert(static_cast<unsigned>(n) < 32);
	return oak::DReg(n);
}

oak::QReg oakQRegister(int n)
{
	pxAssert(static_cast<unsigned>(n) < 32);
	return oak::QReg(n);
}

bool oakIsCalleeSavedRegister(int reg)
{
	return reg >= 19;
}

bool oakIsCallerSaved(int id)
{
#if defined(__ANDROID__)
	return id <= 15;
#else
#ifdef _WIN32
	return id <= 2 || (id >= 8 && id <= 11);
#else
	return id <= 2 || id == 6 || id == 7 || (id >= 8 && id <= 11);
#endif
#endif
}

bool oakIsCallerSavedXmm(int id)
{
#if defined(__ANDROID__)
	return true;
#else
#ifdef _WIN32
	return id < 6;
#else
	return true;
#endif
#endif
}

thread_local oak::CodeGenerator* oakAsm;
thread_local u8* oakAsmPtr;
thread_local size_t oakAsmCapacity;

void oakSetAsmPtr(void* ptr, size_t capacity)
{
	pxAssert(!oakAsm);
	oakAsmPtr = static_cast<u8*>(ptr);
	oakAsmCapacity = capacity;
}

void oakAlignAsmPtr()
{
	static constexpr uintptr_t ALIGNMENT = 16;
	u8* new_ptr = reinterpret_cast<u8*>((reinterpret_cast<uintptr_t>(oakAsmPtr) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1));
	pxAssert(static_cast<size_t>(new_ptr - oakAsmPtr) <= oakAsmCapacity);
	oakAsmCapacity -= new_ptr - oakAsmPtr;
	oakAsmPtr = new_ptr;
}

u8* oakStartBlock()
{
	oakAlignAsmPtr();

	HostSys::BeginCodeWrite();

	pxAssert(!oakAsm);
	oakAsm = new oak::CodeGenerator(reinterpret_cast<u32*>(oakAsmPtr));
	return oakAsmPtr;
}

u8* oakEndBlock()
{
	pxAssert(oakAsm);

	const u32 size = static_cast<u32>(oakAsm->offset());
	pxAssert(size < oakAsmCapacity);

	delete oakAsm;
	oakAsm = nullptr;

	HostSys::EndCodeWrite();
	HostSys::FlushInstructionCache(oakAsmPtr, size);

	oakAsmPtr += size;
	oakAsmCapacity -= size;
	return oakAsmPtr;
}

void oakMoveAddressToReg(oak::XReg reg, const void* addr)
{
	pxAssert(oakAsm);
	oakAsm->MOVP2R(reg, addr);
}

void oakEmitJmp(const void* ptr)
{
	pxAssert(oakAsm);
	oakMoveAddressToReg(OAK_XSCRATCH, ptr);
	oakAsm->BR(OAK_XSCRATCH);
}

void oakEmitCall(const void* ptr)
{
	pxAssert(oakAsm);
	oakMoveAddressToReg(OAK_XSCRATCH, ptr);
	oakAsm->BLR(OAK_XSCRATCH);
}

void oakEmitCondBranch(oak::Cond cond, const void* ptr)
{
	pxAssert(oakAsm);
	oak::Label branch_not_taken;
	oakAsm->B(oak::invert(cond), branch_not_taken);
	oakEmitJmp(ptr);
	oakAsm->l(branch_not_taken);
}

void oakEmitCbz(oak::WReg reg, const void* ptr)
{
	pxAssert(oakAsm);
	oak::Label branch_not_taken;
	oakAsm->CBNZ(reg, branch_not_taken);
	oakEmitJmp(ptr);
	oakAsm->l(branch_not_taken);
}

void oakEmitCbz(oak::XReg reg, const void* ptr)
{
	pxAssert(oakAsm);
	oak::Label branch_not_taken;
	oakAsm->CBNZ(reg, branch_not_taken);
	oakEmitJmp(ptr);
	oakAsm->l(branch_not_taken);
}

void oakEmitCbnz(oak::WReg reg, const void* ptr)
{
	pxAssert(oakAsm);
	oak::Label branch_not_taken;
	oakAsm->CBZ(reg, branch_not_taken);
	oakEmitJmp(ptr);
	oakAsm->l(branch_not_taken);
}

void oakEmitCbnz(oak::XReg reg, const void* ptr)
{
	pxAssert(oakAsm);
	oak::Label branch_not_taken;
	oakAsm->CBZ(reg, branch_not_taken);
	oakEmitJmp(ptr);
	oakAsm->l(branch_not_taken);
}

void oakEmitJmpPtr(void* code, const void* dst, bool flush_icache)
{
	const s64 displacement = static_cast<s64>(reinterpret_cast<intptr_t>(dst) - reinterpret_cast<intptr_t>(code));
	pxAssert((displacement & 3) == 0);
	pxAssert(displacement >= -(1ll << 27) && displacement < (1ll << 27));

	const u32 imm26 = static_cast<u32>((displacement >> 2) & 0x03ffffff);
	const u32 encoded = 0x14000000u | imm26;
	std::memcpy(code, &encoded, sizeof(encoded));

	if (flush_icache)
		HostSys::FlushInstructionCache(code, sizeof(encoded));
}

void oakPatchCondBranch(void* code, const void* dst, oak::Cond cond, bool flush_icache)
{
	const s64 displacement = static_cast<s64>(reinterpret_cast<intptr_t>(dst) - reinterpret_cast<intptr_t>(code));
	pxAssert((displacement & 3) == 0);
	pxAssert(displacement >= -(1ll << 20) && displacement < (1ll << 20));

	const u32 imm19 = static_cast<u32>((displacement >> 2) & 0x0007ffff);
	const u32 encoded = 0x54000000u | (imm19 << 5) | static_cast<u32>(cond);
	std::memcpy(code, &encoded, sizeof(encoded));

	if (flush_icache)
		HostSys::FlushInstructionCache(code, sizeof(encoded));
}

void oakLoad32(oak::WReg dst, OakMemOperand mem)
{
	pxAssert(oakAsm);
	if (oakIsUnsignedScaled(mem.offset, 4, 12))
		return oakAsm->LDR(dst, mem.base, oak::POffset<14, 2>(mem.offset));
	if (oakIsSigned9(mem.offset))
		return oakAsm->LDUR(dst, mem.base, oak::SOffset<9, 0>(mem.offset));

	oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(mem.offset));
	oakAsm->ADD(OAK_XSCRATCH, mem.base, OAK_XSCRATCH);
	oakAsm->LDR(dst, OAK_XSCRATCH);
}

void oakLoad64(oak::XReg dst, OakMemOperand mem)
{
	pxAssert(oakAsm);
	if (oakIsUnsignedScaled(mem.offset, 8, 12))
		return oakAsm->LDR(dst, mem.base, oak::POffset<15, 3>(mem.offset));
	if (oakIsSigned9(mem.offset))
		return oakAsm->LDUR(dst, mem.base, oak::SOffset<9, 0>(mem.offset));

	oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(mem.offset));
	oakAsm->ADD(OAK_XSCRATCH, mem.base, OAK_XSCRATCH);
	oakAsm->LDR(dst, OAK_XSCRATCH);
}

void oakLoad16(oak::WReg dst, OakMemOperand mem)
{
	pxAssert(oakAsm);
	if (oakIsUnsignedScaled(mem.offset, 2, 12))
		return oakAsm->LDRH(dst, mem.base, oak::POffset<13, 1>(mem.offset));
	if (oakIsSigned9(mem.offset))
		return oakAsm->LDURH(dst, mem.base, oak::SOffset<9, 0>(mem.offset));

	oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(mem.offset));
	oakAsm->ADD(OAK_XSCRATCH, mem.base, OAK_XSCRATCH);
	oakAsm->LDRH(dst, OAK_XSCRATCH);
}

void oakLoad128(oak::QReg dst, OakMemOperand mem)
{
	pxAssert(oakAsm);
	if (oakIsUnsignedScaled(mem.offset, 16, 12))
		return oakAsm->LDR(dst, mem.base, oak::POffset<16, 4>(mem.offset));
	if (oakIsSigned9(mem.offset))
		return oakAsm->LDUR(dst, mem.base, oak::SOffset<9, 0>(mem.offset));

	oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(mem.offset));
	oakAsm->ADD(OAK_XSCRATCH, mem.base, OAK_XSCRATCH);
	oakAsm->LDR(dst, OAK_XSCRATCH);
}

void oakStore32(oak::WReg src, OakMemOperand mem)
{
	pxAssert(oakAsm);
	if (src.index() == mem.base.index())
	{
		oakAsm->MOV(OAK_XSCRATCH2, static_cast<u64>(mem.offset));
		oakAsm->ADD(OAK_XSCRATCH2, mem.base, OAK_XSCRATCH2);
		return oakAsm->STR(src, OAK_XSCRATCH2);
	}
	if (oakIsUnsignedScaled(mem.offset, 4, 12))
		return oakAsm->STR(src, mem.base, oak::POffset<14, 2>(mem.offset));
	if (oakIsSigned9(mem.offset))
		return oakAsm->STUR(src, mem.base, oak::SOffset<9, 0>(mem.offset));

	const oak::XReg scratch = (src.index() == OAK_XSCRATCH.index()) ? OAK_XSCRATCH2 : OAK_XSCRATCH;
	oakAsm->MOV(scratch, static_cast<u64>(mem.offset));
	oakAsm->ADD(scratch, mem.base, scratch);
	oakAsm->STR(src, scratch);
}

void oakStore64(oak::XReg src, OakMemOperand mem)
{
	pxAssert(oakAsm);
	if (src.index() == mem.base.index())
	{
		oakAsm->MOV(OAK_XSCRATCH2, static_cast<u64>(mem.offset));
		oakAsm->ADD(OAK_XSCRATCH2, mem.base, OAK_XSCRATCH2);
		return oakAsm->STR(src, OAK_XSCRATCH2);
	}
	if (oakIsUnsignedScaled(mem.offset, 8, 12))
		return oakAsm->STR(src, mem.base, oak::POffset<15, 3>(mem.offset));
	if (oakIsSigned9(mem.offset))
		return oakAsm->STUR(src, mem.base, oak::SOffset<9, 0>(mem.offset));

	const oak::XReg scratch = (src.index() == OAK_XSCRATCH.index()) ? OAK_XSCRATCH2 : OAK_XSCRATCH;
	oakAsm->MOV(scratch, static_cast<u64>(mem.offset));
	oakAsm->ADD(scratch, mem.base, scratch);
	oakAsm->STR(src, scratch);
}

void oakStore16(oak::WReg src, OakMemOperand mem)
{
	pxAssert(oakAsm);
	if (src.index() == mem.base.index())
	{
		oakAsm->MOV(OAK_XSCRATCH2, static_cast<u64>(mem.offset));
		oakAsm->ADD(OAK_XSCRATCH2, mem.base, OAK_XSCRATCH2);
		return oakAsm->STRH(src, OAK_XSCRATCH2);
	}
	if (oakIsUnsignedScaled(mem.offset, 2, 12))
		return oakAsm->STRH(src, mem.base, oak::POffset<13, 1>(mem.offset));
	if (oakIsSigned9(mem.offset))
		return oakAsm->STURH(src, mem.base, oak::SOffset<9, 0>(mem.offset));

	const oak::XReg scratch = (src.index() == OAK_XSCRATCH.index()) ? OAK_XSCRATCH2 : OAK_XSCRATCH;
	oakAsm->MOV(scratch, static_cast<u64>(mem.offset));
	oakAsm->ADD(scratch, mem.base, scratch);
	oakAsm->STRH(src, scratch);
}

void oakStore128(oak::QReg src, OakMemOperand mem)
{
	pxAssert(oakAsm);
	if (oakIsUnsignedScaled(mem.offset, 16, 12))
		return oakAsm->STR(src, mem.base, oak::POffset<16, 4>(mem.offset));
	if (oakIsSigned9(mem.offset))
		return oakAsm->STUR(src, mem.base, oak::SOffset<9, 0>(mem.offset));

	oakAsm->MOV(OAK_XSCRATCH, static_cast<u64>(mem.offset));
	oakAsm->ADD(OAK_XSCRATCH, mem.base, OAK_XSCRATCH);
	oakAsm->STR(src, OAK_XSCRATCH);
}

void oakEmitSmokeReturn42()
{
	pxAssert(oakAsm);
	using namespace oak::util;
	oakAsm->MOV(W0, 42);
	oakAsm->RET();
}

void oakEmitSmokeFpAddReturn()
{
	pxAssert(oakAsm);
	using namespace oak::util;
	oakAsm->FADD(S0, S1, S2);
	oakAsm->RET();
}
