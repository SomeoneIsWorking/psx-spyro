#include "spyro3/render/hud_slide.h"

#include "core.h"
#include "core/boot_prefix_frame_driver.h"
#include "core/hud_slide.h"
#include "core/session.h"
#include "emit_memory.h"
#include "game.h"
#include "native_dispatch.h"
#include "projected_vertex.h"
#include "r3000.h"
#include "replay_frame.h"
#include "spyro3/render/hud_anchor.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <optional>

namespace spyro3::hud_slide {
namespace {

constexpr std::uint32_t kIconEmitter = 0x800289C8u;
constexpr std::uint32_t kSlideTable = 0x800674E8u;  // bytes, read at 0..0x3B by the icon
constexpr std::uint32_t kAcrossTable = 0x800674ACu; // the digits' x arm reads its bytes from here
constexpr std::uint32_t kGlyphImages = 0x8006C788u; // word: 8-byte glyph images
constexpr std::uint32_t kFirstDigit = 0x800719CCu;  // halfword: the image of digit 0
constexpr std::uint32_t kCursor = 0x8006C664u;
constexpr std::uint32_t kSlashImage = 0x50u; // 0x800295C0: ten images past digit 0
constexpr std::int32_t kSlashGap = 4;        // 0x8002959C
constexpr std::int32_t kLastIcon = 0x3B;
constexpr std::int32_t kAcross = 0x3C; // 0x800297DC: the x arms add it to the slide they hand on
constexpr std::uint32_t kPacketBytes = 0x28u;
constexpr std::array<std::uint32_t, 4> kCorners = {0x08u, 0x10u, 0x18u, 0x20u};

hud_anchor::DrawState &drawState(Core &core) {
  return static_cast<hud_anchor::DrawState &>(*spyro::session(core).titleState);
}

spyro::HudSlide &slideOf(Core &core) {
  return core.game->frameDriverAs<spyro::BootPrefixFrameDriver>().motion().hudSlide();
}

std::int32_t signedOf(std::uint32_t word) {
  return static_cast<std::int32_t>(word);
}

// The index every caller of FUN_80029708 hands it for a slide (0x80029874, 0x80029948, ...).
std::int32_t drawerIndex(std::int32_t slide) {
  return slide == 0 ? 0 : (slide + 10) >> 1;
}

// FUN_80029708's arms for one sample: the icon's move and the slide it hands the digits.
enum class Arm : std::uint8_t { None, Down, Across };

struct Step {
  std::int32_t index = 0;
  std::int32_t entry = 0;
  Arm arm = Arm::None;
  spyro::HudSlide::Place move;
  std::int32_t handed = 0;
};

Step stepOf(Core &core, const spyro::HudSlide::Sample &sample, std::int32_t index) {
  const std::uint8_t flags = sample.state;
  const std::int32_t slide = sample.slide;
  Step step;
  step.index = index < 0 ? 0 : (index > kLastIcon ? kLastIcon : index);
  step.entry = core.mem_r8(kSlideTable + static_cast<std::uint32_t>(step.index));
  step.handed = step.index;
  if ((flags & 1u) != 0) {
    step = {step.index, step.entry, Arm::Down, {.y = -step.entry}, -slide};
  } else if ((flags & 2u) != 0) {
    step = {step.index, step.entry, Arm::Down, {.y = step.entry}, slide};
  } else if ((flags & 4u) != 0) {
    step = {step.index, step.entry, Arm::Across, {.x = -2 * step.entry}, -(slide + kAcross)};
  } else if ((flags & 8u) != 0) {
    step = {step.index, step.entry, Arm::Across, {.x = 2 * step.entry}, slide + kAcross};
  }
  return step;
}

// FUN_80029708(widget, &index, &x, &y).
void slideStep(Core *core) {
  const std::uint32_t widget = core->r[4];
  const std::uint32_t indexAt = core->r[5];
  const std::uint32_t xAt = core->r[6];
  const std::uint32_t yAt = core->r[7];
  spyro::HudSlide &slide = slideOf(*core);
  const spyro::HudSlide::Sample now = slide.now(*core, widget);
  const auto index = signedOf(core->mem_r32(indexAt));
  const Step later = stepOf(*core, now, index);
  const spyro::HudSlide::Place at{.x = signedOf(core->mem_r32(xAt)),
                                  .y = signedOf(core->mem_r32(yAt))};
  spyro::HudSlide::Place shown{.x = at.x + later.move.x, .y = at.y + later.move.y};
  slide.settle();
  hud_anchor::DrawState &state = drawState(*core);
  state.dropCarry();
  const std::optional<float> t = core->runtime.replayFrame(*core)->replaying();
  const std::optional<spyro::HudSlide::Sample> before =
      t && index == drawerIndex(now.slide) ? slide.earlier(*core, widget) : std::nullopt;
  if (before) {
    const Step earlier = stepOf(*core, *before, drawerIndex(before->slide));
    shown =
        slide.between(*core, {.x = at.x + earlier.move.x, .y = at.y + earlier.move.y}, shown, *t);
    state.carry({.run = core->writeRollback.run(),
                 .sp = core->r[psx::cpu::kSp],
                 .handed = later.handed,
                 .earlier = earlier.handed});
  }
  core->mem_w32(indexAt, static_cast<std::uint32_t>(later.handed));
  core->mem_w32(xAt, static_cast<std::uint32_t>(shown.x));
  core->mem_w32(yAt, static_cast<std::uint32_t>(shown.y));
  // $v0 is the handed slide on every arm, $v1 the table entry or, across, the new x.
  core->r[2] = later.arm == Arm::None ? 0u : static_cast<std::uint32_t>(later.handed);
  switch (later.arm) {
  case Arm::None:
    core->r[3] = now.state;
    break;
  case Arm::Down:
    core->r[3] = static_cast<std::uint32_t>(later.entry);
    break;
  case Arm::Across:
    core->r[3] = static_cast<std::uint32_t>(shown.x);
    break;
  }
}

// A digit's table read: the slide its drawer was handed and the table index.
struct DigitAt {
  std::int32_t handed = 0;
  std::int32_t index = 0;
};

// A digit's move by the slide it was handed at its table index (0x800292C4..0x80029318).
spyro::HudSlide::Place digitMove(Core &core, DigitAt digit) {
  const std::int32_t handed = digit.handed;
  if (handed == 0) {
    return {};
  }
  const auto at = static_cast<std::uint32_t>(digit.index);
  if (((std::abs(handed) + 10) >> 1) - 1 < kAcross) {
    const std::int32_t entry = core.mem_r8(kSlideTable + at);
    return {.y = handed < 0 ? -entry : entry};
  }
  const std::int32_t entry = core.mem_r8(kAcrossTable + at);
  return {.x = handed < 0 ? -entry : entry};
}

// The digit images and their size (0x800291E0..0x8002922C).
struct Glyphs {
  std::uint32_t images = 0;
  std::int32_t first = 0;
  std::int32_t width = 0;
  std::int32_t half = 0;
};

Glyphs glyphsOf(Core &core) {
  Glyphs glyphs{.images = core.mem_r32(kGlyphImages),
                .first = static_cast<std::int16_t>(core.mem_r16(kFirstDigit))};
  const std::uint32_t image = glyphs.images + static_cast<std::uint32_t>(glyphs.first) * 8u;
  glyphs.width = core.mem_r8(image + 4) - core.mem_r8(image);
  glyphs.half = (core.mem_r8(image + 5) - core.mem_r8(image + 1)) >> 1;
  return glyphs;
}

std::int32_t carriesBeyond(std::int32_t value) {
  std::int32_t count = 0;
  for (; value > 9; value /= 10) {
    ++count;
  }
  return count;
}

// The glyphs of a digit drawer, rightmost first: glyph k sits (last - k) widths right of `x`,
// moved by the slide at table index lead - last + k.
class DigitRow {
public:
  // The drawer's x and y, the slide it was handed and its last glyph's index.
  struct Args {
    spyro::HudSlide::Place at;
    std::int32_t handed = 0;
    std::int32_t last = 0;
  };

