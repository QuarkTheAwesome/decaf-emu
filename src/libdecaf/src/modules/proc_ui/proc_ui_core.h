#pragma once
#include "ppcutils/wfunc_ptr.h"
#include "proc_ui_enum.h"

#include <common/cbool.h>
#include <cstdint>

namespace proc_ui
{

/**
 * \ingroup proc_ui
 * @{
 */

/**
 * Called when the application needs to save.
 */
using ProcUISaveCallback = wfunc_ptr<void>;
/**
 * Called when the application needs to save.
 * void* argument is provided in ProcUIInitEx().
 */
using ProcUISaveCallbackEx = wfunc_ptr<uint32_t, void*>;
/**
 * Generic ProcUI callback.
 * void* argument is provided in ProcUIRegisterCallback().
 */
using ProcUICallback = wfunc_ptr<uint32_t, void *>;

void
ProcUIInit(ProcUISaveCallback saveCallback);

void
ProcUIInitEx(ProcUISaveCallbackEx saveCallbackEx,
             void *arg);

ProcUIStatus
ProcUIProcessMessages(BOOL block);

ProcUIStatus
ProcUISubProcessMessages(BOOL block);

void
ProcUIRegisterCallback(ProcUICallbackType type,
                       ProcUICallback callback,
                       void *param,
                       uint32_t unk);

void
ProcUISetMEM1Storage(void *buffer,
                     uint32_t size);

/** @} */

} // namespace proc_ui
