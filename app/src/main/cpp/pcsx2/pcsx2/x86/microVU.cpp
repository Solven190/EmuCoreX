// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "microVU.h"

#include "common/AlignedMalloc.h"
#include "common/Perf.h"
#include "common/StringUtil.h"
#include "arm64/OaknutHelpers.h"

alignas(64) vuRegistersPack g_vuRegistersPack;
VU_Thread& vu1Thread = g_vuRegistersPack.vu1Thread;

static constexpr size_t mVUmaxProgramsPerStartPc = 32;
static constexpr size_t mVUmaxRetirePerTrim = 4;

//------------------------------------------------------------------
// Micro VU - Main Functions
//------------------------------------------------------------------

// Only run this once per VU! ;)
void mVUinit(microVU& mVU, uint vuIndex)
{
	std::memset(&mVU.prog, 0, sizeof(mVU.prog));
	// std::vector survives memset ({nullptr,0,0} = valid empty vector).
	// std::unordered_map does NOT – bucket list must be properly initialized.
	// Re-run constructors in-place after zeroing to fix the crash.
	new (&mVU.prog.garbage_programs) std::vector<microProgram*>();
	new (&mVU.prog.prog_lookup) std::unordered_map<u64, microProgram*>();

	mVU.index        =  vuIndex;
	mVU.cop2         =  0;
	mVU.vuMemSize    = (mVU.index ? 0x4000 : 0x1000);
	mVU.microMemSize = (mVU.index ? 0x4000 : 0x1000);
	mVU.progSize     = (mVU.index ? 0x4000 : 0x1000) / 4;
	mVU.progMemMask  =  mVU.progSize-1;
	mVU.cache        = vuIndex ? SysMemory::GetVU1Rec() : SysMemory::GetVU0Rec();
	mVU.prog.x86end  = (vuIndex ? SysMemory::GetVU1RecEnd() : SysMemory::GetVU0RecEnd()) - (mVUcacheSafeZone * _1mb);

	mVU.regAlloc.reset(new microRegAlloc(mVU.index));
}

// Resets Rec Data
void mVUreset(microVU& mVU, bool resetReserve)
{
	mVU.profiler.Add(mVU.profiler.resets);

	if (THREAD_VU1)
	{
		DevCon.Warning("mVU Reset");
		// If MTVU is toggled on during gameplay we need to flush the running VU1 program, else it gets in a mess
		if (VU0.VI[REG_VPU_STAT].UL & 0x100)
		{
			CpuVU1->Execute(vu1RunCycles);
		}
		VU0.VI[REG_VPU_STAT].UL &= ~0x100;
	}

    oakSetAsmPtr(mVU.cache, mVU.index ? HostMemoryMap::mVU1recSize : HostMemoryMap::mVU0recSize);
    oakStartBlock();
    ////
	mVUdispatcherAB(mVU);
	mVUdispatcherCD(mVU);
	mVUGenerateWaitMTVU(mVU);
	mVUGenerateCopyPipelineState(mVU);
	mVUGenerateCompareState(mVU);
    ////
    mVU.prog.x86start = oakEndBlock();

	mVU.regs().nextBlockCycles = 0;
	memset(&mVU.prog.lpState, 0, sizeof(mVU.prog.lpState));
	mVU.profiler.Reset(mVU.index);

	// Program Variables
	mVU.prog.cleared  =  1;
	mVU.prog.isSame   = -1;
	mVU.prog.cur      = NULL;
	mVU.prog.total    =  0;
	mVU.prog.curFrame =  0;

	// Setup Dynarec Cache Limits for Each Program
//	mVU.prog.x86start = xGetAlignedCallTarget();
	mVU.prog.x86ptr   = mVU.prog.x86start;

    u32 i, e = (mVU.progSize >> 1); // mVU.progSize / 2
	for ( i = 0; i < e; ++i)
	{
		if (!mVU.prog.prog[i])
		{
			mVU.prog.prog[i] = new std::deque<microProgram*>();
			continue;
		}
		auto it(mVU.prog.prog[i]->begin());
		for (; it != mVU.prog.prog[i]->end(); ++it)
		{
			mVUdeleteProg(mVU, it[0]);
		}
		mVU.prog.prog[i]->clear();
		mVU.prog.quick[i].block = nullptr;
		mVU.prog.quick[i].prog = nullptr;
		mVU.prog.quick[i].cachedBlockManager = nullptr;
		mVU.prog.quick[i].cachedBlock = nullptr;
		mVU.prog.quick[i].cachedBlockQuick = 0;
	}
	for (microProgram* p : mVU.prog.garbage_programs)
	{
		mVUdeleteProg(mVU, p);
	}
	mVU.prog.garbage_programs.clear();
	mVU.prog.prog_lookup.clear(); // Clear O(1) lookup map
	if (mVU.index)
		mVU.prog.prog_lookup.reserve(1024);
	std::memset(mVU.prog.active_mask, 0, sizeof(mVU.prog.active_mask));
}

