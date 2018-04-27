#include "dmae.h"
#include "modules/coreinit/coreinit_time.h"

#include <common/cbool.h>
#include <cstdint>

namespace dmae
{

/**
 * \defgroup dmae dmae
 *
 * Functions to interact with the Espresso's DMA engine and perform accelerated
 * memory operations.
 * @{
 */

/**
 * Possible endian swap methods to apply during a copy.
 */ 
enum class DMAEEndianSwapMode : uint32_t
{
   //! No endian manipulation is performed.
   None = 0,
   //! Divide input into 16-bit values, then perform an endian swap on each.
   Swap8In16 = 1,
   //! Divide input into 32-bit values, then perform an endian swap on each.
   Swap8In32 = 2
};

static uint32_t
sDmaeTimeout = 5;

static coreinit::OSTime
sLastTimeStamp = 0;

/**
 * Gets the last retired operation.
 *
 * \return
 * The timestamp corresponding to the last retired operation.
 *
 * \if false
 * meta: what is this actually supposed to do? would like to write something
 * slightly more helpful
 * \endif
 */
coreinit::OSTime
DMAEGetRetiredTimeStamp()
{
   return sLastTimeStamp;
}

/**
 * Gets the operation last submitted to the hardware.
 *
 * \return
 * The timestamp corresponding to the last submitted operation.
 */
coreinit::OSTime
DMAEGetLastSubmittedTimeStamp()
{
   return sLastTimeStamp;
}

/**
 * Wait for a given operation to complete.
 *
 * \param timestamp
 * The timestamp corresponding to the operation to wait for.
 *
 * \return
 * \c true if the operation completed successfully.
 *
 * \if false
 * meta: made up the return bit, please correct if wrong. perhaps false means a
 * timeout occured?
 * \endif 
 */
BOOL
DMAEWaitDone(coreinit::OSTime timestamp)
{
   return TRUE;
}

/**
 * Start an async copy operation using the hardware DMA engine.
 * When using DMAEEndianSwapMode::None, this function is similar to memcpy.
 * This method can be significantly faster than CPU-based copies, and can be
 * processed asynchronously.
 *
 * \param dst
 * The destination's virtual address.
 *
 * \param src
 * The source's virtual address.
 *
 * \param numDwords
 * The size of the copy operation *in dwords* (32-bit/4-byte groups)
 *
 * \param endian
 * The type of endian swapping to use while copying.
 *
 * \return
 * A timestamp corresponding with the copy. Used with other DMAE functions to
 * identify this operation.
 *
 * \sa
 *  - DMAEWaitDone()
 *  - DMAEFillMem()
 */
coreinit::OSTime
DMAECopyMem(void *dst, void *src, uint32_t numDwords, DMAEEndianSwapMode endian)
{
   if (endian == DMAEEndianSwapMode::None) {
      memcpy(dst, src, numDwords * 4);
   } else if (endian == DMAEEndianSwapMode::Swap8In16) {
      auto dstWords = reinterpret_cast<uint16_t*>(dst);
      auto srcWords = reinterpret_cast<uint16_t*>(src);
      for (uint32_t i = 0; i < numDwords * 2; ++i) {
         *dstWords++ = byte_swap(*srcWords++);
      }
   } else if (endian == DMAEEndianSwapMode::Swap8In32) {
      auto dstDwords = reinterpret_cast<uint32_t*>(dst);
      auto srcDwords = reinterpret_cast<uint32_t*>(src);
      for (uint32_t i = 0; i < numDwords; ++i) {
         *dstDwords++ = byte_swap(*srcDwords++);
      }
   }

   sLastTimeStamp = coreinit::OSGetTime();
   return sLastTimeStamp;
}

/**
 * Start an async fill operation using the hardware DMA engine.
 * This function is similar to memset.
 * This method can be significantly faster than CPU-based fills, and can be
 * processed asynchronously.
 *
 * \param dst
 * The destination's virtual address.
 *
 * \param value
 * The 32-bit value to fill the memory with.
 *
 * \param numDwords
 * The size of the fill operation *in dwords* (32-bit/4-byte groups)
 *
 * \return
 * A timestamp corresponding with the fill. Used with other DMAE functions to
 * identify this operation.
 *
 * \sa
 *  - DMAEWaitDone()
 *  - DMAECopyMem()
 */
coreinit::OSTime
DMAEFillMem(void *dst, uint32_t value, uint32_t numDwords)
{
   // Note that rather than using be_val here, we simply byte-swap
   //  the incoming word before we enter the loop to write it out.
   auto dstValue = byte_swap(value);
   auto dstDwords = reinterpret_cast<uint32_t*>(dst);
   for (uint32_t i = 0; i < numDwords; ++i) {
      dstDwords[i] = dstValue;
   }

   sLastTimeStamp = coreinit::OSGetTime();
   return sLastTimeStamp;
}

/**
 * Sets the timeout for DMA operations.
 *
 * \param timeout
 * The new timeout to set.
 *
 * \sa
 * - DMAEGetTimeout()
 *
 * \if false
 * meta: what unit is this in? what does it affect?
 * \endif
 */
void
DMAESetTimeout(uint32_t timeout)
{
   sDmaeTimeout = timeout;
}

/**
 * Gets the current timeout for DMA operations.
 *
 * \return
 * The current timeout.
 *
 * \sa
 * - DMAESetTimeout()
 */
uint32_t
DMAEGetTimeout()
{
   return sDmaeTimeout;
}

/** @} */

void
Module::registerCoreFunctions()
{
   RegisterKernelFunction(DMAEGetRetiredTimeStamp);
   RegisterKernelFunction(DMAEGetLastSubmittedTimeStamp);
   RegisterKernelFunction(DMAEWaitDone);
   RegisterKernelFunction(DMAECopyMem);
   RegisterKernelFunction(DMAEFillMem);
   RegisterKernelFunction(DMAESetTimeout);
   RegisterKernelFunction(DMAEGetTimeout);
}

} // namespace sysapp
