#pragma once
#include "cafe/kernel/cafe_kernel_mmu.h"
#include "cafe/kernel/cafe_kernel_processid.h"
#include "cafe/loader/cafe_loader_init.h"
#include "ios/mcp/ios_mcp_mcp_types.h"

#include <array>
#include <libcpu/be2_struct.h>
#include <string_view>

namespace cafe::loader
{
struct RPL_STARTINFO;
};

namespace cafe::kernel
{

struct ProcessPerCoreStartInfo
{
   UNKNOWN(0x8);
   be2_val<virt_addr> stackBase;
   be2_val<virt_addr> stackEnd;
   be2_val<uint32_t> sda2Base;
   be2_val<uint32_t> sdaBase;
   UNKNOWN(0x4);
   be2_val<uint32_t> unk0x1C;
   be2_val<virt_addr> entryPoint;
   be2_val<virt_addr> exceptionStackBase;
   be2_val<virt_addr> exceptionStackEnd;
};
CHECK_OFFSET(ProcessPerCoreStartInfo, 0x08, stackBase);
CHECK_OFFSET(ProcessPerCoreStartInfo, 0x0C, stackEnd);
CHECK_OFFSET(ProcessPerCoreStartInfo, 0x10, sda2Base);
CHECK_OFFSET(ProcessPerCoreStartInfo, 0x14, sdaBase);
CHECK_OFFSET(ProcessPerCoreStartInfo, 0x1C, unk0x1C);
CHECK_OFFSET(ProcessPerCoreStartInfo, 0x20, entryPoint);
CHECK_OFFSET(ProcessPerCoreStartInfo, 0x24, exceptionStackBase);
CHECK_OFFSET(ProcessPerCoreStartInfo, 0x28, exceptionStackEnd);
CHECK_SIZE(ProcessPerCoreStartInfo, 0x2C);

struct RamPartitionAllocation
{
   phys_addr dataStart;
   phys_addr availStart;
   phys_addr codeGenStart;
   phys_addr codeStart;
   phys_addr codeEnd;
   uint32_t codegen_core;
   uint32_t unk0x18;
   uint32_t unk0x1C;
};

struct ProcessData
{
   loader::RPL_STARTINFO startInfo;
   std::array<ProcessPerCoreStartInfo, 3> perCoreStartInfo;
   RamPartitionAllocation ramPartitionAllocation;
   internal::AddressSpace addressSpace;
   ios::mcp::MCPPPrepareTitleInfo titleInfo;
   bool overlayArenaEnabled;
   std::string argstr;
};

void
initialiseRamPartitionConfig();

ProcessData *
getCurrentProcessData();

RamPartitionId
getCurrentRampid();

UniqueProcessId
getCurrentUpid();

RamPartitionId
getRampidFromUpid(UniqueProcessId id);

ProcessPerCoreStartInfo *
getProcessPerCoreStartInfo(uint32_t id);

loader::RPL_STARTINFO *
getProcessStartInfo();

void
loadGameProcess(std::string_view rpx,
                ios::mcp::MCPPPrepareTitleInfo &titleInfo);

void
finishInitAndPreload();

} // namespace cafe::kernel
