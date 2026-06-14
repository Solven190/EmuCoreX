#include "JitProfiler.h"
#include "arm64/OaknutHelpers.h"
#include "Host.h"
#include "VMManager.h"
#include "common/Path.h"
#include "common/FileSystem.h"
#include "Config.h"
#include "R5900.h"
#include "R3000A.h"
#include "VUmicro.h"
#include "vtlb.h"
#include "IopMem.h"
#include "arm64/cpuRegistersPack.h"
#include "DebugTools/Debug.h"
#include "MemoryTypes.h"
#include <atomic>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

extern void EE_JitGetBlockProfiles(std::vector<JitBlockProfile>& outBlocks);
extern void IOP_JitGetBlockProfiles(std::vector<JitBlockProfile>& outBlocks);
extern void VU0_JitGetBlockProfiles(std::vector<JitBlockProfile>& outBlocks);
extern void VU1_JitGetBlockProfiles(std::vector<JitBlockProfile>& outBlocks);

namespace JitProfiler
{
	static std::atomic<bool> s_active{false};

	static bool readEEMemory(u32 address, void* dest, u32 size)
	{
		if (vtlb_memSafeReadBytes(address, dest, size))
			return true;

		u32 paddr = address & 0x1fffffff;
		if (paddr + size <= Ps2MemSize::TotalRam)
		{
			if (eeMem)
			{
				std::memcpy(dest, &eeMem->Main[paddr], size);
				return true;
			}
		}

		if (address >= 0x70000000 && address < 0x70004000)
		{
			u32 offset = address & 0x3fff;
			if (offset + size <= Ps2MemSize::Scratch)
			{
				if (eeMem)
				{
					std::memcpy(dest, &eeMem->Scratch[offset], size);
					return true;
				}
			}
		}

		return false;
	}

	bool IsActive()
	{
		return s_active.load(std::memory_order_relaxed);
	}

	void EmitBlockIncrement(void* counter_ptr)
	{
		if (!oakAsm) return;
		
		// Load the 64-bit address of the counter into X16 (scratch register)
		oakMoveAddressToReg(oak::util::X16, counter_ptr);
		
		// Load the 64-bit value from [X16] into X17
		oakAsm->LDR(oak::util::X17, oak::util::X16);
		
		// Increment X17
		oakAsm->ADD(oak::util::X17, oak::util::X17, 1);
		
		// Store X17 back to [X16]
		oakAsm->STR(oak::util::X17, oak::util::X16);
	}

	void Start()
	{
		if (s_active.exchange(true))
			return; // already active
			
		Host::RunOnCPUThread([]() {
			recCpu.Reset();
			psxRec.Reset();
			CpuMicroVU0.Reset();
			CpuMicroVU1.Reset();
		}, true); // block until CPU thread executes it
	}

