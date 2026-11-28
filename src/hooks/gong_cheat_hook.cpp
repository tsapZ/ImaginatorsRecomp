// The battle gong for every Skylander, as a cheat (Imaginators #20).
//
// Every story level has a battle gong that only a Sensei can ring (the battle it starts is part of
// the Sensei's own play). Its script asks the script binding isSensei (Sky-Chi class, name at
// 0x820D3AA0, called through the wrapper sub_82B3DCD8), whose native is sub_82DFA750: it calls
// sub_82D1D220 ("is this toy a Sensei": toy id 600-650 at +20 of its toy data) and stores the answer
// byte for the script and its negation next to it.
//
// One midasm hook (config/default.toml) at 0x82DFA790, the clrlwi right after that call: with
// cheat_gong_for_everyone on, the answer there is yes. That is every script's isSensei, not only
// the gong's. Watched in test runs (developer kit, Chop Chop, 2026-11-18 and 2026-11-28): the gong
// asks about three times a second near it; at a figure's arrival in a level ten scripts ask once
// each. With yes the gong lit up for Chop Chop as for a real Sensei (Ambush, the same route), no
// Sky-Chi meter appeared, and Chop Chop's figure file after the run was byte for byte the one of the
// same run with the cheat off. The native code's own callers of sub_82D1D220 (nine more: the portal,
// the figure arrival) keep the real answer. Nothing is written to the game's memory; with the cheat
// off the hook does nothing.

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

#include <atomic>
#include <cstdint>

REXCVAR_DEFINE_BOOL(cheat_gong_for_everyone, false, "Extras",
                    "Every Skylander can ring the battle gong, not only Senseis (achievements don't unlock while "
                    "it's on)");

// clrlwi r11,r3,24 right after bl sub_82D1D220 in isSensei's native: r3 is its "is a Sensei" answer.
void GongSenseiHook(PPCRegister& r3) {
  if (!REXCVAR_GET(cheat_gong_for_everyone)) return;
  // The game reads only the low byte (clrlwi r11,r3,24 here, stb r3 later).
  static std::atomic<bool> logged{false};
  if ((r3.u32 & 0xFF) == 0 && !logged.exchange(true)) {
    REXLOG_INFO("Gong for everyone: a non-Sensei counts as a Sensei");
  }
  r3.u64 = (r3.u64 & ~uint64_t(0xFF)) | 1;
}
