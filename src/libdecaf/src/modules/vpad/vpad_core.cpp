#include "vpad.h"
#include "vpad_core.h"

namespace vpad
{

/**
 * Initialises the VPAD library for use.
 *
 * \deprecated
 * As of Cafe OS 5.5.x (OSv10 v15702) this function simply logs a deprecation
 * message and returns. However, this may not be the case on older versions.
 */
void
VPADInit()
{
}

void
VPADSetAccParam(uint32_t chan,
                float unk1,
                float unk2)
{
}

void
VPADSetBtnRepeat(uint32_t chan,
                 float unk1,
                 float unk2)
{
}

/**
 * Turns on the rumble motor on the desired Gamepad.
 * A custom rumble pattern can be set by setting bytes in the input buffer.
 *
 * \note
 * Retail Wii U systems have a single Gamepad on channel 0.
 *
 * \param chan
 * The channel of the Gamepad to rumble.
 *
 * \param buffer
 * Pointer to an array of bytes, where each byte represents the status of the
 * rumble at a given time. 0xFF denotes rumble while 0x00 denotes no rumble.
 *
 * \param size
 * The size of the rumble pattern, in bytes.
 *
 * \if false
 * meta: find out if the bytes in buffer are an analog intensity control (e.g
 * is 0x7F "half intensity"?) or are simply binary motor on/off toggles
 * \endif
 */
int
VPADControlMotor(uint32_t chan,
                 uint8_t *buffer,
                 uint32_t size)
{
   return 0;
}

/**
 * Stops the desired Gamepad's rumble motor and cancels any ongoing rumble
 * pattern.
 *
 * \note
 * Retail Wii U systems have a single Gamepad on channel 0.
 *
 * \param chan
 * The channel of the Gamepad to stop rumbling.
 */
void
VPADStopMotor(uint32_t chan)
{
}

float
VPADIsEnableGyroAccRevise(uint32_t chan)
{
   return 0.0f;
}

float
VPADIsEnableGyroZeroPlay(uint32_t chan)
{
   return 0.0f;
}

float
VPADIsEnableGyroZeroDrift(uint32_t chan)
{
   return 0.0f;
}

float
VPADIsEnableGyroDirRevise(uint32_t chan)
{
   return 0.0f;
}

void
Module::registerCoreFunctions()
{
   RegisterKernelFunction(VPADInit);
   RegisterKernelFunction(VPADSetAccParam);
   RegisterKernelFunction(VPADSetBtnRepeat);
   RegisterKernelFunction(VPADControlMotor);
   RegisterKernelFunction(VPADStopMotor);
   RegisterKernelFunction(VPADIsEnableGyroAccRevise);
   RegisterKernelFunction(VPADIsEnableGyroZeroPlay);
   RegisterKernelFunction(VPADIsEnableGyroZeroDrift);
   RegisterKernelFunction(VPADIsEnableGyroDirRevise);
}

} // namespace vpad
