#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include "modfile_format.hpp" //for enums
#include "utils/logger.h"
struct ModWriteWords {
  uint32_t target_addr;
  std::vector<uint32_t> data;
};

struct ModWriteBytes {
  std::string rpl;
  MODSFILE_OFFSET_BASE target_offset_base;
  uint32_t target_offset;

  bool is_instructions;

  std::vector<uint8_t> data;

  bool IsTargetRPL(std::string name) const {
    return name.ends_with(rpl);
  }
};

struct ModFile {
  std::vector<ModWriteWords> write_words_patches;
  std::vector<ModWriteBytes> write_bytes_patches;
};

std::optional<ModFile> ParseModFile(std::filesystem::path file);