// Free Allocated Resources
void mVUclose(microVU& mVU)
{
	// Delete Programs and Block Managers
    u32 i, e = (mVU.progSize >> 1); // mVU.progSize / 2
	for (i = 0; i < e; ++i)
	{
		if (!mVU.prog.prog[i])
			continue;
		auto it(mVU.prog.prog[i]->begin());
		for (; it != mVU.prog.prog[i]->end(); ++it)
		{
			mVUdeleteProg(mVU, it[0]);
		}
		safe_delete(mVU.prog.prog[i]);
	}
	for (microProgram* p : mVU.prog.garbage_programs)
	{
		mVUdeleteProg(mVU, p);
	}
	mVU.prog.garbage_programs.clear();
}

static __fi bool mVUrangeOverlaps(const microRange& range, u32 start, u32 end)
{
	if (range.start < 0 || range.end < 0)
		return true;
	return start < static_cast<u32>(range.end) && static_cast<u32>(range.start) < end;
}

static __fi bool mVUprogOverlapsRange(const microProgram& prog, u32 start, u32 end)
{
	if (!prog.ranges || prog.ranges->empty())
		return true;

	for (const microRange& range : *prog.ranges)
	{
		if (mVUrangeOverlaps(range, start, end))
			return true;
	}
	return false;
}

static __fi bool mVUquickReferencesProg(const microVU& mVU, const microProgram& prog)
{
	for (u16 i : *prog.active_blocks)
	{
		if (mVU.prog.quick[i].prog == &prog)
			return true;
	}
	return false;
}

static void mVUremoveLookupEntriesForProg(microVU& mVU, const microProgram& prog)
{
	for (auto it = mVU.prog.prog_lookup.begin(); it != mVU.prog.prog_lookup.end();)
	{
		if (it->second == &prog)
			it = mVU.prog.prog_lookup.erase(it);
		else
			++it;
	}
}

static void mVUretireProgFromLookup(microVU& mVU, microProgram* prog)
{
	mVUremoveLookupEntriesForProg(mVU, *prog);
	mVU.prog.garbage_programs.push_back(prog);
	mVU.profiler.Add(mVU.profiler.retiredPrograms);
}

static void mVUtrimProgramList(microVU& mVU, microProgramList& list)
{
	if (list.size() <= mVUmaxProgramsPerStartPc)
		return;

	size_t retired = 0;
	for (auto it = list.end(); it != list.begin() && list.size() > mVUmaxProgramsPerStartPc && retired < mVUmaxRetirePerTrim;)
	{
		--it;
		microProgram* prog = *it;
		if (prog == mVU.prog.cur || mVUquickReferencesProg(mVU, *prog))
			continue;

		it = list.erase(it);
		mVUretireProgFromLookup(mVU, prog);
		retired++;
	}
}

static u32 mVUcountActiveQuickEntries(const microVU& mVU)
{
	u32 count = 0;
	for (u64 mask : mVU.prog.active_mask)
		count += static_cast<u32>(__builtin_popcountll(mask));
	return count;
}

