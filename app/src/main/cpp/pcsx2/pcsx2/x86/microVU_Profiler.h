// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include "arm64/OaknutHelpers.h"
#include "emucorex/native_profiler.h"

static constexpr bool mVUProfileOpcodes = false;

enum microOpcode
{
	// Upper Instructions
	opABS, opCLIP, opOPMULA, opOPMSUB, opNOP,
	opADD,   opADDi,   opADDq,   opADDx,   opADDy,   opADDz,   opADDw,
	opADDA,  opADDAi,  opADDAq,  opADDAx,  opADDAy,  opADDAz,  opADDAw,
	opSUB,   opSUBi,   opSUBq,   opSUBx,   opSUBy,   opSUBz,   opSUBw,
	opSUBA,  opSUBAi,  opSUBAq,  opSUBAx,  opSUBAy,  opSUBAz,  opSUBAw,
	opMUL,   opMULi,   opMULq,   opMULx,   opMULy,   opMULz,   opMULw,
	opMULA,  opMULAi,  opMULAq,  opMULAx,  opMULAy,  opMULAz,  opMULAw,
	opMADD,  opMADDi,  opMADDq,  opMADDx,  opMADDy,  opMADDz,  opMADDw,
	opMADDA, opMADDAi, opMADDAq, opMADDAx, opMADDAy, opMADDAz, opMADDAw,
	opMSUB,  opMSUBi,  opMSUBq,  opMSUBx,  opMSUBy,  opMSUBz,  opMSUBw,
	opMSUBA, opMSUBAi, opMSUBAq, opMSUBAx, opMSUBAy, opMSUBAz, opMSUBAw,
	opMAX,   opMAXi,             opMAXx,   opMAXy,    opMAXz,  opMAXw,
	opMINI,  opMINIi,            opMINIx,  opMINIy,   opMINIz, opMINIw,
	opFTOI0, opFTOI4, opFTOI12, opFTOI15,
	opITOF0, opITOF4, opITOF12, opITOF15,
	// Lower Instructions
	opDIV, opSQRT, opRSQRT,
	opIADD, opIADDI, opIADDIU,
	opIAND, opIOR,
	opISUB, opISUBIU,
	opMOVE, opMFIR, opMTIR, opMR32, opMFP,
	opLQ, opLQD, opLQI,
	opSQ, opSQD, opSQI,
	opILW, opISW, opILWR, opISWR,
	opRINIT, opRGET, opRNEXT, opRXOR,
	opWAITQ, opWAITP,
	opFSAND, opFSEQ, opFSOR, opFSSET,
	opFMAND, opFMEQ, opFMOR,
	opFCAND, opFCEQ, opFCOR, opFCSET, opFCGET,
	opIBEQ, opIBGEZ, opIBGTZ, opIBLTZ, opIBLEZ, opIBNE,
	opB, opBAL, opJR, opJALR,
	opESADD, opERSADD, opELENG, opERLENG,
	opEATANxy, opEATANxz, opESUM, opERCPR,
	opESQRT, opERSQRT, opESIN, opEATAN,
	opEEXP, opXITOP, opXTOP, opXGKICK,
	opLastOpcode
};

