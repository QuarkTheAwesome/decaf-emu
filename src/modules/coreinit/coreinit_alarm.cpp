#include <array>
#include "coreinit.h"
#include "coreinit_alarm.h"
#include "coreinit_core.h"
#include "coreinit_spinlock.h"
#include "coreinit_scheduler.h"
#include "coreinit_thread.h"
#include "coreinit_memheap.h"
#include "coreinit_time.h"
#include "coreinit_queue.h"
#include "utils/wfunc_call.h"
#include "cpu/cpu.h"

namespace coreinit
{

namespace internal
{
   void updateCpuAlarm();
}

const uint32_t
OSAlarm::Tag;

const uint32_t
OSAlarmQueue::Tag;

static OSSpinLock *
sAlarmLock;

static std::array<OSAlarmQueue *, CoreCount>
sAlarmQueue;


/**
 * Internal alarm cancel.
 *
 * Reset the alarm state to cancelled.
 * Wakes up all threads waiting on the alarm.
 * Removes the alarm from any queue it is in.
 */
static BOOL
cancelAlarmNoLock(OSAlarm *alarm)
{
   if (alarm->state != OSAlarmState::Set) {
      return FALSE;
   }

   alarm->state = OSAlarmState::None;
   alarm->nextFire = 0;
   alarm->period = 0;

   if (alarm->alarmQueue) {
      OSEraseFromQueue<OSAlarmQueue>(alarm->alarmQueue, alarm);
      alarm->alarmQueue = nullptr;
   }

   OSWakeupThread(&alarm->threadQueue);
   return TRUE;
}


/**
 * Cancel an alarm.
 */
BOOL
OSCancelAlarm(OSAlarm *alarm)
{
   ScopedSpinLock lock(sAlarmLock);
   BOOL result = cancelAlarmNoLock(alarm);
   internal::updateCpuAlarm();
   return result;
}


/**
 * Cancel all alarms which have a matching tag.
 */
void
OSCancelAlarms(uint32_t group)
{
   ScopedSpinLock lock(sAlarmLock);

   for (auto i = 0u; i < 3; ++i) {
      auto queue = sAlarmQueue[i];

      for (OSAlarm *alarm = queue->head; alarm; ) {
         auto next = alarm->link.next;

         if (alarm->group == group) {
            cancelAlarmNoLock(alarm);
         }

         alarm = next;
      }
   }
   internal::updateCpuAlarm();
}


/**
 * Initialise an alarm structure.
 */
void
OSCreateAlarm(OSAlarm *alarm)
{
   OSCreateAlarmEx(alarm, nullptr);
}


/**
 * Initialise an alarm structure.
 */
void
OSCreateAlarmEx(OSAlarm *alarm, const char *name)
{
   memset(alarm, 0, sizeof(OSAlarm));
   alarm->tag = OSAlarm::Tag;
   alarm->name = name;
   OSInitThreadQueueEx(&alarm->threadQueue, alarm);
}


/**
 * Return the user data stored in the alarm using OSSetAlarmUserData
 */
void *
OSGetAlarmUserData(OSAlarm *alarm)
{
   return alarm->userData;
}


/**
 * Initialise an alarm queue structure
 */
void
OSInitAlarmQueue(OSAlarmQueue *queue)
{
   OSInitAlarmQueueEx(queue, nullptr);
}


/**
 * Initialise an alarm queue structure with a name
 */
void
OSInitAlarmQueueEx(OSAlarmQueue *queue, const char *name)
{
   memset(queue, 0, sizeof(OSAlarmQueue));
   queue->tag = OSAlarmQueue::Tag;
   queue->name = name;
}


/**
 * Set a one shot alarm to perform a callback after an amount of time.
 */
BOOL
OSSetAlarm(OSAlarm *alarm, OSTime time, AlarmCallback callback)
{
   return OSSetPeriodicAlarm(alarm, OSGetTime() + time, 0, callback);
}


/**
 * Set a repeated alarm to execute a callback every interval from start.
 */
BOOL
OSSetPeriodicAlarm(OSAlarm *alarm, OSTime start, OSTime interval, AlarmCallback callback)
{
   ScopedSpinLock lock(sAlarmLock);

   // Set alarm
   alarm->nextFire = start;
   alarm->callback = callback;
   alarm->period = interval;
   alarm->context = nullptr;
   alarm->state = OSAlarmState::Set;

   // Erase from old alarm queue
   if (alarm->alarmQueue) {
      OSEraseFromQueue(static_cast<OSAlarmQueue*>(alarm->alarmQueue), alarm);
      alarm->alarmQueue = nullptr;
   }

   // Add to this core's alarm queue
   auto core = OSGetCoreId();
   auto queue = sAlarmQueue[core];
   alarm->alarmQueue = queue;
   OSAppendQueue(queue, alarm);

   // Set the interrupt timer in processor
   internal::updateCpuAlarm();
   return TRUE;
}


/**
 * Set an alarm tag which is used in OSCancelAlarms for bulk cancellation.
 */
void
OSSetAlarmTag(OSAlarm *alarm, uint32_t group)
{
   OSUninterruptibleSpinLock_Acquire(sAlarmLock);
   alarm->group = group;
   OSUninterruptibleSpinLock_Release(sAlarmLock);
}


/**
 * Set alarm user data which is returned by OSGetAlarmUserData.
 */
void
OSSetAlarmUserData(OSAlarm *alarm, void *data)
{
   OSUninterruptibleSpinLock_Acquire(sAlarmLock);
   alarm->userData = data;
   OSUninterruptibleSpinLock_Release(sAlarmLock);
}


/**
 * Sleep the current thread until the alarm has been triggered or cancelled.
 */
BOOL
OSWaitAlarm(OSAlarm *alarm)
{
   coreinit::internal::lockScheduler();
   OSUninterruptibleSpinLock_Acquire(sAlarmLock);
   BOOL result = FALSE;
   assert(alarm);
   assert(alarm->tag == OSAlarm::Tag);

   if (alarm->state != OSAlarmState::Set) {
      OSUninterruptibleSpinLock_Release(sAlarmLock);
      coreinit::internal::unlockScheduler();
      return FALSE;
   }

   coreinit::internal::sleepThreadNoLock(&alarm->threadQueue);
   OSUninterruptibleSpinLock_Release(sAlarmLock);
   coreinit::internal::rescheduleNoLock();

   OSUninterruptibleSpinLock_Acquire(sAlarmLock);

   if (alarm->state == OSAlarmState::Expired) {
      result = TRUE;
   }

   OSUninterruptibleSpinLock_Release(sAlarmLock);
   coreinit::internal::unlockScheduler();
   return result;
}


void
Module::registerAlarmFunctions()
{
   RegisterKernelFunction(OSCancelAlarm);
   RegisterKernelFunction(OSCancelAlarms);
   RegisterKernelFunction(OSCreateAlarm);
   RegisterKernelFunction(OSCreateAlarmEx);
   RegisterKernelFunction(OSGetAlarmUserData);
   RegisterKernelFunction(OSInitAlarmQueue);
   RegisterKernelFunction(OSInitAlarmQueueEx);
   RegisterKernelFunction(OSSetAlarm);
   RegisterKernelFunction(OSSetPeriodicAlarm);
   RegisterKernelFunction(OSSetAlarmTag);
   RegisterKernelFunction(OSSetAlarmUserData);
   RegisterKernelFunction(OSWaitAlarm);
}

void
Module::initialiseAlarm()
{
   sAlarmLock = coreinit::internal::sysAlloc<OSSpinLock>();

   for (auto i = 0u; i < CoreCount; ++i) {
      sAlarmQueue[i] = coreinit::internal::sysAlloc<OSAlarmQueue>();
      OSInitAlarmQueue(sAlarmQueue[i]);
   }
}

namespace internal
{

/**
 * Internal alarm handler.
 *
 * Resets the alarm state.
 * Calls the users callback.
 * Wakes up any threads waiting on the alarm.
 */
static void
triggerAlarmNoLock(OSAlarmQueue *queue, OSAlarm *alarm, OSContext *context)
{
   alarm->context = context;

   if (alarm->period) {
      alarm->nextFire = OSGetTime() + alarm->period;
      alarm->state = OSAlarmState::Set;
      coreinit::internal::updateCpuAlarm();
   } else {
      alarm->nextFire = 0;
      alarm->state = OSAlarmState::None;
      alarm->alarmQueue = nullptr;
      OSEraseFromQueue<OSAlarmQueue>(queue, alarm);
   }

   if (alarm->callback) {
      OSUninterruptibleSpinLock_Release(sAlarmLock);
      alarm->callback(alarm, context);
      OSUninterruptibleSpinLock_Acquire(sAlarmLock);
   }

   coreinit::internal::wakeupThreadNoLock(&alarm->threadQueue);
}

void
updateCpuAlarm()
{
   auto queue = sAlarmQueue[cpu::get_current_core_id()];
   auto next = std::chrono::time_point<std::chrono::system_clock>::max();

   OSUninterruptibleSpinLock_Acquire(sAlarmLock);

   for (OSAlarm *alarm = queue->head; alarm; ) {
      auto nextAlarm = alarm->link.next;

      // Update next if its not past yet
      if (alarm->state == OSAlarmState::Set && alarm->nextFire) {
         auto nextFire = coreinit::internal::toTimepoint(alarm->nextFire);

         if (nextFire < next) {
            next = nextFire;
         }
      }

      alarm = nextAlarm;
   }

   OSUninterruptibleSpinLock_Release(sAlarmLock);

   cpu::core_set_next_alarm(next);
}

void
handleAlarmInterrupt(uint32_t core_id, OSContext *context)
{
   auto queue = sAlarmQueue[core_id];
   auto now = OSGetTime();
   auto next = std::chrono::time_point<std::chrono::system_clock>::max();
   bool alarmExpired = false;

   OSUninterruptibleSpinLock_Acquire(sAlarmLock);

   for (OSAlarm *alarm = queue->head; alarm; ) {
      auto nextAlarm = alarm->link.next;

      // Expire it if its past its nextFire time
      if (alarm->nextFire <= now && alarm->state == OSAlarmState::Set) {
         alarm->state = OSAlarmState::Expired;
         alarm->context = context;
         alarmExpired = true;
      }

      // Update next if its not past yet
      if (alarm->state == OSAlarmState::Set && alarm->nextFire) {
         auto nextFire = coreinit::internal::toTimepoint(alarm->nextFire);

         if (nextFire < next) {
            next = nextFire;
         }
      }

      alarm = nextAlarm;
   }

   OSUninterruptibleSpinLock_Release(sAlarmLock);

   if (alarmExpired) {
      coreinit::internal::signalIoThreadNoLock(core_id);
   }

   cpu::core_set_next_alarm(next);
}

/**
 * Internal check to see if any alarms are ready to be triggered.
 *
 * Updates the processor internal interrupt timer to trigger for the next ready alarm.
 */
void
checkAlarms(uint32_t core_id)
{
   auto queue = sAlarmQueue[core_id];
   auto now = OSGetTime();
   auto next = std::chrono::time_point<std::chrono::system_clock>::max();

   OSUninterruptibleSpinLock_Acquire(sAlarmLock);

   // Trigger all pending alarms
   for (OSAlarm *alarm = queue->head; alarm; ) {
      auto nextAlarm = alarm->link.next;

      if (alarm->state == OSAlarmState::Expired) {
         triggerAlarmNoLock(queue, alarm, alarm->context);
      }

      alarm = nextAlarm;
   }

   OSUninterruptibleSpinLock_Release(sAlarmLock);
}

} // namespace internal

} // namespace coreinit
