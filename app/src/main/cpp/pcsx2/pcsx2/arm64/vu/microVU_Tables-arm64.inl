// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

//------------------------------------------------------------------
// Declarations
//------------------------------------------------------------------
mVUop(mVU_UPPER_FD_00);
mVUop(mVU_UPPER_FD_01);
mVUop(mVU_UPPER_FD_10);
mVUop(mVU_UPPER_FD_11);
mVUop(mVULowerOP);
mVUop(mVULowerOP_T3_00);
mVUop(mVULowerOP_T3_01);
mVUop(mVULowerOP_T3_10);
mVUop(mVULowerOP_T3_11);
mVUop(mVUunknown);
//------------------------------------------------------------------

//------------------------------------------------------------------
// Opcode Tables
//------------------------------------------------------------------
static const Fnptr_mVUrecInst mVULOWER_OPCODE[128] = {
	mVU_LQ_emit     , mVU_SQ_emit     , mVUunknown , mVUunknown,
	mVU_ILW_emit    , mVU_ISW_emit    , mVUunknown , mVUunknown,
	mVU_IADDIU_emit , mVU_ISUBIU_emit , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVU_FCEQ_emit   , mVU_FCSET_emit  , mVU_FCAND_emit  , mVU_FCOR_emit,
	mVU_FSEQ_emit   , mVU_FSSET_emit  , mVU_FSAND_emit  , mVU_FSOR_emit,
	mVU_FMEQ_emit   , mVUunknown , mVU_FMAND_emit  , mVU_FMOR_emit,
	mVU_FCGET_emit  , mVUunknown , mVUunknown , mVUunknown,
	mVU_B_emit      , mVU_BAL_emit    , mVUunknown , mVUunknown,
	mVU_JR_emit     , mVU_JALR_emit   , mVUunknown , mVUunknown,
	mVU_IBEQ_emit   , mVU_IBNE_emit   , mVUunknown , mVUunknown,
	mVU_IBLTZ_emit  , mVU_IBGTZ_emit  , mVU_IBLEZ_emit  , mVU_IBGEZ_emit,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVULowerOP , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
};

static const Fnptr_mVUrecInst mVULowerOP_T3_00_OPCODE[32] = {
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVU_MOVE_emit   , mVU_LQI_emit    , mVU_DIV_emit    , mVU_MTIR_emit,
	mVU_RNEXT_emit  , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVU_MFP_emit    , mVU_XTOP_emit   , mVU_XGKICK_emit,
	mVU_ESADD_emit  , mVU_EATANxy_emit, mVU_ESQRT_emit  , mVU_ESIN_emit,
};

static const Fnptr_mVUrecInst mVULowerOP_T3_01_OPCODE[32] = {
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVU_MR32_emit   , mVU_SQI_emit    , mVU_SQRT_emit   , mVU_MFIR_emit,
	mVU_RGET_emit   , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVU_XITOP_emit  , mVUunknown,
	mVU_ERSADD_emit , mVU_EATANxz_emit, mVU_ERSQRT_emit , mVU_EATAN_emit,
};

static const Fnptr_mVUrecInst mVULowerOP_T3_10_OPCODE[32] = {
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVU_LQD_emit    , mVU_RSQRT_emit  , mVU_ILWR_emit,
	mVU_RINIT_emit  , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVU_ELENG_emit  , mVU_ESUM_emit   , mVU_ERCPR_emit  , mVU_EEXP_emit,
};

const Fnptr_mVUrecInst mVULowerOP_T3_11_OPCODE [32] = {
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVU_SQD_emit    , mVU_WAITQ_emit  , mVU_ISWR_emit,
	mVU_RXOR_emit   , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVU_ERLENG_emit , mVUunknown , mVU_WAITP_emit  , mVUunknown,
};

static const Fnptr_mVUrecInst mVULowerOP_OPCODE[64] = {
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVU_IADD_emit   , mVU_ISUB_emit   , mVU_IADDI_emit  , mVUunknown,
	mVU_IAND_emit   , mVU_IOR_emit    , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVULowerOP_T3_00, mVULowerOP_T3_01, mVULowerOP_T3_10, mVULowerOP_T3_11,
};

