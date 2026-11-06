// Always crit, elemental power everywhere, gold and XP multipliers, as cheats (CoL #48, TT #20 and
// the multipliers asked for in CoL #48, darkmattereli-hue). The same as Swap Force's
// (SwapForceRecomp src/hooks/attribute_cheats_hook.cpp); Imaginators's addresses.
//
// Imaginators keeps a Skylander's stats in character attributes (the enum's names at 0x82043A00 (eCA_XpLuck 29, eCA_GoldLuck 30, eCA_CriticalHitChance 43)). Found from the getters' shape, the same as in SuperChargers (their script names getGoldMultiplier and getXpMultiplier sit in a table outside the image's first 16 MB), and the crit roll's random numbers (Park-Miller, 16807) next to the crit flag 0x800:
// - The damage code sub_82F40670 adds the attacker's critical hit chance (sub_82F39B20, with its
//   bonus against the target's kind) to the attack's own and rolls a number from 0 to 100 against
//   the sum; below it, the hit is critical. Reached only for an attacker with the attribute
//   component, so not for enemies.
// - sub_82F1E698: whether the Skylander's element is one of the area's; only the attribute bonus
//   functions call it, which then add the eCA_*ElementalBoost attributes.
// - sub_82F39A30 and sub_82F39940: the gold and XP multipliers (luck, its element bonus, the catch-up for a
//   Skylander below the area's level); the pickup code sub_82F45FC8 multiplies a pickup by them.
//
// Four midasm hooks (config/default.toml), each doing nothing while its cheat is off. Nothing is
// written to the game's memory; the gold and the levels are saved to the figure by the game.

#include <algorithm>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>

#include "portal_rex/achievements_ui.h"

REXCVAR_DEFINE_BOOL(cheat_always_crit, false, "Extras",
                    "Every hit of your Skylanders is a critical hit");
REXCVAR_DEFINE_BOOL(cheat_elemental_power, false, "Extras",
                    "Your Skylanders get their element's bonus in every area, as if each area were "
                    "theirs");

namespace {

void LogOnce(bool& logged, const char* what, double before, double after) {
  if (!logged) {
    logged = true;
    REXLOG_INFO("Extras: {} ({:.2f} -> {:.2f})", what, before, after);
  }
}

}  // namespace

// Before 0x82F40718 in sub_82F40670 (fadds f30,f1,f30): f1 is the attacker's chance,
// just returned by sub_82F39B20. With 100 added, the roll (0 to 100, never 100) always lands below.
void AlwaysCritHook(PPCRegister& f1) {
  if (!REXCVAR_GET(cheat_always_crit)) return;
  static bool logged = false;
  const double before = f1.f64;
  f1.f64 = std::max(f1.f64, 100.0);
  LogOnce(logged, "always crit raised a critical hit chance", before, f1.f64);
}

// Before 0x82F1E764 in sub_82F1E698, after mr r3,r31: r3 is whether the element is the area's.
void ElementalPowerHook(PPCRegister& r3) {
  if (!REXCVAR_GET(cheat_elemental_power)) return;
  static bool logged = false;
  LogOnce(logged, "elemental power made an area's element check pass", double(r3.u64 & 0xFF), 1.0);
  r3.u64 = 1;
}

// Before the blr of sub_82F39A30: f1 is the gold multiplier.
void GoldMultiplierHook(PPCRegister& f1) {
  const int k = skylanders::CheatGoldMultiplier();
  if (k <= 1) return;
  static bool logged = false;
  const double before = f1.f64;
  f1.f64 = double(float(f1.f64 * k));
  LogOnce(logged, "the gold multiplier scaled the game's own", before, f1.f64);
}

// Before the blr of sub_82F39940: f1 is the XP multiplier.
void XpMultiplierHook(PPCRegister& f1) {
  const int k = skylanders::CheatXpMultiplier();
  if (k <= 1) return;
  static bool logged = false;
  const double before = f1.f64;
  f1.f64 = double(float(f1.f64 * k));
  LogOnce(logged, "the XP multiplier scaled the game's own", before, f1.f64);
}
