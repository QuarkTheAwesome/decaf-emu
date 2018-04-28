#include "proc_ui.h"
#include "proc_ui_core.h"

namespace proc_ui
{

static ProcUISaveCallback
sSaveCallback;

static ProcUISaveCallbackEx
sSaveCallbackEx;

static void *
sSaveCallbackExArg;

struct RegisteredCallback
{
   ProcUICallback callback;
   void *param;
};

std::array<RegisteredCallback, ProcUICallbackType::Max>
sRegisteredCallbacks;

static bool
sSentAcquireMessage = false;

/**
 * Initialises the ProcUI library for use.
 *
 * \param saveCallback
 * A callback to be called when the application needs to save. The callback
 * cannot be NULL and it must call OSSavesDone_ReadyToRelease().
 *
 * \sa
 * - OSSavesDone_ReadyToRelease()
 */
void
ProcUIInit(ProcUISaveCallback saveCallback)
{
   sSaveCallback = saveCallback;
   sSaveCallbackEx = nullptr;
   sSaveCallbackExArg = nullptr;
}

/**
 * Initialises the ProcUI library for use; using a save callback that takes
 * arguments.
 *
 * \param saveCallbackEx
 * A callback to be called when the application needs to save. The callback
 * cannot be NULL and it must call OSSavesDone_ReadyToRelease().
 *
 * \param arg
 * An argument to pass into saveCallbackEx.
 *
 * \sa
 * - OSSavesDone_ReadyToRelease()
 */
void
ProcUIInitEx(ProcUISaveCallbackEx saveCallbackEx, void *arg)
{
   sSaveCallback = nullptr;
   sSaveCallbackEx = saveCallbackEx;
   sSaveCallbackExArg = arg;
}

/**
 * Main runloop for ProcUI. This function processes messages from the OS and
 * provides it an opportinity to take control (to open the HOME menu overlay,
 * for example). Returns the current state of the application.
 *
 * \param block
 * Determines whether the function should block before returning. If \c false,
 * the function returns immediately.
 *
 * \return
 * The current state of the program. See #ProcUIStatus.
 *
 * \warning
 * At this time, using ProcUI's non-blocking mode is not recommended as not much
 * is known about it. Instead, set block to \c true and examine the return
 * value.
 *
 * \note
 * This function should only be called from the main core. See OSIsMainCore().
 *
 * \if false
 * meta: what happens when block=false? is the return value trustworthy? how
 * does it interact with ProcUIRegisterCallback?
 * \endif
 */
ProcUIStatus
ProcUIProcessMessages(BOOL block)
{
   // TODO: ProcUIProcessMessages
   return ProcUIStatus::InForeground;
}

/**
 * ProcUIProcessMessages(), but for non-main cores.
 *
 * \param block
 * Determines whether the function should block before returning. If \c false,
 * the function returns immediately.
 *
 * \warning
 * At this time, using ProcUI's non-blocking mode is not recommended as not much
 * is known about it. Instead, set block to \c true and examine the return
 * value.
 *
 * \if false
 * didn't know what this did, only seen it in the else{} from OSIsMainCore...
 * please research and correct!
 * \endif
 */
ProcUIStatus
ProcUISubProcessMessages(BOOL block)
{
   // TODO: ProcUISubProcessMessages
   return ProcUIStatus::InForeground;
}

/**
 * Register a callback for certain ProcUI events.
 *
 * \param type
 * The event to register a callback for. See #ProcUICallbackType.
 *
 * \param callback
 * Function pointer for the callback to call when the given event occurs.
 *
 * \param param
 * Argument for the callback. This will be passed in as the *second* argument.
 *
 * \param unk
 */
void
ProcUIRegisterCallback(ProcUICallbackType type,
                       ProcUICallback callback,
                       void *param,
                       uint32_t unk)
{
   if (type < sRegisteredCallbacks.size()) {
      sRegisteredCallbacks[type].callback = callback;
      sRegisteredCallbacks[type].param = param;
   }
}

/**
 * Set a buffer for storing MEM1 when the application goes into the background.
 * MEM1 is not preserved when an application goes into the background, but MEM2
 * is preserved. This function can be used to set a buffer to preserve MEM1.
 *
 * \param buffer
 * The buffer in MEM2 to set as a storage area for MEM1.
 *
 * \param size
 * The size of the buffer.
 */
void
ProcUISetMEM1Storage(void *buffer, uint32_t size)
{
   // TODO: ProcUISetMEM1Storage
}

void
Module::registerCoreFunctions()
{
   RegisterKernelFunction(ProcUIInit);
   RegisterKernelFunction(ProcUIInitEx);
   RegisterKernelFunction(ProcUIProcessMessages);
   RegisterKernelFunction(ProcUISubProcessMessages);
   RegisterKernelFunction(ProcUIRegisterCallback);
   RegisterKernelFunction(ProcUISetMEM1Storage);
}

} // namespace proc_ui
