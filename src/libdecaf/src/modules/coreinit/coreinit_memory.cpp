#include "coreinit.h"
#include "coreinit_memory.h"
#include "coreinit_core.h"
#include "kernel/kernel_memory.h"
#include "kernel/kernel_syscalls.h"

#include <common/platform_memory.h>
#include <common/teenyheap.h>
#include <libcpu/mmu.h>

namespace coreinit
{

/**
 * Moves chunks of memory around, similarly to memmove. Overlapping source and
 * destination regions are supported.
 *
 * \param dst
 * The destination address for the move.
 *
 * \param src
 * The source address for the move.
 *
 * \param size
 * The size of the data to be moved, in bytes.
 *
 * \param flush
 * Whether to flush the data caches for the source and destination.
 *
 * \return
 * dst is returned.
 *
 * \note
 * This function makes use of the cache to speed up the copy, so a flush is
 * recommended.
 */
void *
OSBlockMove(void *dst,
            const void *src,
            uint32_t size,
            BOOL flush)
{
   std::memmove(dst, src, size);
   return dst;
}

/**
 * Fills a chunk of memory with the given value, like memset.
 *
 * \param dst
 * Pointer to the memory to fill.
 *
 * \param val
 * Byte value to be set.
 *
 * \param size
 * Number of bytes to be set to val.
 *
 * \return
 * dst is returned.
 */
void *
OSBlockSet(void *dst,
           uint8_t val,
           uint32_t size)
{
   std::memset(dst, val, size);
   return dst;
}

/**
 * C standard memmove. Under the hood, this calls OSBlockMove() with flush set
 * to \c true.
 *
 * \param dst
 * The destination for the move.
 *
 * \param src
 * The source for the move.
 *
 * \param size
 * The amount of memory to move, in bytes.
 *
 * \if false
 * meta: worth noting that most homebrew uses the newlib implementation and not
 * this one? maybe recommend calling super-optimised OSBlockMove instead?
 * \endif
 */
static void *
coreinit_memmove(void *dst,
                 const void *src,
                 uint32_t size)
{
   std::memmove(dst, src, size);
   return dst;
}

/**
 * C standard memcpy. Under the hood, this calls OSBlockMove() with flush set
 * to \c true.
 *
 * \param dst
 * Pointer to the destination for the copy.
 *
 * \param src
 * Pointer to the source of data to be copied.
 *
 * \size
 * The number of bytes to copy.
 *
 * \return
 * dst is returned.
 */
static void *
coreinit_memcpy(void *dst,
                const void *src,
                uint32_t size)
{
   std::memcpy(dst, src, size);
   return dst;
}

/**
 * C standard memset. Under the hood, this calls OSBlockSet().
 *
 * \param dst
 * Pointer to the memory to fill.
 *
 * \param val
 * Byte value to be set.
 *
 * \param size
 * Number of bytes to be set to val.
 *
 * \return
 * dst is returned.
 */
static void *
coreinit_memset(void *dst,
                int val,
                uint32_t size)
{
   std::memset(dst, val, size);
   return dst;
}

/**
 * Gets the location and size of available memory areas.
 *
 * \param type
 * Type of memory to get information about. See #OSMemoryType.
 *
 * \param addr
 * Pointer to write the area's address to.
 *
 * \param size
 * Pointer to write the area's size to.
 *
 * \return
 * 0 on success, -1 otherwise.
 */
int
OSGetMemBound(OSMemoryType type,
              be_val<uint32_t> *addr,
              be_val<uint32_t> *size)
{
   uint32_t memAddr, memSize;

   switch (type) {
   case OSMemoryType::MEM1:
   {
      auto bounds = kernel::getVirtualRange(kernel::VirtualRegion::MEM1);
      memAddr = bounds.start.getAddress();
      memSize = bounds.size;
      break;
   }
   case OSMemoryType::MEM2:
   {
      auto bounds = kernel::getVirtualRange(kernel::VirtualRegion::AppHeapData);
      memAddr = bounds.start.getAddress();
      memSize = bounds.size;
      break;
   }
   default:
      return -1;
   }

   *addr = memAddr;
   *size = memSize;
   return 0;
}

/**
 * Gets the location and size of the foreground bucket memory area.
 *
 * \note
 * This function may not account for the system reserved portion of the
 * foreground bucket. Consider using OSGetForegroundBucketFreeArea() instead. 
 *
 * \param addr
 * Pointer to write the foreground bucket's address to.
 *
 * \param size
 * Pointer to write the foreground bucket's size to.
 *
 * \return
 * /c true on success.
 */
BOOL
OSGetForegroundBucket(be_val<uint32_t> *addr,
                      be_val<uint32_t> *size)
{
   auto bounds = kernel::getVirtualRange(kernel::VirtualRegion::ForegroundBucket);

   if (addr) {
      *addr = bounds.start.getAddress();
   }

   if (size) {
      *size = bounds.size;
   }

   return TRUE;
}

/**
 * Gets the location and size of the application-usable portion of the
 * foreground bucket area.
 *
 * \param addr
 * Pointer to write the bucket area's address to.
 *
 * \param size
 * Pointer to write the bucket area's size to.
 *
 * \return
 * /c true on success.
 */
BOOL
OSGetForegroundBucketFreeArea(be_val<uint32_t> *addr,
                              be_val<uint32_t> *size)
{
   auto bounds = kernel::getVirtualRange(kernel::VirtualRegion::ForegroundBucket);

   if (addr) {
      *addr = bounds.start.getAddress();
   }

   if (size) {
      // First 40mb of foreground is for applications
      // Last 24mb of foreground is saved for system use
      *size = 40 * 1024 * 1024;
      decaf_check(bounds.size >= *size);
   }

   return TRUE;
}

/**
 * Runs a PowerPC \c sync and a PowerPC \c eieio instruction, required in some
 * caching situations. If you don't know what this is, you don't need it.
 *
 * \note
 * All of Cafe OS's caching functions, such as DCFlushRange(), have their own
 * memory barriers embedded. You do not need to call this function after calling
 * a Cafe OS caching function.
 */
void
OSMemoryBarrier()
{
   // TODO: OSMemoryBarrier
}

/**
 * Gets the range of available physical memory (not reserved for app code or
 * data).
 *
 * \param start
 * Pointer to write the starting address of the memory area to.
 *
 * \param size
 * Pointer to write the size of the memory area to.
 *
 * \if false
 * Is memory returned by this function actually safe to map and use? couldn't
 * get a straight answer from kernel_memory.cpp...
 * \endif
 */
void
OSGetAvailPhysAddrRange(be_val<ppcaddr_t> *start,
                        be_val<uint32_t> *size)
{
   auto range = kernel::syscall::getAvailPhysAddrRange();

   if (start) {
      *start = range.start.getAddress();
   }

   if (size) {
      *size = range.size;
   }
}

/**
 * Gets the range of physical memory used for the application's data.
 *
 * \param start
 * Pointer to write the starting address of the memory area to.
 *
 * \param size
 * Pointer to write the size of the memory area to.
 *
 * \if false
 * does this include the main heap?
 * \endif
 */
void
OSGetDataPhysAddrRange(be_val<ppcaddr_t> *start,
                       be_val<uint32_t> *size)
{
   auto range = kernel::syscall::getDataPhysAddrRange();

   if (start) {
      *start = range.start.getAddress();
   }

   if (size) {
      *size = range.size;
   }
}

/**
 * Gets the range of virtual addresses available for mapping.
 *
 * \param start
 * Pointer to write the starting address of the memory area to.
 *
 * \param size
 * Pointer to write the size of the memory area to.
 *
 * \sa
 * - OSMapMemory()
 */
void
OSGetMapVirtAddrRange(be_val<ppcaddr_t> *start,
                      be_val<uint32_t> *size)
{
   auto range = kernel::syscall::getMapVirtAddrRange();

   if (start) {
      *start = range.start.getAddress();
   }

   if (size) {
      *size = range.size;
   }
}

/**
 * Allocates virtual address range for later mapping.
 *
 * \param address
 * Requested start address for the range. If there is no preference, NULL can be
 * used.
 *
 * \param size
 * Size of address range to allocate.
 *
 * \param size
 * Alignment of address range to allocate.
 *
 * \return
 * The starting address of the newly allocated range, or NULL on failure.
 */
ppcaddr_t
OSAllocVirtAddr(ppcaddr_t address,
                uint32_t size,
                uint32_t alignment)
{
   return kernel::syscall::allocVirtAddr(cpu::VirtualAddress { address },
                                         size,
                                         alignment).getAddress();
}

BOOL
OSFreeVirtAddr(ppcaddr_t address,
               uint32_t size)
{
   return kernel::syscall::freeVirtAddr(cpu::VirtualAddress { address },
                                        size)
         ? TRUE : FALSE;
}

kernel::VirtualMemoryType
OSQueryVirtAddr(ppcaddr_t address)
{
   return kernel::syscall::queryVirtAddr(cpu::VirtualAddress { address });
}

BOOL
OSMapMemory(ppcaddr_t virtAddress,
            ppcaddr_t physAddress,
            uint32_t size,
            kernel::MapPermission permission)
{
   return kernel::syscall::mapMemory(cpu::VirtualAddress { virtAddress },
                                     cpu::PhysicalAddress { physAddress },
                                     size,
                                     permission)
         ? TRUE : FALSE;
}

BOOL
OSUnmapMemory(ppcaddr_t virtAddress,
              uint32_t size)
{
   return kernel::syscall::unmapMemory(cpu::VirtualAddress { virtAddress },
                                       size)
         ? TRUE : FALSE;
}

void
Module::registerMemoryFunctions()
{
   RegisterKernelFunction(OSBlockMove);
   RegisterKernelFunction(OSBlockSet);
   RegisterKernelFunction(OSGetMemBound);
   RegisterKernelFunction(OSGetForegroundBucket);
   RegisterKernelFunction(OSGetForegroundBucketFreeArea);
   RegisterKernelFunction(OSMemoryBarrier);
   RegisterKernelFunction(OSGetAvailPhysAddrRange);
   RegisterKernelFunction(OSGetDataPhysAddrRange);
   RegisterKernelFunction(OSGetMapVirtAddrRange);
   RegisterKernelFunction(OSAllocVirtAddr);
   RegisterKernelFunction(OSFreeVirtAddr);
   RegisterKernelFunction(OSQueryVirtAddr);
   RegisterKernelFunction(OSMapMemory);
   RegisterKernelFunction(OSUnmapMemory);
   RegisterKernelFunctionName("memcpy", coreinit_memcpy);
   RegisterKernelFunctionName("memset", coreinit_memset);
   RegisterKernelFunctionName("memmove", coreinit_memmove);
}

} // namespace coreinit
