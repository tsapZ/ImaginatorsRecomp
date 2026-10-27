// The portraits of the player's own Imaginators (creation crystals) were black
// (ImaginatorsRecomp #15).
//
// The game renders the Imaginator into a full-screen render target, resolves it, and copies the
// picture out on the CPU with the XDK's XGUntileSurface (sub_83646FB8: r3 destination, r4 row
// pitch, r5 destination point, r6 source, r7 width, r8 height, r9 source rect, r10 bytes per
// texel) to build the portrait. On the 360 a resolve lands in main memory; here it stays in the
// GPU's copy unless it is read back, so the copy came out empty (Cemu shows the same). Reading back
// every resolve of every frame (readback_resolve) fixes it but costs about a fifth of the frame
// rate, and the target's address changes from run to run. Instead this asks the GPU for that one
// surface right before the copy (ReadBackResolvedMemory: only what was resolved there in the last
// frames), once per portrait. Found 2026-10-27 with guard pages on every resolved range: the only
// CPU reader is this call, while a level loads.

#include <cstdint>

#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/runtime.h>
#include <rex/system/interfaces/graphics.h>
#include <rex/system/xmemory.h>

REX_EXTERN(__imp__sub_83646FB8);  // XGUntileSurface (original)

REX_HOOK_RAW(sub_83646FB8) {
  if (auto* runtime = rex::Runtime::instance()) {
    auto* memory = runtime->memory();
    auto* graphics = runtime->graphics_system();
    // The whole tiled source surface: both sides padded to 32 texels, in whole pages.
    const uint64_t width = (ctx.r7.u32 + 31) & ~31u, height = (ctx.r8.u32 + 31) & ~31u;
    const uint64_t length = (width * height * ctx.r10.u32 + 4095) & ~uint64_t(4095);
    const uint32_t physical = memory ? memory->GetPhysicalAddress(ctx.r6.u32) : UINT32_MAX;
    if (graphics && physical != UINT32_MAX && length && length <= (64u << 20) &&
        graphics->ReadBackResolvedMemory(physical, uint32_t(length))) {
      static uint32_t count = 0;
      if (++count <= 8) {
        REXLOG_INFO("Portrait: read the rendered picture back from the GPU ({:#010x}, {} KB)",
                    physical, length >> 10);
      }
    }
  }
  __imp__sub_83646FB8(ctx, base);
}