	void Stop()
	{
		if (!s_active.exchange(false))
			return; // already stopped
			
		Host::RunOnCPUThread([]() {
			std::vector<JitBlockProfile> profiles;
			EE_JitGetBlockProfiles(profiles);
			IOP_JitGetBlockProfiles(profiles);
			VU0_JitGetBlockProfiles(profiles);
			VU1_JitGetBlockProfiles(profiles);
			
			std::string filepath = Path::Combine(EmuFolders::DataRoot, "jit_profile.txt");
			std::ofstream out(filepath);
			if (out.is_open())
			{
				out << "=========================================\n";
				out << "        EmuCoreX JIT Profiler Report     \n";
				out << "=========================================\n\n";
				
				u64 total_ee_blocks = 0, total_iop_blocks = 0, total_vu0_blocks = 0, total_vu1_blocks = 0;
				u64 ee_exec = 0, iop_exec = 0, vu0_exec = 0, vu1_exec = 0;
				u64 ee_guest_instrs = 0, iop_guest_instrs = 0, vu0_guest_instrs = 0, vu1_guest_instrs = 0;
				u64 ee_host_bytes = 0, iop_host_bytes = 0, vu0_host_bytes = 0, vu1_host_bytes = 0;
				
				for (const auto& p : profiles)
				{
					if (p.type == 0) {
						total_ee_blocks++;
						ee_exec += p.execution_count;
						ee_guest_instrs += p.size * p.execution_count;
						ee_host_bytes += p.host_size;
					} else if (p.type == 1) {
						total_iop_blocks++;
						iop_exec += p.execution_count;
						iop_guest_instrs += p.size * p.execution_count;
						iop_host_bytes += p.host_size;
					} else if (p.type == 2) {
						total_vu0_blocks++;
						vu0_exec += p.execution_count;
						vu0_guest_instrs += p.size * p.execution_count;
						vu0_host_bytes += p.host_size;
					} else if (p.type == 3) {
						total_vu1_blocks++;
						vu1_exec += p.execution_count;
						vu1_guest_instrs += p.size * p.execution_count;
						vu1_host_bytes += p.host_size;
					}
				}
				
				out << "Summary Stats:\n";
				out << "-----------------------------------------\n";
				out << "EE:  Blocks=" << total_ee_blocks << ", Executions=" << ee_exec << ", GuestInstrsExecuted=" << ee_guest_instrs << ", HostCompiledBytes=" << ee_host_bytes << "\n";
				out << "IOP: Blocks=" << total_iop_blocks << ", Executions=" << iop_exec << ", GuestInstrsExecuted=" << iop_guest_instrs << ", HostCompiledBytes=" << iop_host_bytes << "\n";
				out << "VU0: Blocks=" << total_vu0_blocks << ", Executions=" << vu0_exec << ", GuestInstrsExecuted=" << vu0_guest_instrs << ", HostCompiledBytes=" << vu0_host_bytes << "\n";
				out << "VU1: Blocks=" << total_vu1_blocks << ", Executions=" << vu1_exec << ", GuestInstrsExecuted=" << vu1_guest_instrs << ", HostCompiledBytes=" << vu1_host_bytes << "\n\n";
				
				std::sort(profiles.begin(), profiles.end(), [](const JitBlockProfile& a, const JitBlockProfile& b) {
					return (a.size * a.execution_count) > (b.size * b.execution_count);
				});
				
				out << "Hottest JIT Blocks (Top 100):\n";
				out << "---------------------------------------------------------------------------------\n";
				out << std::left << std::setw(8) << "Type" 
				    << std::setw(12) << "Guest PC" 
				    << std::setw(15) << "Exec Count" 
				    << std::setw(12) << "Guest Size" 
				    << std::setw(12) << "Host Size" 
				    << std::setw(18) << "Weight (Count*Sz)" 
				    << "Expansion Ratio\n";
				out << "---------------------------------------------------------------------------------\n";
				
				int count = 0;
				for (const auto& p : profiles)
				{
					if (p.execution_count == 0) continue;
					if (++count > 100) break;
					
					std::string type_str = (p.type == 0) ? "EE" : (p.type == 1) ? "IOP" : (p.type == 2) ? "VU0" : "VU1";
					double ratio = (p.size > 0) ? (double)p.host_size / (p.size * 4) : 0;
					
					char main_line[256];
					std::snprintf(main_line, sizeof(main_line), "%-8s0x%08x  %-15llu%-12u%-12u%-18llu%.2f\n",
						type_str.c_str(), p.startpc, (unsigned long long)p.execution_count, p.size, p.host_size,
						(unsigned long long)(p.size * p.execution_count), ratio);
					out << main_line;

					if (p.type == 0) // EE
					{
						std::vector<u32> instrs(p.size);
						if (readEEMemory(p.startpc, instrs.data(), p.size * 4)) {
							out << "      Guest instructions:\n";
							for (u32 i = 0; i < p.size; i++) {
								u32 pc = p.startpc + i * 4;
								u32 code = instrs[i];
								std::string disasm;
								R5900::disR5900Fasm(disasm, code, pc);
								char buf[256];
								std::snprintf(buf, sizeof(buf), "        0x%08x: 0x%08x  %s\n", pc, code, disasm.c_str());
								out << buf;
							}
						} else {
							out << "      [Failed to read guest memory]\n";
						}
					}
					else if (p.type == 1) // IOP
					{
						std::vector<u32> instrs(p.size);
						if (iopMemSafeReadBytes(p.startpc, instrs.data(), p.size * 4)) {
							out << "      Guest instructions:\n";
							for (u32 i = 0; i < p.size; i++) {
								u32 pc = p.startpc + i * 4;
								u32 code = instrs[i];
								char* disasm = R3000A::disR3000AF(code, pc);
								char buf[256];
								std::snprintf(buf, sizeof(buf), "        0x%08x: 0x%08x  %s\n", pc, code, disasm ? disasm : "");
								out << buf;
							}
						} else {
							out << "      [Failed to read guest memory]\n";
						}
					}
					else if (p.type == 2) // VU0
					{
						if (VU0.Micro) {
							out << "      Guest instructions:\n";
							for (u32 i = 0; i < p.size; i++) {
								u32 offset = p.startpc + i * 8;
								if (offset + 8 <= VU0_PROGSIZE) {
									u32 upper = *(u32*)&VU0.Micro[offset];
									u32 lower = *(u32*)&VU0.Micro[offset + 4];
									char* dis_up = disVU0MicroUF(upper, offset / 8);
									std::string up_str = dis_up ? dis_up : "";
									char* dis_lo = disVU0MicroLF(lower, offset / 8);
									std::string lo_str = dis_lo ? dis_lo : "";
									char buf[512];
									std::snprintf(buf, sizeof(buf), "        %03x: 0x%08x 0x%08x  %s | %s\n",
										offset / 8, upper, lower, up_str.c_str(), lo_str.c_str());
									out << buf;
								}
							}
						} else {
							out << "      [VU0.Micro is null]\n";
						}
					}
					else if (p.type == 3) // VU1
					{
						if (VU1.Micro) {
							out << "      Guest instructions:\n";
							for (u32 i = 0; i < p.size; i++) {
								u32 offset = p.startpc + i * 8;
								if (offset + 8 <= VU1_PROGSIZE) {
									u32 upper = *(u32*)&VU1.Micro[offset];
									u32 lower = *(u32*)&VU1.Micro[offset + 4];
									char* dis_up = disVU1MicroUF(upper, offset / 8);
									std::string up_str = dis_up ? dis_up : "";
									char* dis_lo = disVU1MicroLF(lower, offset / 8);
									std::string lo_str = dis_lo ? dis_lo : "";
									char buf[512];
									std::snprintf(buf, sizeof(buf), "        %03x: 0x%08x 0x%08x  %s | %s\n",
										offset / 8, upper, lower, up_str.c_str(), lo_str.c_str());
									out << buf;
								}
							}
						} else {
							out << "      [VU1.Micro is null]\n";
						}
					}
				}
				
				out.close();
			}
			
			recCpu.Reset();
			psxRec.Reset();
			CpuMicroVU0.Reset();
			CpuMicroVU1.Reset();
		}, true);
	}
}
