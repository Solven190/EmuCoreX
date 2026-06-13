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
					
					std::stringstream pc_ss;
					pc_ss << "0x" << std::hex << std::setfill('0') << std::setw(8) << p.startpc;
					
					out << std::left << std::setw(8) << type_str
					    << std::setw(12) << pc_ss.str()
					    << std::setw(15) << p.execution_count
					    << std::setw(12) << p.size
					    << std::setw(12) << p.host_size
					    << std::setw(18) << (p.size * p.execution_count)
					    << std::fixed << std::setprecision(2) << ratio << "\n";
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
