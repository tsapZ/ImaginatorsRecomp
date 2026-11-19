// More room in the game's main heap, for mods with bigger characters (Core of Light #56, 2026-11-19: SkyP4tcher's
// imagipatcher turns the player's own Wii U Imaginators files into Donkey Kong and his Barrel Blaster; their data needs
// bigger character and vehicle budgets in MemoryPoolConfigs.igx, which sit inside MP_GAME_MODE_RESIZABLE, and a bigger
// MP_GAME_MODE_RESIZABLE no longer fit: the stock game crashes at startup with only +1 MB there).
//
// The game's memory setup, sub_828E74D0, stores fixed sizes for its big physical allocations, among them 0x09558000 at
// 0x837D4B30 (its store at 828E755C): the heap the memory pools of MemoryPoolConfigs.igx are cut from, allocated right
// after (MmAllocatePhysicalMemoryEx of 0x9560000, seen with a TEMP DIAG on every allocation of 1 MB and up,
// 2026-11-19; the others are 0x86B800, 0xCA4800, 0x87F000 and 0x10000000; sub_8248E3F8 reads the word back and
// creates the pool with it). The setup then creates the .igx pools from that heap, MP_GAME_MODE_RESIZABLE (id 0x52) as
// the 17th of 39 (a TEMP DIAG on sub_828E7638); with it 1 MB bigger the stock game crashed inside that creation (read of
// guest 0x8 in sub_824E1230 at startup). This setting adds its bytes to the heap right after the game stored the size;
// 0 (the default) leaves the game as it is.
//
// Tested on a scratch copy of the game files (kit\im56_heap*.ps1: the save copy, the overland map, level_501): +1 MB
// on MP_GAME_MODE_RESIZABLE needs 2 MB here (1 MB still crashed: the heap has almost no slack), +3 MB runs with 4 MB.
// So: 1 MB more than the pool grows (Donkey Kong and the Barrel Blaster, +3 MB: 0x400000). The port's physical memory
// is the console's 512 MB and the game's big allocations take about 441 MB of it, so a few MB are free.

#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>

// At most 32 MB (a typo like 0x3000000, 48 MB, would leave the game without memory for its other allocations); read
// once at the start. Hex works in the settings file (TOML numbers); on the command line give it in decimal (3145728 for
// 0x300000).
REXCVAR_DEFINE_INT32(game_heap_extra, 0, "Mods",
                     "Extra bytes for the game's main heap, for mods that raise MemoryPool_xenon_MP_GAME_MODE_RESIZABLE "
                     "in MemoryPoolConfigs.igx: 1 MB more than they add there (Donkey Kong and the Barrel Blaster "
                     "from the Wii U game: 0x400000); 0 = the game's own 0x9558000")
    .range(0, 0x2000000)
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

namespace {
constexpr uint32_t kGameHeapSize = 0x837D4B30;
}  // namespace

REX_EXTERN(__imp__sub_828E74D0);  // the game's memory setup (original)

REX_HOOK_RAW(sub_828E74D0) {
  __imp__sub_828E74D0(ctx, base);
  const int32_t extra = REXCVAR_GET(game_heap_extra);
  if (extra <= 0) return;
  // (64 KB pages: the allocation rounds up anyway)
  const uint32_t more = (uint32_t(extra) + 0xFFFF) & ~0xFFFFu;
  const uint32_t game = rex::memory::load_and_swap<uint32_t>(base + kGameHeapSize);
  rex::memory::store_and_swap<uint32_t>(base + kGameHeapSize, game + more);
  REXLOG_INFO("Mods: game heap {:#x} bytes (the game's {:#x}, +{:#x})", game + more, game, more);
}