static const char microOpcodeName[][16] = {
	// Upper Instructions
	"ABS", "CLIP", "OPMULA", "OPMSUB", "NOP",
	"ADD",   "ADDi",   "ADDq",   "ADDx",   "ADDy",   "ADDz",   "ADDw",
	"ADDA",  "ADDAi",  "ADDAq",  "ADDAx",  "ADDAy",  "ADDAz",  "ADDAw",
	"SUB",   "SUBi",   "SUBq",   "SUBx",   "SUBy",   "SUBz",   "SUBw",
	"SUBA",  "SUBAi",  "SUBAq",  "SUBAx",  "SUBAy",  "SUBAz",  "SUBAw",
	"MUL",   "MULi",   "MULq",   "MULx",   "MULy",   "MULz",   "MULw",
	"MULA",  "MULAi",  "MULAq",  "MULAx",  "MULAy",  "MULAz",  "MULAw",
	"MADD",  "MADDi",  "MADDq",  "MADDx",  "MADDy",  "MADDz",  "MADDw",
	"MADDA", "MADDAi", "MADDAq", "MADDAx", "MADDAy", "MADDAz", "MADDAw",
	"MSUB",  "MSUBi",  "MSUBq",  "MSUBx",  "MSUBy",  "MSUBz",  "MSUBw",
	"MSUBA", "MSUBAi", "MSUBAq", "MSUBAx", "MSUBAy", "MSUBAz", "MSUBAw",
	"MAX",   "MAXi",             "MAXx",   "MAXy",   "MAXz",   "MAXw",
	"MINI",  "MINIi",            "MINIx",  "MINIy",  "MINIz",  "MINIw",
	"FTOI0", "FTOI4", "FTOI12", "FTOI15",
	"ITOF0", "ITOF4", "ITOF12", "ITOF15",
	// Lower Instructions
	"DIV", "SQRT", "RSQRT",
	"IADD", "IADDI", "IADDIU",
	"IAND", "IOR",
	"ISUB", "ISUBIU",
	"MOVE", "MFIR", "MTIR", "MR32", "MFP",
	"LQ", "LQD", "LQI",
	"SQ", "SQD", "SQI",
	"ILW", "ISW", "ILWR", "ISWR",
	"RINIT", "RGET", "RNEXT", "RXOR",
	"WAITQ", "WAITP",
	"FSAND", "FSEQ", "FSOR", "FSSET",
	"FMAND", "FMEQ", "FMOR",
	"FCAND", "FCEQ", "FCOR", "FCSET", "FCGET",
	"IBEQ", "IBGEZ", "IBGTZ", "IBLTZ", "IBLEZ", "IBNE",
	"B", "BAL", "JR", "JALR",
	"ESADD", "ERSADD", "ELENG", "ERLENG",
	"EATANxy", "EATANxz", "ESUM", "ERCPR",
	"ESQRT", "ERSQRT", "ESIN", "EATAN",
	"EEXP", "XITOP", "XTOP", "XGKICK"
};



struct microProfiler
{
	u64 opStats[opLastOpcode];
	u64 executeCalls = 0;
	u64 searchCalls = 0;
	u64 searchQuickHits = 0;
	u64 searchListHits = 0;
	u64 searchMisses = 0;
	u64 blockFetches = 0;
	u64 blockHits = 0;
	u64 blockMisses = 0;
	u64 compiles = 0;
	u64 compileJitCalls = 0;
	u64 jumpCacheHits = 0;
	u64 jumpCacheMisses = 0;
	u64 createPrograms = 0;
	u64 retiredPrograms = 0;
	u64 clears = 0;
	u64 resets = 0;
	u64 compiledInstructions = 0;
	u64 compiledBytes = 0;
	int index;

	void Reset(int _index)
	{
		std::memset(opStats, 0, sizeof(opStats));
		index = _index;
		ResetStats();
	}

	bool IsVu1ProfilerEnabled() const
	{
		return index == 1 && emucorex::android::profiler::IsEnabled();
	}

	void ResetStats()
	{
		executeCalls = 0;
		searchCalls = 0;
		searchQuickHits = 0;
		searchListHits = 0;
		searchMisses = 0;
		blockFetches = 0;
		blockHits = 0;
		blockMisses = 0;
		compiles = 0;
		compileJitCalls = 0;
		jumpCacheHits = 0;
		jumpCacheMisses = 0;
		createPrograms = 0;
		retiredPrograms = 0;
		clears = 0;
		resets = 0;
		compiledInstructions = 0;
		compiledBytes = 0;
	}

	void Add(u64& counter, u64 amount = 1)
	{
		if (IsVu1ProfilerEnabled())
			counter += amount;
	}

	void EmitOp(microOpcode op)
	{
		if constexpr (!mVUProfileOpcodes)
			return;

		if (!IsVu1ProfilerEnabled() || op == opNOP || !oakHasBlock())
			return;

		recBeginOaknutEmit();
		oakMoveAddressToReg(OAK_XSCRATCH2, &opStats[op]);
		oakAsm->LDR(OAK_XSCRATCH, OAK_XSCRATCH2);
		oakAsm->ADD(OAK_XSCRATCH, OAK_XSCRATCH, 1);
		oakAsm->STR(OAK_XSCRATCH, OAK_XSCRATCH2);
		recEndOaknutEmit();
	}

