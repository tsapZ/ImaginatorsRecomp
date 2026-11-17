// Skip talks (F4 > Game): B or Y skips every cutscene, also those the game only lets you skip on a replay (the owner's
// wish 2026-11-16, "die labern zu viel", and 2026-11-17 "mach B einfach fest"; the game's own skip button is Y).
//
// Vicarious Visions' developer switch "auto skip cutscenes" is byte +74 of the object behind the word 0x837F3E74 (the
// scripts' native AutoSkipCutscenes, sub_82A62F10 -> sub_829AEFF8, reads it). The scene update sub_82A4EA68 tests the
// same switch itself, inlined: at 0x82A4EE60 r11 is 1 when it is set (or the second, timer-based test passes), and
// then a scene in state 3 (+60 of r28) is ended. Two readers of the byte in the whole program (the native and this).
// Setting the byte 3 s into level_501's opening ended the scene within 3 s where it otherwise runs past 8 s
// (im_autoskip_test.ps1, 2026-11-17 22:01). The update tests it every frame, in the levels and hubs too. With
// skip_talks on, config/default.toml's midasm hook at 0x82A4EE60 makes r11 1 for 100 ms after a new press of B or Y:
// that ends a playing scene and does nothing without one (tested 22:49: level_501's opening ended at the first B; without
// the option B only brings up the game's "press Y to skip"). The presses come from the lib's own collector for this
// (TakeNewSkipPresses), so the movie skip keeps its presses. Nothing is written to the game.

#include <chrono>
#include <cstdint>
#include <mutex>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

#include "portal_rex/portal_menu_input.h"

REXCVAR_DEFINE_BOOL(skip_talks, false, "Game",
                    "B or Y skips every cutscene, also those the game only lets you skip on a replay");

namespace {

constexpr uint16_t kSkipButtons = 0x2000 | 0x8000;  // X_INPUT_GAMEPAD_B | X_INPUT_GAMEPAD_Y
using Clock = std::chrono::steady_clock;

std::mutex g_mutex;
Clock::time_point g_last_test{};
Clock::time_point g_skip_until{};

}  // namespace

void SkipTalksAutoSkipHook(PPCRegister& r11) {
  if (!REXCVAR_GET(skip_talks)) return;
  const Clock::time_point now = Clock::now();
  std::lock_guard lock(g_mutex);
  const bool new_run = now - g_last_test > std::chrono::milliseconds(250);
  g_last_test = now;
  const uint16_t presses = skylanders::menu_input::TakeNewSkipPresses();
  if (!new_run && (presses & kSkipButtons) && !skylanders::menu_input::MenuOpen()) {
    g_skip_until = now + std::chrono::milliseconds(100);  // the update asks every frame
    REXLOG_INFO("Skip talks: {} skips the scene", (presses & 0x2000) ? "B" : "Y");
  }
  if (now < g_skip_until) r11.u64 = 1;
}