static std::string mVUformatCacheState(const microVU& mVU)
{
	const u64 cache_used = (mVU.prog.x86ptr > mVU.prog.x86start) ?
		static_cast<u64>(mVU.prog.x86ptr - mVU.prog.x86start) : 0;
	const u64 cache_limit = (mVU.prog.x86end > mVU.prog.x86start) ?
		static_cast<u64>(mVU.prog.x86end - mVU.prog.x86start) : 0;
	const u64 cache_free = (cache_limit > cache_used) ? (cache_limit - cache_used) : 0;

	char line[256];
	std::snprintf(line, sizeof(line),
		"cache_used=%llu cache_limit=%llu cache_free=%llu lookup=%llu garbage=%llu active_quick=%u",
		static_cast<unsigned long long>(cache_used),
		static_cast<unsigned long long>(cache_limit),
		static_cast<unsigned long long>(cache_free),
		static_cast<unsigned long long>(mVU.prog.prog_lookup.size()),
		static_cast<unsigned long long>(mVU.prog.garbage_programs.size()),
		mVUcountActiveQuickEntries(mVU));
	return line;
}

// Clears Block Data in specified range
__fi void mVUclear(mV, u32 addr, u32 size)
{
	mVU.profiler.Add(mVU.profiler.clears);

	const u32 clear_start = std::min(addr, mVU.microMemSize);
	const u32 clear_end = std::min(addr + size, mVU.microMemSize);
	bool invalidated = false;

	microProgram* last_prog = nullptr;
	bool last_overlaps = false;

	for (u32 mask_idx = 0; mask_idx < (mProgSizeHalf / 64); ++mask_idx)
	{
		u64 mask = mVU.prog.active_mask[mask_idx];
		while (mask)
		{
			u32 bit = __builtin_ctzll(mask);
			mask &= mask - 1; // Clear lowest bit
			u32 i = mask_idx * 64 + bit;

			microProgramQuick& quick = mVU.prog.quick[i];
			if (!quick.prog)
			{
				mVU.prog.active_mask[mask_idx] &= ~(1ULL << bit);
				continue;
			}

			bool overlaps;
			if (quick.prog == last_prog)
			{
				overlaps = last_overlaps;
			}
			else
			{
				overlaps = mVUprogOverlapsRange(*quick.prog, clear_start, clear_end);
				last_prog = quick.prog;
				last_overlaps = overlaps;
			}

			if (!overlaps)
				continue;

			quick.block = nullptr;
			quick.prog = nullptr;
			quick.cachedBlockManager = nullptr;
			quick.cachedBlock = nullptr;
			quick.cachedBlockQuick = 0;
			mVU.prog.active_mask[mask_idx] &= ~(1ULL << bit);
			invalidated = true;
		}
	}

	if (invalidated)
	{
		mVU.prog.cleared = 1;
		std::memset(&mVU.prog.lpState, 0, sizeof(mVU.prog.lpState)); // Clear pipeline state
	}
}

//------------------------------------------------------------------
// Micro VU - Private Functions
//------------------------------------------------------------------

// Deletes a program
__ri void mVUdeleteProg(microVU& mVU, microProgram*& prog)
{
	for (u16 i : *prog->active_blocks)
	{
		microProgramQuick& quick = mVU.prog.quick[i];
		if (quick.prog == prog)
		{
			quick.block = nullptr;
			quick.prog = nullptr;
			quick.cachedBlockManager = nullptr;
			quick.cachedBlock = nullptr;
			quick.cachedBlockQuick = 0;
			mVU.prog.active_mask[i / 64] &= ~(1ULL << (i % 64));
		}
	}

	for (u16 i : *prog->active_blocks)
	{
		safe_delete(prog->block[i]);
	}
	safe_delete(prog->ranges);
	safe_delete(prog->active_blocks);
	safe_aligned_free(prog);
}

// Creates a new Micro Program
__ri microProgram* mVUcreateProg(microVU& mVU, int startPC)
{
	EMUCOREX_PROFILE_SCOPE(mVU.index ? "microVU1 Create Program" : "microVU0 Create Program");
	mVU.profiler.Add(mVU.profiler.createPrograms);
	auto* prog = (microProgram*)_aligned_malloc(sizeof(microProgram), 64);
	memset(prog, 0, sizeof(microProgram));
	prog->idx = mVU.prog.total++;
	prog->ranges = new std::deque<microRange>();
	prog->active_blocks = new std::vector<u16>();
	prog->startPC = startPC;
	prog->microMemVersion = (mVU.index && THREAD_VU1) ? vu1Thread.microMemVersion : 0;
	if(doWholeProgCompare)
		mVUcacheProg(mVU, *prog); // Cache Micro Program
	return prog;
}