  DigitRow(Core &core, const Args &args)
      : core_(core), glyphs_(glyphsOf(core)), x_(args.at.x), y_(args.at.y), handed_(args.handed),
        last_(args.last) {
    const std::optional<float> t = core.runtime.replayFrame(core)->replaying();
    const std::optional<std::int32_t> earlier =
        t ? drawState(core).takeCarry(core.writeRollback.run(), core.r[psx::cpu::kSp], handed_)
          : std::nullopt;
    if (t && earlier) {
      t_ = *t;
      earlier_ = earlier;
    }
  }

  [[nodiscard]] const Glyphs &glyphs() const {
    return glyphs_;
  }

  // Glyph k's place; inside a replay with the earlier slide carried, between the two.
  [[nodiscard]] spyro::HudSlide::Place place(std::int32_t k) {
    const std::int32_t offset = (last_ - k) * glyphs_.width;
    const auto at = [&](std::int32_t handed) {
      const spyro::HudSlide::Place move =
          digitMove(core_, {.handed = handed, .index = leadOf(handed) - last_ + k});
      return spyro::HudSlide::Place{.x = x_ + offset + move.x, .y = y_ + move.y};
    };
    const spyro::HudSlide::Place now = at(handed_);
    spyro::HudSlide &slide = slideOf(core_);
    if (!earlier_) {
      slide.settle();
      return now;
    }
    return slide.between(core_, at(*earlier_), now, t_);
  }

