#pragma once
#include "sha256.hpp" // FIXME: wtf?
#include "stdint.h"
#include <array>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#define MTLB_FOURCC(ch0, ch1, ch2, ch3)                                        \
  ((uint32_t)(char)(ch0) | ((uint32_t)(char)(ch1) << 8) |                      \
   ((uint32_t)(char)(ch2) << 16) | ((uint32_t)(char)(ch3) << 24))

const uint32_t MTLB_Magic = MTLB_FOURCC('M', 'T', 'L', 'B');

typedef enum MTLBFourCC : uint32_t {
  MTLB_Name = MTLB_FOURCC('N', 'A', 'M', 'E'),
  MTLB_Type = MTLB_FOURCC('T', 'Y', 'P', 'E'),
  MTLB_Offset = MTLB_FOURCC('O', 'F', 'F', 'T'),
  MTLB_Hash = MTLB_FOURCC('H', 'A', 'S', 'H'),
  MTLB_Size = MTLB_FOURCC('M', 'D', 'S', 'Z'),
  MTLB_Version = MTLB_FOURCC('V', 'E', 'R', 'S'),
  MTLB_Layer = MTLB_FOURCC('L', 'A', 'Y', 'R'),
  MTLB_Tess = MTLB_FOURCC('T', 'E', 'S', 'S'),
  MTLB_EndTag = MTLB_FOURCC('E', 'N', 'D', 'T'),
} MTLBFourCC;

typedef enum : uint16_t { MTLBPlatform_macOS = 0x8001 } MTLBPlatform;

typedef enum : uint8_t {
  MTLBType_Executable = 0x00,
} MTLBType;

typedef enum : uint8_t {
  MTLBOS_macOS = 0x81,
} MTLBOS;

typedef struct __attribute__((packed)) {
  uint32_t Magic; // MTLB
  MTLBPlatform Platform;
  uint16_t VersionMajor;
  uint16_t VersionMinor;
  MTLBType Type;
  MTLBOS OS;
  uint16_t OSVersionMajor;
  uint16_t OSVersionMinor;
  uint64_t FileSize;
  uint64_t FunctionListOffset;
  uint64_t FunctionListSize;
  uint64_t PublicMetadataOffset;
  uint64_t PublicMetadataSize;
  uint64_t PrivateMetadataOffset;
  uint64_t PrivateMetadataSize;
  uint64_t BitcodeOffset;
  uint64_t BitcodeSize;
} MTLBHeader;

typedef enum : uint8_t {
  MTLB_VertexFunction = 0x00,
  MTLB_FragmentFunction = 0x01,
  MTLB_KernelFunction = 0x02,
} MTLB_FunctionType;

struct __attribute__((packed)) MTLB_TYPE_TAG {
  MTLBFourCC TAG = MTLB_Type;
  uint16_t TAG_SIZE = 1;
  uint8_t type;
};

struct __attribute__((packed)) MTLB_HASH_TAG {
  MTLBFourCC TAG = MTLB_Hash;
  uint16_t TAG_SIZE = 0x20;
  sha256_hash hash;
};

struct __attribute__((packed)) MTLB_OFFT_TAG {
  MTLBFourCC TAG = MTLB_Offset;
  uint16_t TAG_SIZE = 0x18;
  uint64_t PublicMetadataOffset;
  uint64_t PrivateMetadataOffset;
  uint64_t BitcodeOffset;
};

struct __attribute__((packed)) MTLB_VERS_TAG {
  MTLBFourCC TAG = MTLB_Version;
  uint16_t TAG_SIZE = 0x08;
  uint16_t airVersionMajor;
  uint16_t airVersionMinor;
  uint16_t languageVersionMajor;
  uint16_t languageVersionMinor;
};

struct __attribute__((packed)) MTLB_MDSZ_TAG {
  MTLBFourCC TAG = MTLB_Size;
  uint16_t TAG_SIZE = 0x08;
  uint64_t bitcodeSize;
};

#ifdef __cplusplus
};
#endif