// Caches Micro Program
__ri void mVUcacheProg(microVU& mVU, microProgram& prog)
{
	if (!doWholeProgCompare)
	{
		auto cmpOffset = [&](void* x) { return (u8*)x + mVUrange.start; };
		memcpy(cmpOffset(prog.data), cmpOffset(mVU.regs().Micro), (mVUrange.end - mVUrange.start));
	}
	else
	{
		if (!mVU.index)
			memcpy(prog.data, mVU.regs().Micro, 0x1000);
		else
			memcpy(prog.data, mVU.regs().Micro, 0x4000);
	}
	// Update the fast-reject hash from prog.data at startPC.
	// startPC is in 8-byte units; we XOR the first two u64 words there (16 bytes = 2 VU instructions).
	// This is safe because the first compiled range always covers startPC.
	const u32 startByteOff = prog.startPC * 8;
	if (startByteOff + 16 <= mVU.microMemSize)
	{
		const u64* p = reinterpret_cast<const u64*>(reinterpret_cast<const u8*>(prog.data) + startByteOff);
		prog.startHash = p[0] ^ p[1];
	}
}

// Generate Hash for partial program based on compiled ranges...
u64 mVUrangesHash(microVU& mVU, microProgram& prog)
{
	union
	{
		u64 v64;
		u32 v32[2];
	} hash = {0};

	std::deque<microRange>::const_iterator it(prog.ranges->begin());
    int i, s, e;
	for (; it != prog.ranges->end(); ++it)
	{
		if ((it[0].start < 0) || (it[0].end < 0))
		{
			DevCon.Error("microVU%d: Negative Range![%d][%d]", mVU.index, it[0].start, it[0].end);
		}
        s = it[0].start >> 2; // it[0].start / 4
        e = it[0].end >> 2;   // it[0].end / 4
		for (i = s; i < e; ++i)
		{
			hash.v32[0] -= prog.data[i];
			hash.v32[1] ^= prog.data[i];
		}
	}
	return hash.v64;
}

// Prints the ratio of unique programs to total programs
void mVUprintUniqueRatio(microVU& mVU)
{
	std::vector<u64> v;
    u32 pc, e = mProgSize >> 1; // mProgSize / 2
	for (pc = 0; pc < e; ++pc)
	{
		microProgramList* list = mVU.prog.prog[pc];
		if (!list)
			continue;
		auto it(list->begin());
		for (; it != list->end(); ++it)
		{
			v.push_back(mVUrangesHash(mVU, *it[0]));
		}
	}
	u32 total = v.size();
	sortVector(v);
	makeUnique(v);
	if (!total)
		return;
}

// Compare Cached microProgram to mVU.regs().Micro
// liveStartHash: XOR of first 16 bytes of live micro-memory at startPC, pre-computed once per search.
__fi bool mVUcmpProg(microVU& mVU, microProgram& prog, u64 liveStartHash)
{
	if (mVU.index && THREAD_VU1 && prog.microMemVersion == vu1Thread.microMemVersion)
	{
		mVU.prog.cleared = 0;
		mVU.prog.cur = &prog;
		mVU.prog.isSame = 1;
		return true;
	}

	// Fast O(1) reject: if the first two instructions at startPC differ, skip memcmp entirely.
	// prog.startHash == 0 means not yet computed (brand-new program) – allow through.
	if (prog.startHash != 0 && prog.startHash != liveStartHash)
		return false;

	if (doWholeProgCompare)
	{
		if (memcmp((u8*)prog.data, mVU.regs().Micro, mVU.microMemSize))
			return false;
	}
	else
	{
		for (const auto& range : *prog.ranges)
		{
#if defined(PCSX2_DEVBUILD) || defined(_DEBUG)
			if ((range.start < 0) || (range.end < 0))
				DevCon.Error("microVU%d: Negative Range![%d][%d]", mVU.index, range.start, range.end);
#endif
			auto cmpOffset = [&](void* x) { return (u8*)x + range.start; };

			u8* pData = cmpOffset(prog.data);
			u8* pMem  = cmpOffset(mVU.regs().Micro);
			u32 len   = range.end - range.start;

			if (len >= 4 && *(u32*)pData != *(u32*)pMem)
				return false;

			if (memcmp(pData, pMem, len))
				return false;
		}
	}
	mVU.prog.cleared = 0;
	mVU.prog.cur = &prog;
	mVU.prog.isSame = doWholeProgCompare ? 1 : -1;
	prog.microMemVersion = (mVU.index && THREAD_VU1) ? vu1Thread.microMemVersion : 0;
	return true;
}

