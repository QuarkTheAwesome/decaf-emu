#pragma optimize("", off)
#include "debugger/debugger.h"
#include "decaf_events.h"
#include "decaf_config.h"
#include "filesystem/filesystem.h"
#include "kernel.h"
#include "kernel_hle.h"
#include "kernel_loader.h"
#include "kernel_filesystem.h"
#include "ios/ios.h"
#include "cafe/libraries/gx2/gx2_event.h"
#include "modules/sci/sci_cafe_settings.h"
#include "modules/sci/sci_caffeine_settings.h"
#include "modules/sci/sci_parental_account_settings_uc.h"
#include "modules/sci/sci_parental_settings.h"
#include "modules/sci/sci_spot_pass_settings.h"

#include "cafe/libraries/cafe_hle.h"
#include "cafe/libraries/cafe_hle_library.h"
#include "cafe/cafe_ppc_interface_invoke.h"
#include "cafe/loader/cafe_loader_globals.h"
#include "cafe/kernel/cafe_kernel_loader.h"
#include "cafe/kernel/cafe_kernel_exception.h"
#include "cafe/kernel/cafe_kernel_heap.h"
#include "cafe/kernel/cafe_kernel_mmu.h"
#include "ios/mcp/ios_mcp_mcp_types.h"

#include <common/decaf_assert.h>
#include <common/platform_dir.h>
#include <common/platform_fiber.h>
#include <common/platform_thread.h>
#include <common/teenyheap.h>
#include <fmt/format.h>
#include <libcpu/trace.h>
#include <libcpu/mem.h>
#include <pugixml.hpp>

