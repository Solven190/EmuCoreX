// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "common/Pcsx2Defs.h"

// Keep my nice alignment please!
#define MOVZ MOVZtemp
#define MOVN MOVNtemp

enum class eeOpcode
{
	// Core
	special , regimm , J    , JAL   , BEQ  , BNE  , BLEZ  , BGTZ  ,
	ADDI    , ADDIU  , SLTI , SLTIU , ANDI , ORI  , XORI  , LUI   ,
	cop0    , cop1   , cop2 , /*,*/   BEQL , BNEL , BLEZL , BGTZL ,
	DADDI   , DADDIU , LDL  , LDR   , mmi  , /*,*/  LQ    , SQ    ,
	LB      , LH     , LWL  , LW    , LBU  , LHU  , LWR   , LWU   ,
	SB      , SH     , SWL  , SW    , SDL  , SDR  , SWR   , CACHE ,
	/*,*/     LWC1   , /*,*/  PREF  , /*,*/  /*,*/  LQC2  , LD    ,
	/*,*/     SWC1   , /*,*/  /*,*/   /*,*/  /*,*/  SQC2  , SD    ,

	// Special
	SLL  , /*,*/   SRL  , SRA  , SLLV    , /*,*/   SRLV   , SRAV   ,
	JR   , JALR  , MOVZ , MOVN , SYSCALL , BREAK , /*,*/    SYNC   ,
	MFHI , MTHI  , MFLO , MTLO , DSLLV   , /*,*/   DSRLV  , DSRAV  ,
	MULT , MULTU , DIV  , DIVU , /*,*/     /*,*/   /*,*/    /*,*/
	ADD  , ADDU  , SUB  , SUBU , AND     , OR    , XOR    , NOR    ,
	MFSA , MTSA  , SLT  , SLTU , DADD    , DADDU , DSUB   , DSUBU  ,
	TGE  , TGEU  , TLT  , TLTU , TEQ     , /*,*/   TNE    , /*,*/
	DSLL , /*,*/   DSRL , DSRA , DSLL32  , /*,*/   DSRL32 , DSRA32 ,

	// Regimm
	BLTZ   , BGEZ   , BLTZL   , BGEZL   , /*,*/  /*,*/  /*,*/  /*,*/
	TGEI   , TGEIU  , TLTI    , TLTIU   , TEQI , /*,*/  TNEI , /*,*/
	BLTZAL , BGEZAL , BLTZALL , BGEZALL , /*,*/  /*,*/  /*,*/  /*,*/
	MTSAB  , MTSAH  , /*,*/     /*,*/     /*,*/  /*,*/  /*,*/  /*,*/

	// MMI
	MADD  , MADDU  , /*,*/   /*,*/   PLZCW , /*,*/  /*,*/   /*,*/
	MMI0  , MMI2   , /*,*/   /*,*/   /*,*/   /*,*/  /*,*/   /*,*/
	MFHI1 , MTHI1  , MFLO1 , MTLO1 , /*,*/   /*,*/  /*,*/   /*,*/
	MULT1 , MULTU1 , DIV1  , DIVU1 , /*,*/   /*,*/  /*,*/   /*,*/
	MADD1 , MADDU1 , /*,*/   /*,*/   /*,*/   /*,*/  /*,*/   /*,*/
	MMI1  , MMI3   , /*,*/   /*,*/   /*,*/   /*,*/  /*,*/   /*,*/
	PMFHL , PMTHL  , /*,*/   /*,*/   PSLLH , /*,*/  PSRLH , PSRAH ,
	/*,*/   /*,*/    /*,*/   /*,*/   PSLLW , /*,*/  PSRLW , PSRAW ,