  // The emitter call for one glyph, its y lifted by half the digit height; false when the call
  // did not return.
  [[nodiscard]] bool emit(std::uint32_t image, spyro::HudSlide::Place place) {
    core_.r[4] = image;
    core_.r[5] = static_cast<std::uint32_t>(place.x);
    core_.r[6] = static_cast<std::uint32_t>(place.y - glyphs_.half);
    return psx::cpu::dispatchGuestOrPropagate(core_, kIconEmitter, "spyro3-hud-digits");
  }

private:
  static std::int32_t leadOf(std::int32_t handed) {
    return ((std::abs(handed) + 10) >> 1) - 1;
  }

  Core &core_;
  Glyphs glyphs_;
  std::int32_t x_;
  std::int32_t y_;
  std::int32_t handed_;
  std::int32_t last_;
  std::optional<std::int32_t> earlier_;
  float t_ = 1.0f;
};

std::uint32_t digitImage(const Glyphs &glyphs, std::int32_t digit) {
  return glyphs.images + static_cast<std::uint32_t>(glyphs.first + digit) * 8u;
}

// FUN_800293C4(a, b, x, y, slide): b's digits, a slash, then a's, drawn right to left.
void drawRatio(Core *core) {
  const auto a = signedOf(core->r[4]);
  const auto b = signedOf(core->r[5]);
  const std::int32_t last = carriesBeyond(a) + 2 + carriesBeyond(b);
  const auto handed = signedOf(core->mem_r32(core->r[psx::cpu::kSp] + 0x10u));
  DigitRow row(*core,
               {.at = {.x = signedOf(core->r[6]), .y = signedOf(core->r[7])},
                .handed = handed,
                .last = last});
  const Glyphs glyphs = row.glyphs();
  std::int32_t shown = b;
  for (std::int32_t k = 0; k <= last; ++k) {
    if (shown == 0) {
      shown = -100;
    } else if (shown < 0) {
      shown = a;
    }
    spyro::HudSlide::Place place = row.place(k);
    std::uint32_t image = 0;
    if (shown < 0) {
      place.x += kSlashGap;
      image = glyphs.images + static_cast<std::uint32_t>(glyphs.first) * 8u + kSlashImage;
    } else {
      image = digitImage(glyphs, shown % 10);
    }
    if (!row.emit(image, place)) {
      return;
    }
    shown /= 10;
  }
  core->r[2] = static_cast<std::uint32_t>((last + 1) * glyphs.width);
}

} // namespace

// FUN_800291B8(value, x, y, slide): the value's digits, right to left.
void drawValue(Core &core) {
  std::int32_t value = signedOf(core.r[4]);
  const std::int32_t last = carriesBeyond(value);
  DigitRow row(core,
               {.at = {.x = signedOf(core.r[5]), .y = signedOf(core.r[6])},
                .handed = signedOf(core.r[7]),
                .last = last});
  const Glyphs glyphs = row.glyphs();
  for (std::int32_t k = 0; k <= last; ++k) {
    if (!row.emit(digitImage(glyphs, value % 10), row.place(k))) {
      return;
    }
    value /= 10;
  }
  core.r[2] = static_cast<std::uint32_t>((last + 1) * glyphs.width);
}

std::uint32_t primitiveCursor(Core &core) {
  return core.mem_r32(kCursor);
}

void placeEmitted(Core &core, std::uint32_t firstPacket) {
  const spyro::HudSlide::Remainder rest = slideOf(core).takeRemainder(core);
  if (rest.x == 0.0f && rest.y == 0.0f) {
    return;
  }
  const psx::present::EmitMemory memory(core);
  const std::uint32_t end = primitiveCursor(core);
  for (std::uint32_t packet = firstPacket; packet < end; packet += kPacketBytes) {
    for (const std::uint32_t corner : kCorners) {
      const std::uint32_t word = memory.mem_r32(packet + corner);
      GtePlace place = psx::gte::ProjectedVertex::whole(word).place();
      place.x += rest.x;
      place.y += rest.y;
      memory.storeVertex(packet + corner, psx::gte::ProjectedVertex(word, place));
    }
  }
}

void registerOverrides(Core &core) {
  psx::cpu::installNativeOverride(core, kSlideStep, "spyro3-hud-slide-step", slideStep);
  psx::cpu::installNativeOverride(core, kRatioDrawer, "spyro3-hud-ratio", drawRatio);
}

} // namespace spyro3::hud_slide
