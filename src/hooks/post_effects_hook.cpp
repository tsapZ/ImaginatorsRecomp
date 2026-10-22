// Strength of the game's bloom and motion blur, as in Swap Force and SuperChargers (their
// post_effects_hook.cpp; Cemu's Wii U packs offer the same).
//
// Imaginators keeps its bloom in the level's environment settings as SuperChargers does: one
// object per zone with 215 reflection fields (names at 0x820790B0, their u16 offsets at
// 0x82079770), _bloomLuminanceCutoff at +0x3A0 and _bloomScale, how much glow is added, at
// +0x3A4. sub_82A92408 blends the zones into the object at +2456 of its `this` each time it runs,
// every field from the sources afresh. Its last instruction before the exit
// (config/default.toml, 0x82A94FC0) stores into that object through r11; the hook there scales
// the blended _bloomScale. The early exit (nothing to blend) skips it, so the scale never
// compounds.
//
// Found 2026-10-22 from SuperChargers' function (the same three lerps of the field, here at
// +0x3A4 instead of +0x3B0) and the same field tables. Checked in the first level: the hook runs
// every frame (cutoff 5, scale 1 there, the same each time), and a cutoff of 0.1 set through it
// made the scene glow, so the pass uses this object; with the level's own cutoff of 5 little of the
// jungle is bright enough to glow, so the setting shows most in bright scenes.
//
// Motion blur, as Swap Force's option and the same way as SuperChargers' (its
// post_effects_hook.cpp): CMotionBlurMaterial keeps ig_motion_blur_exposure_params at +96 as
// loaded with the material; sub_82899870 hands it (and the tile and radius parameters at +112,
// +128) to the shader each time the material is bound, the only function using the parameter's
// handle (0x837DDB98, looked up by its name 0x82006E44 in sub_828A1D48), the same function as
// SuperChargers' sub_8360C7E8. The hook at its start writes the game's value times
// motion_blur_strength into +96 / +100, remembering the game's value so nothing compounds.

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <unordered_map>

#include <rex/cvar.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_DOUBLE(bloom_strength, 1.0, "Graphics",
                      "Glow around bright areas: 0 = off, 1 = the game's, up to 3");
// Off by default, as in Swap Force (the owner's choice there, 2026-10-19): the blur is tuned for
// 30 FPS and mostly smears at higher frame rates.
REXCVAR_DEFINE_DOUBLE(motion_blur_strength, 0.0, "Graphics",
                      "Motion blur: 0 = off (default), 1 = the game's, up to 2");

namespace {

constexpr uint32_t kBloomScale = 0x3A4;  // the environment object's _bloomScale

}  // namespace

// 0x82A94FC0: r11 = the blended environment object, its fields written.
void BloomStrengthHook(PPCRegister& r11) {
  const double scale = std::clamp(REXCVAR_GET(bloom_strength), 0.0, 3.0);
  if (scale == 1.0 || !r11.u32) return;
  uint8_t* p = rex::system::kernel_state()->memory()->virtual_membase() + r11.u32 + kBloomScale;
  rex::memory::store_and_swap<float>(p, float(rex::memory::load_and_swap<float>(p) * scale));
}

// 0x82A95A18: the join after sub_82A94FC8 blended one zone's _bloomScale into the result
// (r3 = its this, r4 = the zone). sub_82A94FC8 is the other blend: it moves each field of the result
// towards a zone's own value, result = result + (zone - result) * w, for the fields the zone sets
// (_blendAll at +110 or the field's state, +52 for _bloomScale), w at +2496 of this.
// Started from the scaled result, that leaves the zone's share unscaled; adding
// w * (scale - 1) * zone gives result + (scale * zone - result) * w, the scaled blend, whichever
// of the two functions ran before.
void BloomStrengthZoneHook(PPCRegister& r3, PPCRegister& r4) {
  const double scale = std::clamp(REXCVAR_GET(bloom_strength), 0.0, 3.0);
  if (scale == 1.0 || !r3.u32 || !r4.u32) return;
  uint8_t* base = rex::system::kernel_state()->memory()->virtual_membase();
  if (!base[r4.u32 + 110] && !base[r4.u32 + 52]) return;
  const uint32_t result = rex::memory::load_and_swap<uint32_t>(base + r3.u32 + 2456);
  if (!result) return;
  const double w = rex::memory::load_and_swap<float>(base + r3.u32 + 2496);
  const double zone = rex::memory::load_and_swap<float>(base + r4.u32 + 0x3A4);
  uint8_t* p = base + result + 0x3A4;
  const double now = rex::memory::load_and_swap<float>(p);
  rex::memory::store_and_swap<float>(p, float(now + w * (scale - 1.0) * zone));
}

// 0x82899870, the start of the motion blur material's parameter setter: r3 = the material. Its
// exposure parameter (+96, +100) becomes the game's value times motion_blur_strength.
void MotionBlurStrengthHook(PPCRegister& r3) {
  if (!r3.u32) return;
  struct Seen {
    float game[2];
    float written[2];
  };
  static std::mutex mutex;
  static std::unordered_map<uint32_t, Seen> seen;  // by material
  const float scale = float(std::clamp(REXCVAR_GET(motion_blur_strength), 0.0, 2.0));
  uint8_t* p = rex::system::kernel_state()->memory()->virtual_membase() + r3.u32 + 96;
  std::lock_guard lock(mutex);
  auto [it, fresh] = seen.try_emplace(r3.u32);
  Seen& s = it->second;
  for (int i = 0; i < 2; ++i) {
    const float now = rex::memory::load_and_swap<float>(p + 4 * i);
    if (fresh || now != s.written[i]) s.game[i] = now;
    s.written[i] = s.game[i] * scale;
    rex::memory::store_and_swap<float>(p + 4 * i, s.written[i]);
  }
}
