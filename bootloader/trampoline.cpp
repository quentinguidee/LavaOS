#include <string.h>

#include <ion/src/device/shared/drivers/flash.h>
#include <ion/src/device/n0110/drivers/power.h>

#include <bootloader/trampoline.h>

namespace Bootloader {

void __attribute__((noinline)) suspend() {
  Ion::Device::Power::internalFlashSuspend(true);
}

void* Trampolines[TRAMPOLINES_COUNT]
  __attribute__((section(".trampolines_table")))
  __attribute__((used))
 = {
  (void*) Bootloader::suspend, // Suspend
  (void*) Ion::Device::Flash::EraseSector, // External erase
  (void*) Ion::Device::Flash::WriteMemory, // External write
  (void*) memcmp,
  (void*) memcpy,
  (void*) memmove,
  (void*) memset,
  (void*) strchr,
  (void*) strcmp,
  (void*) strlcat,
  (void*) strlcpy,
  (void*) strlen,
  (void*) strncmp
};

}