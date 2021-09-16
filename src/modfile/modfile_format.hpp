#pragma once

#include <cstdint>

// on-disk file format for mods
// all multibyte constructs are big-endian

struct MODSFILE_SECTION_TLV {
  uint8_t type[2];
  uint8_t length[2]; //64k should be enough for anybody
  uint8_t value[];
};

struct MODSFILE {
  uint8_t magic[4];
  uint8_t version[2];
  uint8_t numSections[2];
//  uint8_t reserved[4];
  MODSFILE_SECTION_TLV sections[];
};

enum MODSFILE_SECTION_TYPE {
  MODSFILE_SECTION_TYPE_WRITE_WORDS = 0,
  MODSFILE_SECTION_TYPE_RESERVED1 = 1, //TODO figure out what a "code group" is
  MODSFILE_SECTION_TYPE_WRITE_BYTES = 2,
  MODSFILE_SECTION_TYPE_RESERVED2 = 3, //TODO figure out inject_asm semantics
};

enum MODSFILE_OFFSET_BASE {
  MODSFILE_OFFSET_BASE_ABSOLUTE = 0, //absolute addressing - do not use!
  MODSFILE_OFFSET_BASE_RPL_TEXT = 1, //rpl .text
  MODSFILE_OFFSET_BASE_RPL_DATA = 2, //rpl .data
  MODSFILE_OFFSET_BASE_RPL_READ = 3, //rpl .rodata
};

struct MODSFILE_SECTION_WRITE_WORDS {
  uint8_t address[4];
  uint8_t words[]; //size must be a multiple of 4
};

enum MODSFILE_WRITEBYTES_FLAG {
  MODSFILE_WRITEBYTES_FLAG_INSTRUCTIONS = 1, //written data is instructions
};

struct MODSFILE_SECTION_WRITE_BYTES {
  char rplname[32]; //patch target - TODO what is the actual max rpl name len
  uint8_t offsetbase[2]; //must be MODSFILE_SECTION_TYPE
  uint8_t reserved;
  uint8_t flag; //MODSFILE_WRITEBYTES_FLAG
  uint8_t offset[4];
  uint8_t data[];
};