// Searches for Cached Micro Program and sets prog.cur to it (returns entry-point to program)
_mVUt __fi void* mVUsearchProg(u32 startPC, uptr pState)
{
	microVU& mVU = mVUx;
	mVU.profiler.Add(mVU.profiler.searchCalls);

    u32 start_pc_8 = startPC >> 3; // startPC / 8
    u32 regs_start_pc_8 = mVU.regs().start_pc >> 3; // mVU.regs().start_pc / 8

	microProgramQuick& quick = mVU.prog.quick[regs_start_pc_8];
	microProgramList*  list  = mVU.prog.prog [regs_start_pc_8];

	if (!quick.prog) // If null, we need to search for new program
	{
		// Pre-compute startHash ONCE: XOR of first 16 bytes at the program entry point.
		u64 liveStartHash = 0;
		{
			const u32 startByteOff = regs_start_pc_8 * 8;
			if (startByteOff + 16 <= mVU.microMemSize)
			{
				const u64* p = reinterpret_cast<const u64*>(
					reinterpret_cast<const u8*>(mVU.regs().Micro) + startByteOff);
				liveStartHash = p[0] ^ p[1];
			}
		}

		// Build the O(1) map key: mix startHash with PC to avoid cross-slot collisions.
		const u64 mapKey = liveStartHash ^ (u64(regs_start_pc_8) * 6364136223846793005ULL);

		// ── Fast path: O(1) hash map lookup ──────────────────────────────────────────
		// Avoids O(N) deque scan entirely when the program was seen before.
		// Falls back to deque only for hash collisions (probability ~1/2^64).
		if (liveStartHash != 0)
		{
			auto mapIt = mVU.prog.prog_lookup.find(mapKey);
			if (mapIt != mVU.prog.prog_lookup.end() && mapIt->second != nullptr)
			{
				microProgram* candidate = mapIt->second;
				if (mVUcmpProg(mVU, *candidate, liveStartHash))
				{
					// Found via O(1) map lookup!
					mVU.profiler.Add(mVU.profiler.searchListHits);
					microProgram* prog = candidate;
					for (u16 i : *prog->active_blocks)
					{
						if (prog->block[i])
						{
							microProgramQuick& q = mVU.prog.quick[i];
							mVU.prog.active_mask[i / 64] |= (1ULL << (i % 64));
							q.block = prog->block[i];
							q.prog = prog;
							q.cachedBlockManager = nullptr;
							q.cachedBlock = nullptr;
							q.cachedBlockQuick = 0;
						}
					}
					if (quick.block == nullptr)
					{
						void* entryPoint = mVUblockFetch(mVU, startPC, pState);
						return entryPoint;
					}
					microBlock* pBlock = quick.block->search(mVU, (microRegInfo*)pState);
					if (pBlock)
					{
						mVU.profiler.Add(mVU.profiler.blockHits);
						if (!((microRegInfo*)pState)->needExactMatch)
						{
							quick.cachedBlockManager = quick.block;
							quick.cachedBlock = pBlock;
							quick.cachedBlockQuick = ((microRegInfo*)pState)->quick64[0];
						}
						return pBlock->x86ptrStart;
					}
					mVU.profiler.Add(mVU.profiler.blockMisses);
					return mVUcompile(mVU, startPC, pState);
				}
				// mVUcmpProg failed: stale map entry, erase and fall through to deque.
				mVU.prog.prog_lookup.erase(mapIt);
			}
		}

		// ── Fallback: O(N) deque scan (hash collision or liveStartHash == 0) ─────────
		auto it(list->begin());
		for (; it != list->end(); ++it)
		{
			bool b = mVUcmpProg(mVU, *it[0], liveStartHash);

			if (b)
			{
				mVU.profiler.Add(mVU.profiler.searchListHits);
				microProgram* prog = it[0];
				for (u16 i : *prog->active_blocks)
				{
					if (prog->block[i])
					{
						microProgramQuick& q = mVU.prog.quick[i];
						mVU.prog.active_mask[i / 64] |= (1ULL << (i % 64));
						q.block = prog->block[i];
						q.prog = prog;
						q.cachedBlockManager = nullptr;
						q.cachedBlock = nullptr;
						q.cachedBlockQuick = 0;
					}
				}
				list->erase(it);
				list->push_front(prog);
				// Update map so next search for this program is O(1)
				if (liveStartHash != 0)
					mVU.prog.prog_lookup[mapKey] = prog;

				// Sanity check, in case for some reason the program compilation aborted half way through (JALR for example)
				if (quick.block == nullptr)
				{
					void* entryPoint = mVUblockFetch(mVU, startPC, pState);
					return entryPoint;
				}
				microBlock* pBlock = quick.block->search(mVU, (microRegInfo*)pState);
				if (pBlock)
				{
					mVU.profiler.Add(mVU.profiler.blockHits);
					if (!((microRegInfo*)pState)->needExactMatch)
					{
						quick.cachedBlockManager = quick.block;
						quick.cachedBlock = pBlock;
						quick.cachedBlockQuick = ((microRegInfo*)pState)->quick64[0];
					}
					return pBlock->x86ptrStart;
				}
				mVU.profiler.Add(mVU.profiler.blockMisses);
				return mVUcompile(mVU, startPC, pState);
			}
		}

		// If cleared and program not found, make a new program instance
		mVU.profiler.Add(mVU.profiler.searchMisses);
		mVU.prog.cleared = 0;
		mVU.prog.isSame  = 1;
		mVU.prog.cur     = mVUcreateProg(mVU, regs_start_pc_8);
		void* entryPoint = mVUblockFetch(mVU,  startPC, pState);
		quick.block      = mVU.prog.cur->block[start_pc_8];
		quick.prog       = mVU.prog.cur;
		quick.cachedBlockManager = nullptr;
		quick.cachedBlock = nullptr;
		quick.cachedBlockQuick = 0;
		mVU.prog.active_mask[start_pc_8 / 64] |= (1ULL << (start_pc_8 % 64));
		list->push_front(mVU.prog.cur);
		mVUtrimProgramList(mVU, *list);
		// Store new program in O(1) map so next search is instant
		if (liveStartHash != 0)
			mVU.prog.prog_lookup[mapKey] = mVU.prog.cur;
		//mVUprintUniqueRatio(mVU);
		return entryPoint;
	}

	// If list.quick, then we've already found and recompiled the program ;)
	mVU.profiler.Add(mVU.profiler.searchQuickHits);
	mVU.prog.isSame = -1;
	mVU.prog.cur = quick.prog;
	// Because the VU's can now run in sections and not whole programs at once
	// we need to set the current block so it gets the right program back
	quick.block = mVU.prog.cur->block[start_pc_8];

	// Sanity check, in case for some reason the program compilation aborted half way through
	if (quick.block == nullptr)
	{
		void* entryPoint = mVUblockFetch(mVU, startPC, pState);
		return entryPoint;
	}
	if (!((microRegInfo*)pState)->needExactMatch && quick.cachedBlockManager == quick.block &&
		quick.cachedBlock && quick.cachedBlockQuick == ((microRegInfo*)pState)->quick64[0])
	{
		mVU.profiler.Add(mVU.profiler.blockHits);
		return quick.cachedBlock->x86ptrStart;
	}

	microBlock* pBlock = quick.block->search(mVU, (microRegInfo*)pState);
	if (pBlock)
	{
		mVU.profiler.Add(mVU.profiler.blockHits);
		if (!((microRegInfo*)pState)->needExactMatch)
		{
			quick.cachedBlockManager = quick.block;
			quick.cachedBlock = pBlock;
			quick.cachedBlockQuick = ((microRegInfo*)pState)->quick64[0];
		}
		return pBlock->x86ptrStart;
	}
	mVU.profiler.Add(mVU.profiler.blockMisses);
	return mVUcompile(mVU, startPC, pState);
}