static const Fnptr_mVUrecInst mVU_UPPER_OPCODE[64] = {
	mVU_ADDx_emit   , mVU_ADDy_emit   , mVU_ADDz_emit   , mVU_ADDw_emit,
	mVU_SUBx_emit   , mVU_SUBy_emit   , mVU_SUBz_emit   , mVU_SUBw_emit,
	mVU_MADDx_emit  , mVU_MADDy_emit  , mVU_MADDz_emit  , mVU_MADDw_emit,
	mVU_MSUBx_emit  , mVU_MSUBy_emit  , mVU_MSUBz_emit  , mVU_MSUBw_emit,
	mVU_MAXx_emit   , mVU_MAXy_emit   , mVU_MAXz_emit   , mVU_MAXw_emit,
	mVU_MINIx_emit  , mVU_MINIy_emit  , mVU_MINIz_emit  , mVU_MINIw_emit,
	mVU_MULx_emit   , mVU_MULy_emit   , mVU_MULz_emit   , mVU_MULw_emit,
	mVU_MULq_emit   , mVU_MAXi_emit   , mVU_MULi_emit   , mVU_MINIi_emit,
	mVU_ADDq_emit   , mVU_MADDq_emit  , mVU_ADDi_emit   , mVU_MADDi_emit,
	mVU_SUBq_emit   , mVU_MSUBq_emit  , mVU_SUBi_emit   , mVU_MSUBi_emit,
	mVU_ADD_emit    , mVU_MADD_emit   , mVU_MUL_emit    , mVU_MAX_emit,
	mVU_SUB_emit    , mVU_MSUB_emit   , mVU_OPMSUB_emit , mVU_MINI_emit,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown  , mVUunknown , mVUunknown,
	mVU_UPPER_FD_00, mVU_UPPER_FD_01, mVU_UPPER_FD_10, mVU_UPPER_FD_11,
};

static const Fnptr_mVUrecInst mVU_UPPER_FD_00_TABLE [32] = {
	mVU_ADDAx_emit  , mVU_SUBAx_emit  , mVU_MADDAx_emit , mVU_MSUBAx_emit,
	mVU_ITOF0_emit  , mVU_FTOI0_emit  , mVU_MULAx_emit  , mVU_MULAq_emit,
	mVU_ADDAq_emit  , mVU_SUBAq_emit  , mVU_ADDA_emit   , mVU_SUBA_emit,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
};

static const Fnptr_mVUrecInst mVU_UPPER_FD_01_TABLE [32] = {
	mVU_ADDAy_emit  , mVU_SUBAy_emit  , mVU_MADDAy_emit , mVU_MSUBAy_emit,
	mVU_ITOF4_emit  , mVU_FTOI4_emit  , mVU_MULAy_emit  , mVU_ABS_emit,
	mVU_MADDAq_emit , mVU_MSUBAq_emit , mVU_MADDA_emit  , mVU_MSUBA_emit,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
};

static const Fnptr_mVUrecInst mVU_UPPER_FD_10_TABLE [32] = {
	mVU_ADDAz_emit  , mVU_SUBAz_emit  , mVU_MADDAz_emit , mVU_MSUBAz_emit,
	mVU_ITOF12_emit , mVU_FTOI12_emit , mVU_MULAz_emit  , mVU_MULAi_emit,
	mVU_ADDAi_emit  , mVU_SUBAi_emit  , mVU_MULA_emit   , mVU_OPMULA_emit,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
};

static const Fnptr_mVUrecInst mVU_UPPER_FD_11_TABLE [32] = {
	mVU_ADDAw_emit  , mVU_SUBAw_emit  , mVU_MADDAw_emit , mVU_MSUBAw_emit,
	mVU_ITOF15_emit , mVU_FTOI15_emit , mVU_MULAw_emit  , mVU_CLIP_emit,
	mVU_MADDAi_emit , mVU_MSUBAi_emit , mVUunknown , mVU_NOP_emit,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
};


//------------------------------------------------------------------
// Table Functions
//------------------------------------------------------------------

mVUop(mVU_UPPER_FD_00)  { mVU_UPPER_FD_00_TABLE   [((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVU_UPPER_FD_01)  { mVU_UPPER_FD_01_TABLE   [((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVU_UPPER_FD_10)  { mVU_UPPER_FD_10_TABLE   [((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVU_UPPER_FD_11)  { mVU_UPPER_FD_11_TABLE   [((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVULowerOP)       { mVULowerOP_OPCODE       [ (mVU.code & 0x3f) ](mX); }
mVUop(mVULowerOP_T3_00) { mVULowerOP_T3_00_OPCODE [((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVULowerOP_T3_01) { mVULowerOP_T3_01_OPCODE [((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVULowerOP_T3_10) { mVULowerOP_T3_10_OPCODE [((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVULowerOP_T3_11) { mVULowerOP_T3_11_OPCODE [((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVUopU)           { mVU_UPPER_OPCODE        [ (mVU.code & 0x3f) ](mX); } // Gets Upper Opcode
mVUop(mVUopL)           { mVULOWER_OPCODE         [ (mVU.code >>  25) ](mX); } // Gets Lower Opcode
mVUop(mVUunknown)
{
	pass1
	{
		if (mVU.code != 0x8000033c)
			mVUinfo.isBadOp = true;
	}
	pass2
	{
		if (mVU.code != 0x8000033c)
			Console.Error("microVU%d: Unknown Micro VU opcode called (%x) [%04x]\n", getIndex, mVU.code, xPC);
	}
	pass3 { mVUlog("Unknown", mVU.code); }
}
