// The battle gong for every Skylander, as a cheat (Imaginators #20).
//
// Every story level has a battle gong that only a Sensei can ring (the battle it starts is part of
// the Sensei's own play). Its script asks the native sub_82DFA750 whether the Skylander next to it
// is a Sensei: that native calls sub_82D1D220 ("is this toy a Sensei": toy id 600-650 at +20 of its
// toy data; ten callers in all, the portal and the figure arrival among them) and stores the answer
// byte for the script and its negation next to it.
//
// One midasm hook (config/default.toml) at 0x82DFA790, the clrlwi right after that one call: with
// cheat_gong_for_everyone on, the answer there is yes. The other nine callers keep the real answer,
// so nothing else takes a non-Sensei for a Sensei (asked ten times at every figure arrival).
//
// Found 2026-11-18 with a TEMP DIAG on isSensei's callers (branch diag/im-gong, never shipped): the
// gong's caller is the one at 0x82DFA790, about three times a second while a player stands near a
// gong (every story level has one, igz_gem_spots.py --name gong); with yes there the gong lit up for
// Chop Chop exactly as for a real Sensei (Ambush, control run). Nothing is written to the game's
// memory; with the cheat off the hook does nothing.

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

#include <atomic>

REXCVAR_DEFINE_BOOL(cheat_gong_for_everyone, false, "Extras",
                    "Every Skylander can ring the battle gong, not only Senseis (achievements don't unlock while "
                    "it's on)");

// clrlwi r11,r3,24 right after bl sub_82D1D220 in the gong's native: r3 is its "is a Sensei" answer.
void GongSenseiHook(PPCRegister& r3) {
  if (!REXCVAR_GET(cheat_gong_for_everyone)) return;
  static std::atomic<bool> logged{false};
  if (r3.u32 == 0 && !logged.exchange(true)) REXLOG_INFO("Gong for everyone: a non-Sensei may ring the gong");
  r3.u64 = 1;
}
