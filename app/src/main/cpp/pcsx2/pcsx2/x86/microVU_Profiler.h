// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

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



struct microProfiler
{
	__fi void Reset(int _index) {}
	__fi void EmitOp(microOpcode op) {}
	__fi void Print() {}
};
