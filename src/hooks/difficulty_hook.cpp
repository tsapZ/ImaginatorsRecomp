// Nightmare ("Albtraum") from the start, as an option (SwapForceRecomp #74, same as there).
//
// The game unlocks its hardest difficulty only after Story Mode was finished once. The difficulty
// screen (sub_8317D680 builds the list, sub_8317D888 handles a choice) marks the level 3 row
// (eDL_Nightmare) as locked unless sub_829BE430 says yes. sub_829BE430 reads the profile value
// "nightmare_difficulty_unlocked" (0x82061324, through sub_82539018) and answers whether it is 1;
// sub_829C4350 writes the 1 when the game unlocks it. The same two functions as Swap Force's
// sub_829CC788 / sub_829CC750.
//
// With nightmare_from_start on, sub_829BE430 answers yes without reading. Nothing is written: the
// profile keeps its own value, so with the setting off again Nightmare is locked until the game
// unlocks it the usual way. Found 2026-10-25 from the profile key's name to its getter and its two
// callers on the difficulty screen.

#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

REXCVAR_DEFINE_BOOL(nightmare_from_start, false, "Game",
                    "Nightmare difficulty can be picked from the start, without finishing Story Mode "
                    "first (the save is not changed)");

REX_EXTERN(__imp__sub_829BE430);  // whether Nightmare is unlocked (original)

REX_HOOK_RAW(sub_829BE430) {
  if (REXCVAR_GET(nightmare_from_start)) {
    static bool logged = false;
    if (!logged) {
      logged = true;
      REXLOG_INFO("Nightmare difficulty open from the start (nightmare_from_start)");
    }
    ctx.r3.u64 = 1;
    return;
  }
  __imp__sub_829BE430(ctx, base);
}