namespace kernel
{

namespace functions
{
void
kcTraceHandler(const std::string& str)
{
   traceLogSyscall(str);
   gLog->debug(str);
}
}

enum class FaultReason : uint32_t {
   Unknown,
   Segfault,
   IllInst
};

static void
cpuEntrypoint(cpu::Core *core);

static void
cpuBranchTraceHandler(cpu::Core *core, uint32_t target);

static void
cpuKernelCallHandler(cpu::Core *core, uint32_t id);

static std::atomic_bool
sRunning { true };

static std::string
sExecutableName;

static loader::LoadedModule *
sUserModule;

static int
sExitCode = 0;

static FaultReason
sFaultReason = FaultReason::Unknown;

static uint32_t
sSegfaultAddress = 0;

static decaf::GameInfo
sGameInfo;

static std::array<const char *, 14>
sSharedLibraries = {
   "coreinit", "tve", "nsysccr", "nsysnet", "uvc", "tcl", "nn_pdm", "dmae",
   "dc", "vpadbase", "vpad", "avm", "gx2", "snd_core"
};

struct PerProcessData
{
   virt_addr stackStart[3];
   virt_addr stackTop[3];
   virt_addr exceptionStackStart[3];
   virt_addr exceptionStackTop[3];
};

static PerProcessData
sProcessData;

static cafe::kernel::internal::AddressSpace
sKernelAddressSpace;

void
setExecutableFilename(const std::string& name)
{
   sExecutableName = name;
}

void
initialise()
{
   cafe::hle::initialiseLibraries();

   // Initialise memory
   cafe::kernel::internal::initialiseAddressSpace(&sKernelAddressSpace,
                                                  cafe::kernel::RamPartitionId::Kernel,
                                                  phys_addr { 0x72000000 }, 0x0E000000,
                                                  phys_addr { 0x20000000 }, 0x52000000,
                                                  0, 0,
                                                  phys_addr { 0 }, 0,
                                                  phys_addr { 0 }, 0,
                                                  0, false);
   cafe::kernel::internal::loadAddressSpace(&sKernelAddressSpace);
   cafe::kernel::internal::initialiseStaticDataHeap();

   // Initialise static data
   cafe::kernel::internal::initialiseStaticContextData();
   cafe::kernel::internal::initialiseStaticExceptionData();

   // Initialise HLE modules
   // initialiseHleModules();

   // Setup cpu
   cpu::setCoreEntrypointHandler(&cpuEntrypoint);
   cafe::kernel::internal::initialiseExceptionHandlers();

   if (decaf::config::log::branch_trace) {
      cpu::setBranchTraceHandler(&cpuBranchTraceHandler);
   }

   cpu::setKernelCallHandler(&cpuKernelCallHandler);
}

void
shutdown()
{
   ios::join();
}

static void
cpuBranchTraceHandler(cpu::Core *core, uint32_t target)
{
   /*auto symNamePtr = loader::findSymbolNameForAddress(target);
   if (!symNamePtr) {
      return;
   }

   gLog->debug("CPU branched to: {}", *symNamePtr);*/
}

static void
cpuKernelCallHandler(cpu::Core *core, uint32_t id)
{
   cafe::hle::Library::handleKernelCall(core, id);
}

static void
coreWfiLoop(cpu::Core *core)
{
   // Set up the default expected state for the nia/cia of idle threads.
   //  This must be kept in sync with reschedule which sets them to this
   //  for debugging purposes.
   core->nia = 0xFFFFFFFF;
   core->cia = 0xFFFFFFFF;

   while (sRunning.load()) {
      cpu::this_core::waitForInterrupt();
   }
}

static void
core1EntryPoint(cpu::Core *core)
{
   /*
   Load coreinit.rpl .text & .data
   Load shared libraries .text
   Load rpx .text -> run __preinit_user
   Load shared libraries .data, load rpx .data
   */
   if (!loadGameInfo(sGameInfo)) {
      gLog->warn("Could not load game info.");
   } else {
      gLog->info("Loaded game info: '{}' - {} v{}", sGameInfo.meta.shortnames[decaf::Language::English], sGameInfo.meta.product_code, sGameInfo.meta.title_version);
   }

   auto rpx = sGameInfo.cos.argstr;

   if (rpx.empty()) {
      rpx = sExecutableName;
   }

   if (rpx.empty()) {
      gLog->error("Could not find game executable to load.");
      return;
   }

   // Load and run the game
   ios::mcp::MCPPPrepareTitleInfo titleInfo;
   titleInfo.version = static_cast<uint32_t>(sGameInfo.cos.version);
   titleInfo.titleId = static_cast<uint64_t>(sGameInfo.app.title_id);
   titleInfo.cmdFlags = static_cast<uint32_t>(sGameInfo.cos.cmdFlags);
   titleInfo.argstr = sGameInfo.cos.argstr;
   titleInfo.max_size = static_cast<uint32_t>(sGameInfo.cos.max_size);
   titleInfo.avail_size = static_cast<uint32_t>(sGameInfo.cos.avail_size);
   titleInfo.codegen_size = static_cast<uint32_t>(sGameInfo.cos.codegen_size);
   titleInfo.codegen_core = static_cast<uint32_t>(sGameInfo.cos.codegen_core);
   titleInfo.max_codesize = static_cast<uint32_t>(sGameInfo.cos.max_codesize);
   titleInfo.overlay_arena = static_cast<uint32_t>(sGameInfo.cos.overlay_arena);
   titleInfo.num_workarea_heap_blocks = static_cast<uint32_t>(sGameInfo.cos.num_workarea_heap_blocks);
   titleInfo.num_codearea_heap_blocks = static_cast<uint32_t>(sGameInfo.cos.num_codearea_heap_blocks);
   titleInfo.default_stack0_size = static_cast<uint32_t>(sGameInfo.cos.default_stack0_size);
   titleInfo.default_stack1_size = static_cast<uint32_t>(sGameInfo.cos.default_stack1_size);
   titleInfo.default_stack2_size = static_cast<uint32_t>(sGameInfo.cos.default_stack2_size);
   titleInfo.default_redzone0_size = static_cast<uint32_t>(sGameInfo.cos.default_redzone0_size);
   titleInfo.default_redzone1_size = static_cast<uint32_t>(sGameInfo.cos.default_redzone1_size);
   titleInfo.default_redzone2_size = static_cast<uint32_t>(sGameInfo.cos.default_redzone2_size);
   titleInfo.exception_stack0_size = static_cast<uint32_t>(sGameInfo.cos.exception_stack0_size);
   titleInfo.exception_stack1_size = static_cast<uint32_t>(sGameInfo.cos.exception_stack1_size);
   titleInfo.exception_stack2_size = static_cast<uint32_t>(sGameInfo.cos.exception_stack2_size);

   for (auto i = 0u; i < titleInfo.permissions.size(); ++i) {
      titleInfo.permissions[i].group = static_cast<uint32_t>(sGameInfo.cos.permissions[i].group);
      titleInfo.permissions[i].mask = static_cast<uint64_t>(sGameInfo.cos.permissions[i].mask);
   }

   titleInfo.sdkVersion = static_cast<uint32_t>(sGameInfo.app.sdk_version);
   titleInfo.titleVersion = static_cast<uint32_t>(sGameInfo.app.title_version);

   cafe::kernel::loadGameProcess(rpx, titleInfo);
   cafe::kernel::finishInitAndPreload();

#if 0
   // Load the default shared libraries, but NOT run their entry points
   auto coreinitModule = loader::loadRPL("coreinit");
   for (auto &name : sSharedLibraries) {
      auto sharedModule = loader::loadRPL(name);

      if (!sharedModule) {
         gLog->warn("Could not load shared library {}", name);
      }
   }

   // Load the application
   auto appModule = loader::loadRPL(rpx);

   if (!appModule) {
      gLog->error("Could not load {}", rpx);
      return;
   }

   gLog->debug("Succesfully loaded {}", rpx);
   sUserModule = appModule;

   // We need to set some default stuff up...
   core->gqr[2].value = 0x40004;
   core->gqr[3].value = 0x50005;
   core->gqr[4].value = 0x60006;
   core->gqr[5].value = 0x70007;

   // Setup coreinit threads
   coreinit::internal::startAlarmCallbackThreads();
   coreinit::internal::startAppIoThreads();
   coreinit::internal::startDefaultCoreThreads();
   coreinit::internal::startDeallocatorThreads();

   // Initialise CafeOS
   coreinit::IPCDriverInit();
   coreinit::IPCDriverOpen();
   coreinit::internal::mcpInit();
   coreinit::internal::loadSharedData();

   // Notify frontend that game has loaded
   decaf::event::onGameLoaded(sGameInfo);

   // Start the entry thread!
   auto gameThreadEntry = coreinitModule->findFuncExport<uint32_t, uint32_t, void*>("GameThreadEntry");
   auto thread = coreinit::OSGetDefaultThread(1);
   thread->entryPoint = gameThreadEntry;
   coreinit::OSResumeThread(thread);
#endif
   // Trip an interrupt to force game thread to run
   cpu::interrupt(1, cpu::GENERIC_INTERRUPT);
   coreWfiLoop(core);
}

static void
core0EntryPoint(cpu::Core *core)
{
   coreWfiLoop(core);
   gLog->error("Core0 exit");
}

static void
core2EntryPoint(cpu::Core *core)
{
   coreWfiLoop(core);
   gLog->error("Core2 exit");
}

void
cpuEntrypoint(cpu::Core *core)
{
   cafe::kernel::internal::initialiseCoreContext(core);
   cafe::kernel::internal::initialiseExceptionContext(core);

   // Run the cores!
   switch (core->id) {
   case 0:
      core0EntryPoint(core);
      break;
   case 1:
      core1EntryPoint(core);
      break;
   case 2:
      core2EntryPoint(core);
      break;
   };
}

loader::LoadedModule *
getUserModule()
{
   return nullptr;
}

loader::LoadedModule *
getTLSModule(uint32_t index)
{
   return nullptr;
}

void
exitProcess(int code)
{
   sExitCode = code;
   sRunning = false;
   cpu::halt();
   cafe::kernel::switchContext(nullptr);
}

bool
hasExited()
{
   return !sRunning;
}

int
getExitCode()
{
   return sExitCode;
}

const decaf::GameInfo &
getGameInfo()
{
   return sGameInfo;
}

} // namespace kernel
