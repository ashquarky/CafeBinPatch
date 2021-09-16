/*  Copyright 2021 Ash Logan "quarktheawesome" <ash@heyquark.com>

    Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <optional>
#include <vector>
#include <filesystem>
#include <algorithm>

#include <wups.h>
#include <kernel/kernel.h>
#include <whb/log_udp.h>
#include <coreinit/cache.h>
#include <coreinit/dynload.h>
#include <coreinit/memorymap.h>

#include "utils/logger.h"
#include "modfile/modfile.hpp"

//TODO: come up with a real name
WUPS_PLUGIN_NAME("CafeBinPatch");
WUPS_PLUGIN_DESCRIPTION("");
WUPS_PLUGIN_VERSION("v0.1");
WUPS_PLUGIN_AUTHOR("quarktheawesome");
WUPS_PLUGIN_LICENSE("ISC");

WUPS_USE_WUT_DEVOPTAB();

std::optional<std::vector<OSDynLoad_NotifyData>> TryGetRPLInfo() {
  int num_rpls = OSDynLoad_GetNumberOfRPLs();
  if (num_rpls == 0) {
    return std::nullopt;
  }

  DEBUG_FUNCTION_LINE("num_rpls: %d", num_rpls);

  std::vector<OSDynLoad_NotifyData> rpls;
  rpls.resize(num_rpls);

  bool ret = OSDynLoad_GetRPLInfo(0, num_rpls, rpls.data());
  if (!ret) {
    return std::nullopt;
  }

  /*auto& rpl = rpls[0];
  DEBUG_FUNCTION_LINE("rpl[0]: [%s] %08x %08x %08x|%08x %08x %08x|%08x %08x %08x",
    rpl.name, rpl.textAddr, rpl.textOffset, rpl.textSize,
    rpl.dataAddr, rpl.dataOffset, rpl.dataSize,
    rpl.readAddr, rpl.readOffset, rpl.readSize
  );*/

  return rpls;
}

bool PatchInstruction(void* instr, uint32_t original, uint32_t replacement) {
  uint32_t current = *(uint32_t*)instr;
  DEBUG_FUNCTION_LINE("current instr %08x", current);
  if (current != original) return current == replacement;

  KernelCopyData(OSEffectiveToPhysical((uint32_t)instr), OSEffectiveToPhysical((uint32_t)&replacement), sizeof(replacement));
  //Only works on AROMA! WUPS 0.1's KernelCopyData is uncached, needs DCInvalidate here instead
  DCFlushRange(instr, 4);
  ICInvalidateRange(instr, 4);

  current = *(uint32_t*)instr;
  DEBUG_FUNCTION_LINE("patched instr %08x", current);

  return true;
}

bool PatchDynLoadFunctions() {
  uint32_t* patch1 = ((uint32_t*)&OSDynLoad_GetNumberOfRPLs) + 6;
  uint32_t* patch2 = ((uint32_t*)&OSDynLoad_GetRPLInfo) + 22;

  if (!PatchInstruction(patch1, 0x41820038 /* beq +38 */,  0x60000000 /*nop*/)) {
    return false;
  }
  if (!PatchInstruction(patch2, 0x41820100 /* beq +100 */, 0x60000000 /*nop*/)) {
    return false;
  }

  return true;
}

ON_APPLICATION_START() {
  WHBLogUdpInit();
  DEBUG_FUNCTION_LINE("Hi!");

  auto orplinfo = TryGetRPLInfo();
  if (!orplinfo) {
    DEBUG_FUNCTION_LINE("Couldn't get RPL info - trying patches");
    bool bret = PatchDynLoadFunctions();
    if (!bret) {
      DEBUG_FUNCTION_LINE("OSDynLoad patches failed! Quitting");
      return;
    }

    //try again
    orplinfo = TryGetRPLInfo();
    if (!orplinfo) {
      DEBUG_FUNCTION_LINE("Still couldn't get RPL info - quitting");
      return;
    }
  }

  auto rplinfo = *orplinfo;

  const std::filesystem::path patchdir{"wiiu/patches"};
  for (const auto& dirent : std::filesystem::directory_iterator{patchdir}) {
    if (dirent.path().extension() != ".pch") continue; //todo less bad extension

    auto modfile = ParseModFile(dirent.path());
    if (!modfile) { DEBUG_FUNCTION_LINE(""); continue; }

    DEBUG_FUNCTION_LINE("Loaded %s.", dirent.path().c_str());
    for (const auto& words : modfile->write_words_patches) {
      DEBUG_FUNCTION_LINE("words: %d words to %08x", words.data.size(), words.target_addr);
      DEBUG_FUNCTION_LINE("TODO: unimplemented");
    }
    for (const auto& bytes : modfile->write_bytes_patches) {
      DEBUG_FUNCTION_LINE("bytes: %d bytes to %08x (%x), rpl %s %d", bytes.data.size(), bytes.target_offset, bytes.target_offset_base, bytes.rpl.c_str(), bytes.rpl.length());

      //find owner
      auto rpl = std::find_if(rplinfo.cbegin(), rplinfo.cend(), [&](const OSDynLoad_NotifyData& data){ return bytes.IsTargetRPL(data.name); });
      if (rpl == std::cend(rplinfo)) {
        DEBUG_FUNCTION_LINE("");
        continue;
      }

      uint32_t target_addr = bytes.target_offset;
      switch(bytes.target_offset_base) {
        case MODSFILE_OFFSET_BASE_RPL_TEXT: {
          target_addr += rpl->textAddr;
          break;
        }
        case MODSFILE_OFFSET_BASE_RPL_DATA: {
          target_addr += rpl->dataAddr;
          break;
        }
        case MODSFILE_OFFSET_BASE_RPL_READ: {
          target_addr += rpl->readAddr;
          break;
        }
        default:
        case MODSFILE_OFFSET_BASE_ABSOLUTE: {
          break;
        }
      }

      KernelCopyData(OSEffectiveToPhysical(target_addr), OSEffectiveToPhysical((uint32_t)bytes.data.data()), bytes.data.size());
      DCFlushRange((void*)bytes.data.data(), bytes.data.size());
      if (bytes.is_instructions) {
        ICInvalidateRange((void*)bytes.data.data(), bytes.data.size());
      }

      DEBUG_FUNCTION_LINE("Patch applied!");
    }
  }
}

INITIALIZE_PLUGIN() {
  WHBLogUdpInit();
}

DEINITIALIZE_PLUGIN() {
  WHBLogUdpDeinit();
}