	// MMI0
	PADDW  , PSUBW  , PCGTW  , PMAXW ,
	PADDH  , PSUBH  , PCGTH  , PMAXH ,
	PADDB  , PSUBB  , PCGTB  , /*,*/
	/*,*/    /*,*/    /*,*/    /*,*/
	PADDSW , PSUBSW , PEXTLW , PPACW ,
	PADDSH , PSUBSH , PEXTLH , PPACH ,
	PADDSB , PSUBSB , PEXTLB , PPACB ,
	/*,*/    /*,*/    PEXT5  , PPAC5 ,

	// MMI1
	/*,*/    PABSW  , PCEQW  , PMINW ,
	PADSBH , PABSH  , PCEQH  , PMINH ,
	/*,*/    /*,*/    PCEQB  , /*,*/
	/*,*/    /*,*/    /*,*/    /*,*/
	PADDUW , PSUBUW , PEXTUW , /*,*/
	PADDUH , PSUBUH , PEXTUH , /*,*/
	PADDUB , PSUBUB , PEXTUB , QFSRV ,
	/*,*/    /*,*/    /*,*/    /*,*/

	// MMI2
	PMADDW , /*,*/    PSLLVW , PSRLVW ,
	PMSUBW , /*,*/    /*,*/    /*,*/
	PMFHI  , PMFLO  , PINTH  , /*,*/
	PMULTW , PDIVW  , PCPYLD , /*,*/
	PMADDH , PHMADH , PAND   , PXOR   ,
	PMSUBH , PHMSBH , /*,*/    /*,*/
	/*,*/    /*,*/    PEXEH  , PREVH  ,
	PMULTH , PDIVBW , PEXEW  , PROT3W ,

	// MMI3
	PMADDUW , /*,*/    /*,*/    PSRAVW ,
	/*,*/     /*,*/    /*,*/    /*,*/
	PMTHI   , PMTLO  , PINTEH , /*,*/
	PMULTUW , PDIVUW , PCPYUD , /*,*/
	/*,*/     /*,*/    POR    , PNOR   ,
	/*,*/     /*,*/    /*,*/    /*,*/
	/*,*/     /*,*/    PEXCH  , PCPYH  ,
	/*,*/     /*,*/    PEXCW  , /*,*/

	// ADD COP0 ??

	// "COP1"
	MFC1   , /*,*/    CFC1   , /*,*/   MTC1   , /*,*/    CTC1    , /*,*/

	// "COP1 BC1"
	BC1F   , BC1T   , BC1FL  , BC1TL , /*,*/    /*,*/    /*,*/     /*,*/

	// "COP1 S"
	ADD_F  , SUB_F  , MUL_F  , DIV_F , SQRT_F , ABS_F  , MOV_F   , NEG_F   ,
	/*,*/    /*,*/    /*,*/    /*,*/   /*,*/    /*,*/    /*,*/     /*,*/
	/*,*/    /*,*/    /*,*/    /*,*/   /*,*/    /*,*/    RSQRT_F , /*,*/
	ADDA_F , SUBA_F , MULA_F , /*,*/   MADD_F , MSUB_F , MADDA_F , MSUBA_F ,
	/*,*/    /*,*/    /*,*/    /*,*/   CVTW   , /*,*/    /*,*/     /*,*/
	MAX_F  , MIN_F  , /*,*/    /*,*/   /*,*/    /*,*/    /*,*/     /*,*/
	CF_F,    /*,*/    CEQ_F  , /*,*/   CLT_F  , /*,*/    CLE_F   , /*,*/

	// "COP1 W"
	CVTS_F,  /*,*/    /*,*/    /*,*/   /*,*/    /*,*/    /*,*/     /*,*/

	LAST
};

#undef MOVZ
#undef MOVN


struct eeProfiler
{
	__fi void Reset() {}
	__fi void EmitOp(eeOpcode op) {}
	__fi void Print() {}
	__fi void EmitMem() {}
	__fi void EmitConstMem(u32 add) {}
	__fi void EmitSlowMem() {}
	__fi void EmitFastMem() {}
};

namespace EE
{
	extern eeProfiler Profiler;
}
