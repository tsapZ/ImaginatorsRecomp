// A shorter figure intro when a Skylander comes onto the portal (Swap Force #60, option, off by
// default), as Swap Force's "Skylander-Vorstellung: Kurz".
//
// When a figure is placed, the game's magic moment (the object at 0xB5CA53B0 in the scripted runs)
// shows it with its name for about 4 s before you can play. Two checks end the intro:
//
// - sub_829FF1F0 checks every frame whether the intro's time is up: it takes the intro's length in
//   f31 (the magic moment's tuning at +112, +236, unless the figure's own intro data has one at
//   +136), compares the time since the intro began (the counter at +232 times the constant at
//   0x82016A70) against it at 0x829FF268 (fcmpu cr6,f0,f31) and, when it is longer, fires an event
//   on the figure (sub_82DF20E8) and marks the time done (+554). That alone does not end the intro
//   (the game went on at the same time, Swap Force #60).
// - sub_82A4BC28, the magic moment's update while the intro plays (its state at +68 is 2), ends it
//   when sub_82D90538 says the figure's intro animation is over: then it sets the state to 4 and
//   the figure goes into the level. It asks at 0x82A4C110 for one player and at 0x82A4BE54 for each
//   figure with several players.
//
// With figure_intro_seconds set, the first hook shortens the length the time check compares
// against, and the second one answers "over" for the animation once the intro has run that long,
// so the game's own ending runs from there, camera and HUD included. The same as SuperChargers'
// (its sub_82A135B8 and sub_82E08770).
//
// Found 2026-10-22: the time check as the function shaped like Swap Force's intro check
// (sub_829BFFF8 there); the end with a write watch on the magic moment's state (+68, found from the
// state dispatcher sub_82A586F0 that calls the time check) during a scripted placement: its writer
// after the intro was sub_82A4BC28 itself (4), once the animation check is true.

#include <cstdint>

#include <rex/cvar.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_DOUBLE(figure_intro_seconds, 0.0, "Game",
                      "How long a Skylander's intro may last when it comes onto the portal, in "
                      "seconds (0: as long as the game's, about 4 s)");

namespace {

constexpr uint32_t kIntroCounter = 232;        // in the magic moment; -1 before the intro
constexpr uint32_t kCounterStep = 0x82016A70;  // seconds per count, as sub_829FF1F0 reads it

}  // namespace

// Before 0x829FF268 (fcmpu cr6,f0,f31): f31 is the intro's length. Only f31 changes, and only to a
// shorter length; f31 is saved by the function and not read after the comparison.
void FigureIntroLengthHook(PPCRegister& f31) {
  const double limit = REXCVAR_GET(figure_intro_seconds);
  if (limit > 0.0 && f31.f64 > limit) f31.f64 = limit;
}

// After the call to sub_82D90538 (0x82A4C114 and 0x82A4BE58, clrlwi r9,r3,24 next): r3 its answer,
// the intro animation over in the low byte; r27 the magic moment. Only a "not over" becomes
// "over", and only once the intro has run figure_intro_seconds.
void FigureIntroEndHook(PPCRegister& r3, PPCRegister& r27) {
  const double limit = REXCVAR_GET(figure_intro_seconds);
  if (limit <= 0.0 || (r3.u32 & 0xFF) || !r27.u32) return;
  uint8_t* base = rex::system::kernel_state()->memory()->virtual_membase();
  const uint32_t counter = rex::memory::load_and_swap<uint32_t>(base + r27.u32 + kIntroCounter);
  if (counter == 0xFFFFFFFF) return;
  const float step = rex::memory::load_and_swap<float>(base + kCounterStep);
  if (double(float(counter) * step) >= limit) r3.u64 = 1;
}
