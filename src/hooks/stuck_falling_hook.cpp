// The game's "stuck while falling" check at any frame rate, as in Swap Force and SuperChargers
// (SuperChargersRecomp src/hooks/stuck_falling_hook.cpp, the same check one game earlier).
//
// CPlayerJumpComponent's update, sub_82EEB628, calls sub_82EEB308 every frame: a falling player
// (state +0x1C == 3, not in hang time +0xE4) who moved less than a distance since the last frame
// runs the component's stuck timer (_stuckFallingTimer, a timer object at +0xA0), and once it ran
// longer than CPlayerSystemData's _stuckFallingTime (+0x64) the game puts the player back
// (sub_82EE35C0 with 5). The distance is the component's _stuckFallingDistanceOverride (+0x100)
// if it is set, else CPlayerSystemData's _stuckFallingDistanceThreshold (+0x60) times the
// component's +0x78 (this game added the override). Either way it is a distance per frame, tuned
// for 30 FPS: this game runs at 60 by default and faster with frame_rate_limit, where more of a
// fall counts as stuck and a long drop ends with the player put back (measured in SuperChargers,
// which has the same check without the override: at the default 60 FPS too).
//
// Here the distance is scaled by the game's own frame time (30 * dt, at most 1), so it means the
// same speed as at 30 FPS at any frame rate. dt is the update context's +0x14: sub_82EEB628 keeps
// the context (its third argument) in r25, which sub_82EEB308 doesn't touch, and passes it to the
// component's other per-frame functions, which read their dt there (sub_82EBE228: lfs f24,20(r29)).
//
// Found 2026-10-23: the field names and offsets from the game's reflection tables
// (CPlayerSystemData's names at 0x820BA658, its offsets after the fields' types at 0x820BA6C8;
// CPlayerJumpComponent's names at 0x820B8030, types at 0x820B8168), and sub_82EEB308 as the
// function reading the system data's +0x60 and +0x64 and the component's timer; it has the shape
// of SuperChargers' sub_82F949B8.
// Measured 2026-10-23 with log_stuck_falling in the first level at about 155 FPS, the player
// launched with the component's bounce pad fields (+0x3C on, +0x48 up speed, +0x4C / +0x50 gravity
// 1): without the fix launches at 5000 and 8000 ended with the player put back after 3 s (normal
// jumps counted as stuck 0.43-0.45 s); with it never (normal jumps 0.06-0.35 s).

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>

REXCVAR_DEFINE_BOOL(fix_stuck_falling, true, "Game",
                    "Above 30 FPS, keep the game from taking a normal fall for being stuck (its "
                    "check counts the distance per frame, tuned for 30 FPS)");
REXCVAR_DEFINE_BOOL(log_stuck_falling, false, "Game",
                    "Log how long each fall counted as stuck (for testing fix_stuck_falling)");

namespace {

using Clock = std::chrono::steady_clock;

float LoadFloat(uint32_t guest_address) {
  return rex::memory::load_and_swap<float>(
      rex::system::kernel_state()->memory()->virtual_membase() + guest_address);
}

// log_stuck_falling: per component, the current run of checks that counted as stuck.
struct Run {
  uint32_t component = 0;
  Clock::time_point since{};
  Clock::time_point last{};
  bool stuck = false;
};

std::mutex g_runs_mutex;
std::array<Run, 8> g_runs;

Run& RunFor(uint32_t component) {
  for (auto& r : g_runs) {
    if (r.component == component) return r;
  }
  for (auto& r : g_runs) {
    if (!r.stuck) {
      r = {component};
      return r;
    }
  }
  g_runs[0] = {component};
  return g_runs[0];
}

void EndRun(Run& r) {
  if (!r.stuck) return;
  r.stuck = false;
  const double seconds = std::chrono::duration<double>(r.last - r.since).count();
  if (seconds >= 0.05) {
    REXLOG_INFO("Stuck falling: counted as stuck for {:.2f} s (component {:08X})", seconds,
                r.component);
  }
}

}  // namespace

// Before 0x82EEB3A8 (fmuls f0,f0,f0): f0 = the distance (the override or the threshold times the
// factor), f13 = the squared distance since the last frame, r25 = the caller's update context (dt
// at +0x14), r30 = CPlayerSystemData, r31 = the component.
void StuckFallingThresholdHook(PPCRegister& f0, PPCRegister& f13, PPCRegister& r25,
                               PPCRegister& r30, PPCRegister& r31) {
  const float dt = LoadFloat(r25.u32 + 0x14);
  const double scale = dt > 0.0f && dt < 1.0f ? std::min(1.0, double(dt) * 30.0) : 1.0;
  static std::atomic<bool> logged{false};
  if (!logged.exchange(true)) {
    REXLOG_INFO("Stuck falling: distance {} per frame (threshold {}, override {}), time {} s, dt "
                "{:.4f}, frame scale {:.3f} (component {:08X})", f0.f64, LoadFloat(r30.u32 + 0x60),
                LoadFloat(r31.u32 + 0x100), LoadFloat(r30.u32 + 0x64), dt, scale, r31.u32);
  }
  if (REXCVAR_GET(fix_stuck_falling)) f0.f64 = double(float(f0.f64 * scale));
  if (REXCVAR_GET(log_stuck_falling)) {
    std::lock_guard lock(g_runs_mutex);
    Run& run = RunFor(r31.u32);
    const auto now = Clock::now();
    // A gap means the checks in between were not falls (the game skips this part then).
    if (run.stuck && now - run.last > std::chrono::milliseconds(100)) EndRun(run);
    if (f13.f64 < f0.f64 * f0.f64) {
      if (!run.stuck) {
        run.stuck = true;
        run.since = now;
      }
      run.last = now;
    } else {
      EndRun(run);
    }
  }
}

// Before 0x82EEB3DC: the stuck timer ran past _stuckFallingTime (f31; f1 = the timer's time) and
// the game puts the player back.
void StuckFallingRespawnHook(PPCRegister& f1, PPCRegister& f31) {
  REXLOG_INFO("Stuck falling: player put back after {:.2f} s (limit {:.2f} s)", f1.f64, f31.f64);
}