	__fi void Print() {}

	std::string GetStatsAndReset()
	{
		if constexpr (!mVUProfileOpcodes)
			return {};

		std::array<std::pair<u64, u32>, opLastOpcode> ordered;
		u64 total = 0;
		for (u32 i = 0; i < opLastOpcode; i++)
		{
			const u64 count = opStats[i];
			opStats[i] = 0;
			total += count;
			ordered[i] = {count, i};
		}

		if (total == 0)
			return {};

		std::sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) {
			return lhs.first > rhs.first;
		});

		std::string out;
		char line[512];
		std::snprintf(line, sizeof(line), "VUOps total=%llu",
			static_cast<unsigned long long>(total));
		out += line;

		for (u32 i = 0; i < 6 && i < ordered.size(); i++)
		{
			const u64 count = ordered[i].first;
			if (count == 0)
				break;

			const double percent = (static_cast<double>(count) * 100.0) / static_cast<double>(total);
			std::snprintf(line, sizeof(line), " %s=%llu/%.1f%%",
				microOpcodeName[ordered[i].second],
				static_cast<unsigned long long>(count),
				percent);
			out += line;
		}
		return out;
	}

	std::string GetJitStatsAndReset()
	{
		auto take = [](u64& counter) {
			const u64 value = counter;
			counter = 0;
			return value;
		};

		const u64 execute = take(executeCalls);
		const u64 searches = take(searchCalls);
		const u64 quick_hits = take(searchQuickHits);
		const u64 list_hits = take(searchListHits);
		const u64 misses = take(searchMisses);
		const u64 fetches = take(blockFetches);
		const u64 hits = take(blockHits);
		const u64 block_misses = take(blockMisses);
		const u64 compile_count = take(compiles);
		const u64 jit_calls = take(compileJitCalls);
		const u64 jump_hits = take(jumpCacheHits);
		const u64 jump_misses = take(jumpCacheMisses);
		const u64 programs = take(createPrograms);
		const u64 retired = take(retiredPrograms);
		const u64 clear_count = take(clears);
		const u64 reset_count = take(resets);
		const u64 instructions = take(compiledInstructions);
		const u64 bytes = take(compiledBytes);
		const std::string op_stats = GetStatsAndReset();

		if ((execute | searches | fetches | compile_count | jit_calls | programs | clear_count | reset_count | instructions | bytes) == 0 && op_stats.empty())
			return {};

		char line[1024];
		std::snprintf(line, sizeof(line),
			"VU1JIT exec=%llu search=%llu quick_hit=%llu list_hit=%llu prog_miss=%llu "
			"fetch=%llu block_hit=%llu block_miss=%llu compile=%llu jit_call=%llu "
			"jump_hit=%llu jump_miss=%llu create_prog=%llu retired_prog=%llu clear=%llu reset=%llu "
			"compiled_ins=%llu compiled_bytes=%llu",
			static_cast<unsigned long long>(execute),
			static_cast<unsigned long long>(searches),
			static_cast<unsigned long long>(quick_hits),
			static_cast<unsigned long long>(list_hits),
			static_cast<unsigned long long>(misses),
			static_cast<unsigned long long>(fetches),
			static_cast<unsigned long long>(hits),
			static_cast<unsigned long long>(block_misses),
			static_cast<unsigned long long>(compile_count),
			static_cast<unsigned long long>(jit_calls),
			static_cast<unsigned long long>(jump_hits),
			static_cast<unsigned long long>(jump_misses),
			static_cast<unsigned long long>(programs),
			static_cast<unsigned long long>(retired),
			static_cast<unsigned long long>(clear_count),
			static_cast<unsigned long long>(reset_count),
			static_cast<unsigned long long>(instructions),
			static_cast<unsigned long long>(bytes));

		std::string out(line);
		if (!op_stats.empty())
		{
			out += ' ';
			out += op_stats;
		}
		return out;
	}
};
