// Where the player is, for problem reports (skylanders-portal dev/where.h).
//
// The level: the level archive the game opened last (rom\archives: Level_500 to Level_512,
// Level_Crash, Hub_2016, EA_01 to EA_10, PU_*, the adventure packs APA1-4, APL1-4, APS1-4; their
// sound banks, cutscenes and figure parts are other archives).
//
// The player: CPlayerJumpComponent's stuck-falling check, sub_82EEB308, runs once a frame for every
// player (from the component's update sub_82EEB628; stuck_falling_hook.cpp has midasm hooks inside
// it). The component's +12 is its entity, and the entity's +16, +20, +24 its position as the game
// reads it (sub_825FAF58), relative to the entity's parent at +8 when it has one. Found 2026-10-29;
// the first player checked of those seen in the last half second is player 1 (ReportPlayerFrame).

#include "hooks/where_hook.h"

#include <cstdint>
#include <regex>
#include <string>
#include <string_view>

#include <rex/hook.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>

#include "dev/where.h"

namespace {

uint32_t Load32(uint8_t* base, uint32_t address) { return rex::memory::load_and_swap<uint32_t>(base + address); }
float LoadFloat(uint8_t* base, uint32_t address) { return rex::memory::load_and_swap<float>(base + address); }
bool GuestPointer(uint32_t v) { return v >= 0x40000000 && v < 0xC0000000; }

// "game:\archives\Level_503.pak" -> "Level_503"; "" for any other file.
std::string LevelOfArchive(std::string_view path) {
  static const std::regex level(R"((?:^|[\\/])((?:Level_(?:\d+|Crash)|Hub_2016|EA_\d+|PU_\d+|AP[ALS]\d))\.pak$)",
                                std::regex::icase);
  std::cmatch m;
  if (!std::regex_search(path.data(), path.data() + path.size(), m, level)) return {};
  return m[1].str();
}

}  // namespace

namespace imaginatorsrecomp {

void InstallWhere() {
  skylanders::dev::WatchLevelArchives(&LevelOfArchive);
}

}  // namespace imaginatorsrecomp

extern "C" void __imp__sub_82EEB308(PPCContext& __restrict ctx, uint8_t* base);

REX_HOOK_RAW(sub_82EEB308) {
  const uint32_t component = ctx.r3.u32;
  const uint32_t entity = GuestPointer(component) ? Load32(base, component + 12) : 0;
  if (GuestPointer(entity)) {
    skylanders::dev::ReportPlayerFrame(component, LoadFloat(base, entity + 16), LoadFloat(base, entity + 20),
                                       LoadFloat(base, entity + 24), Load32(base, entity + 8) != 0);
  }
#if defined(SKYLANDERS_DEVKIT)
  // The developer kit (a private test build only) does its per-player work here as well.
  DevKitPlayerCheck(ctx, base);
#endif
  __imp__sub_82EEB308(ctx, base);
}