//------------------------------------------------------------------
// recMicroVU0 / recMicroVU1
//------------------------------------------------------------------

recMicroVU0 CpuMicroVU0;
recMicroVU1 CpuMicroVU1;

recMicroVU0::recMicroVU0() { m_Idx = 0; IsInterpreter = false; }
recMicroVU1::recMicroVU1() { m_Idx = 1; IsInterpreter = false; }

void recMicroVU0::Reserve()
{
	mVUinit(microVU0, 0);
}
void recMicroVU1::Reserve()
{
	mVUinit(microVU1, 1);
	vu1Thread.Open();
}

void recMicroVU0::Shutdown()
{
	mVUclose(microVU0);
}
void recMicroVU1::Shutdown()
{
	if (vu1Thread.IsOpen())
		vu1Thread.WaitVU();
	mVUclose(microVU1);
}

void recMicroVU0::Reset()
{
	mVUreset(microVU0, true);
}

void recMicroVU0::Step()
{
}

void recMicroVU1::Reset()
{
	vu1Thread.WaitVU();
	vu1Thread.Get_MTVUChanges();
	mVUreset(microVU1, true);
}

void recMicroVU0::SetStartPC(u32 startPC)
{
	VU0.start_pc = startPC;
}

void recMicroVU0::Execute(u32 cycles)
{
	VU0.flags &= ~VUFLAG_MFLAGSET;

	if (!(VU0.VI[REG_VPU_STAT].UL & 1))
		return;
	VU0.VI[REG_TPC].UL <<= 3;

	((mVUrecCall)microVU0.startFunct)(VU0.VI[REG_TPC].UL, cycles);
	VU0.VI[REG_TPC].UL >>= 3;
	if (microVU0.regs().flags & 0x4)
	{
		microVU0.regs().flags &= ~0x4;
		hwIntcIrq(6);
	}
}

