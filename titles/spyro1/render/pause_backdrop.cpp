#include "spyro1/render/pause_backdrop.h"

#include "core.h"
#include "frame_record.h"
#include "gpu_device.h"
#include "native_dispatch.h"

#include <cstdint>

namespace spyro1::pause_backdrop {
namespace {

constexpr std::uint32_t kGreyConversion = 0x80017E98u;
constexpr std::uint32_t kCurrentBuffer = 0x80075888u; // g_CurDB
constexpr std::uint32_t kFirstBuffer = 0x80076EE0u;   // &g_DB[0]

// func_8001A40C's StoreImage rows: the displayed buffer's 224 lines from row 8 or 248.
constexpr int kReadWidth = 512;
constexpr int kReadHeight = 224;
constexpr int kFirstBufferRow = 8;
constexpr int kSecondBufferRow = 248;

// Its LoadImage page: 64 halfwords (128 8-bit texels) per strip from (512, 0).
constexpr int kPageX = 512;
constexpr int kPageWidth = 256;
constexpr int kEightBitMode = 1;

// func_80017E98's index: (4r + 3g + b) >> 3 of the 5-bit channels.
constexpr std::uint8_t kRedWeight = 4;
constexpr std::uint8_t kGreenWeight = 3;
constexpr std::uint8_t kBlueWeight = 1;
constexpr std::uint8_t kIndexShift = 3;

void convertStrip(Core *core) {
  // Before the call: the conversion can suspend and return to the guest caller directly.
  const int readY =
      core->mem_r32(kCurrentBuffer) != kFirstBuffer ? kSecondBufferRow : kFirstBufferRow;
  core->gpuDevice().declareReadBack({.readX = 0,
                                     .readY = readY,
                                     .readWidth = kReadWidth,
                                     .readHeight = kReadHeight,
                                     .pageX = kPageX,
                                     .pageY = 0,
                                     .pageWidth = kPageWidth,
                                     .pageHeight = kReadHeight,
                                     .texMode = kEightBitMode,
                                     .red = kRedWeight,
                                     .green = kGreenWeight,
                                     .blue = kBlueWeight,
                                     .shift = kIndexShift});
  (void)psx::cpu::callOriginalOrPropagate(*core, kGreyConversion);
}

} // namespace

void registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(
      core, kGreyConversion, "spyro1-pause-backdrop-grey", convertStrip);
}

} // namespace spyro1::pause_backdrop
