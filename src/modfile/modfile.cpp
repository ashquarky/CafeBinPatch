/*  Copyright 2021 Ash Logan "quarktheawesome" <ash@heyquark.com>

    Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is hereby granted, provided that the above copyright notice and this permission notice appear in all copies.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "modfile.hpp"
#include <stdio.h>
#include <sys/stat.h>

#include "utils/logger.h"

std::unique_ptr<MODSFILE> ReadToMem(std::filesystem::path path) {
  FILE* fp = fopen(path.c_str(), "rb");
  if (!fp) return {};
  fseek(fp, 0, SEEK_END);
  auto filesize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  std::unique_ptr<MODSFILE> file((MODSFILE*)new uint8_t[filesize]);
  fread(file.get(), filesize, 1, fp);
  fclose(fp);

  return file;
}

#define READ_32(x) (uint32_t)((x[0] << 24) | (x[1] << 16) | (x[2] << 8) | (x[3] << 0))
#define READ_16(x) (uint16_t)((x[0] << 8)  | (x[1] << 0))

std::optional<ModFile> ParseModFile(std::filesystem::path path) {
  auto file = ReadToMem(path);
  ModFile result;

  if (READ_32(file->magic) != 0xBABECAFE) { DEBUG_FUNCTION_LINE(""); return std::nullopt; }
  if (READ_16(file->version) != 1) { DEBUG_FUNCTION_LINE(""); return std::nullopt; }

  int numSections = READ_16(file->numSections); //XXX derive this from filesize, security vuln
  MODSFILE_SECTION_TLV* tlv = file->sections;
  for (int section = 0; section < numSections; section++) {
    switch (READ_16(tlv->type)) {
      case MODSFILE_SECTION_TYPE_WRITE_WORDS: {
        MODSFILE_SECTION_WRITE_WORDS* sec = (MODSFILE_SECTION_WRITE_WORDS*)tlv->value;
        size_t words_len = READ_16(tlv->length) - sizeof(*sec);
        if (words_len % 4) { DEBUG_FUNCTION_LINE(""); break; };

        result.write_words_patches.push_back(ModWriteWords{
          .target_addr = READ_32(sec->address),
          .data = {(uint32_t*)sec->words, (uint32_t*)(sec->words + words_len)},
        });
        break;
      }
      case MODSFILE_SECTION_TYPE_WRITE_BYTES: {
        MODSFILE_SECTION_WRITE_BYTES* sec = (MODSFILE_SECTION_WRITE_BYTES*)tlv->value;
        size_t bytes_len = READ_16(tlv->length) - sizeof(*sec);

        sec->rplname[sizeof(sec->rplname)-1] = '\0';

        result.write_bytes_patches.push_back(ModWriteBytes{
          .rpl = {sec->rplname},
          .target_offset_base = static_cast<MODSFILE_OFFSET_BASE>(READ_16(sec->offsetbase)),
          .target_offset = READ_32(sec->offset),

          .is_instructions = !!(sec->flag & MODSFILE_WRITEBYTES_FLAG_INSTRUCTIONS),

          .data = {sec->data, sec->data + bytes_len},
        });
        break;
      }
    }
    tlv = (MODSFILE_SECTION_TLV*)((void*)tlv + READ_16(tlv->length) + sizeof(*tlv)); //oh dear
  }

  return result;
}
