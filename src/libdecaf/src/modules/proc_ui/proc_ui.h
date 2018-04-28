#pragma once
#include "kernel/kernel_hlemodule.h"

namespace proc_ui
{

/**
 * \defgroup proc_ui proc_ui
 *
 * Contains functions to manage and interface with the process switching in Cafe
 * OS, providing a developer-friendly API for moving in and out of the
 * foreground.
 */

class Module : public kernel::HleModuleImpl<Module>
{
public:
   virtual void initialise() override;

public:
   static void RegisterFunctions();

private:
   static void registerCoreFunctions();
};

} // namespace proc_ui