void recMicroVU1::SetStartPC(u32 startPC)
{
	VU1.start_pc = startPC;
}

void recMicroVU1::Step()
{
}

void recMicroVU1::Execute(u32 cycles)
{
	microVU1.profiler.Add(microVU1.profiler.executeCalls);

	if (!THREAD_VU1)
	{
		if (!(VU0.VI[REG_VPU_STAT].UL & 0x100))
			return;
	}
	VU1.VI[REG_TPC].UL <<= 3;
	((mVUrecCall)microVU1.startFunct)(VU1.VI[REG_TPC].UL, cycles);
	VU1.VI[REG_TPC].UL >>= 3;
	if (microVU1.regs().flags & 0x4 && !THREAD_VU1)
	{
		microVU1.regs().flags &= ~0x4;
		hwIntcIrq(7);
	}
}

void recMicroVU0::Clear(u32 addr, u32 size)
{
	mVUclear(microVU0, addr, size);
}
void recMicroVU1::Clear(u32 addr, u32 size)
{
	mVUclear(microVU1, addr, size);
}

void recMicroVU1::ResumeXGkick()
{
	if (!(VU0.VI[REG_VPU_STAT].UL & 0x100))
		return;
	((mVUrecCallXG)microVU1.startFunctXG)();
}

bool SaveStateBase::vuJITFreeze()
{
	if (IsSaving())
		vu1Thread.WaitVU();

	Freeze(microVU0.prog.lpState);
	Freeze(microVU1.prog.lpState);
	return IsOkay();
}

std::string mVUGetVU1ProfilerStatsAndReset()
{
	std::string stats = microVU1.profiler.GetJitStatsAndReset();
	if (!stats.empty())
	{
		stats += ' ';
		stats += mVUformatCacheState(microVU1);
	}
	return stats;
}
