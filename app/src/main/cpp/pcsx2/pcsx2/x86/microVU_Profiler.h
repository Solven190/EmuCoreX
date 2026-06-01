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
	int index;

	void Reset(int _index)
	{
		std::memset(opStats, 0, sizeof(opStats));
		index = _index;
	}

	void EmitOp(microOpcode op)
	{
		if constexpr (!mVUProfileOpcodes)
			return;

		if (index != 1 || op == opNOP || !oakHasBlock())
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
};
