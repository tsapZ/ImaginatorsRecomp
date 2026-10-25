// Opens the portal menu when the game asks a player for a Skylander.
//
// As in Swap Force and SuperChargers (SwapForceRecomp / SuperChargersRecomp
// src/game/magic_moment_watch.cpp), the game runs everything around "place a Skylander on the
// Portal of Power" through the CMagicMoment singleton. Found 2026-10-21: the class's field names
// (pointer array at 0x821724B0, 162 fields) with their u16 offsets (0x821729C0, found as the table
// that has the two offsets the snapshots gave), the enum names (eMMS_ states from 0x82157FF8) from
// the executable, and the instance pointer from guest memory snapshots of .data and the physical
// heap at the prompt ("Coloca tu Skylander en el Portal de Poder") and in play: the only global
// whose object had 5 at +0x40 and 0 at +0x1D0 at the prompt, 0 and -1 in play.
//   0x837F649C  global pointer to the instance (seen at 0xB5CA53B0)
//   +0x40       _magicMomentState (EMagicMomentState, Swap Force's numbering): 0 = Inactive,
//               3 = TransitionIn (the camera zooms out to the portal), 5 = Idle (the prompt is up,
//               waiting for a figure), 7 IdleOutro, 8 Load, 10 PlayIntro, 11 TransitionOut,
//               16 = Error (a "too many ... on the portal" screen), 17-20 the online states,
//               21 TakeHudPortrait
//   +0x158      _reason (EMagicMomentReason)
//   +0x1CC      _errorType (EMagicMomentErrorType): 0 None, 1 TooManyCharacterToys,
//               2 TooManyCharacterToysForPlayer, ...
//   +0x1D0      _currentPlayerId: 0 = player 1, 1 = player 2, -1 = none
// The error screen opens the portal check instead, so the player can take off what is too much.
//
// Polling instead of a mid-asm hook keeps the generated code untouched and costs three guest
// reads every 30 ms. It only reads: every address is checked to be committed guest memory before
// it is touched, and implausible values are ignored, so a stale pointer can't do harm.

#include "magic_moment_watch.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <thread>

#include <rex/logging.h>
#include <rex/system/xmemory.h>

#include "guest_memory.h"
#include "portal_rex/portal_menu.h"

namespace imaginatorsrecomp {
namespace {

using Clock = std::chrono::steady_clock;

constexpr uint32_t kMagicMomentInstance = 0x837F649C;
constexpr uint32_t kStateOffset = 0x40;
constexpr uint32_t kReasonOffset = 0x158;
constexpr uint32_t kErrorTypeOffset = 0x1CC;
constexpr uint32_t kPlayerOffset = 0x1D0;
constexpr uint32_t kStateZoomOut = 3;
constexpr uint32_t kStateIdle = 5;
// A player's Skylander died: the game waits for another one in this state (SuperChargers' 14 one lower: this game has no FirstVehicleSequence, so IdleOutro is 7 here and 8 there, Error 16 here and 17 there; seen in the owner's log of 2026-10-25, 34 s in 13 until he put another Skylander on).
constexpr uint32_t kStateDead = 13;
constexpr uint32_t kStateError = 16;
constexpr uint32_t kLastState = 21;
// The portal check's reasons (Swap Force's EMagicMomentReason values, which the portal menu
// library uses).
constexpr uint32_t kCheckReasonOther = 0;
constexpr uint32_t kCheckReasonTooManySkylanders = 1;
constexpr uint32_t kErrorTooManyCharacterToys = 1;
constexpr uint32_t kErrorTooManyCharacterToysForPlayer = 2;
// The error screen must stay this long before the portal check opens, and if it was closed with
// the error still up, it comes back after a while.
constexpr auto kErrorDelay = std::chrono::milliseconds(400);
constexpr auto kErrorRepeat = std::chrono::seconds(8);
// How long the game must keep waiting before the menu opens, so it comes up with the prompt
// rather than during a one-frame state flicker.
constexpr auto kOpenDelay = std::chrono::milliseconds(100);

std::thread g_thread;
std::atomic<bool> g_stop{false};
std::atomic<bool> g_active{false};  // state != Inactive at the last poll
uint32_t g_last_state = 0;            // watcher thread only; for the debug log

struct Prompt {
  uint32_t player;
  uint32_t reason;
  bool vehicle = false;  // the game asks for a vehicle, not for a Skylander
};

// The player the game is waiting for, if its "place your Skylander" prompt is up.
std::optional<Prompt> CurrentPrompt(rex::memory::Memory* memory) {
  const auto instance = ReadGuest32(memory, kMagicMomentInstance);
  // The instance lives in physical memory (seen at 0xBF6B816C); ReadGuest32 checks that whatever
  // the pointer says is committed.
  if (!instance || !*instance) {
    g_active.store(false);
    return std::nullopt;
  }
  const auto state = ReadGuest32(memory, *instance + kStateOffset);
  g_active.store(state && *state != 0 && *state <= kLastState);
  const auto player = ReadGuest32(memory, *instance + kPlayerOffset);
  if (state && *state != g_last_state) {
    // The state sequence of one figure change, e.g. to time the portal menu against it or the
    // game's speed at a higher frame rate. A few lines per figure change, so at info level.
    REXLOG_INFO("Magic moment: state {} -> {} (player {})", g_last_state, *state,
                player ? int32_t(*player) + 1 : 0);
    g_last_state = *state;
    // A menu opened with the button comes up during the zoom-out, just before the prompt.
    if (*state == kStateZoomOut && player && *player < 4) skylanders::PortalScreenComing(*player);
    // A Skylander out of health: marked in the menu until it has rested (a new level).
    if (*state == kStateDead && player && *player < 4) skylanders::PlayerSkylanderTired(*player);
  }
  const auto reason = ReadGuest32(memory, *instance + kReasonOffset);
  if (!state || !player || !reason) return std::nullopt;
  // A Skylander died: the game waits for that player's next one, as at its prompt.
  if (*state == kStateDead && *player < 4) return Prompt{*player, *reason};
  if (*state != kStateIdle) return std::nullopt;
  // The vehicle request ("Bitte platziere ein Landfahrzeug auf das magische Portal"): Idle with no
  // player (_currentPlayerId -1, the owner's logs of 2026-10-25), where a Skylander prompt names one.
  if (*player == 0xFFFFFFFFu) return Prompt{0, *reason, true};
  if (*player >= 4) return std::nullopt;
  return Prompt{*player, *reason};
}

// The portal check's reason for the game's portal error screen while it is up.
std::optional<uint32_t> CurrentError(rex::memory::Memory* memory) {
  const auto instance = ReadGuest32(memory, kMagicMomentInstance);
  if (!instance || !*instance) return std::nullopt;
  const auto state = ReadGuest32(memory, *instance + kStateOffset);
  if (!state || *state != kStateError) return std::nullopt;
  const auto error = ReadGuest32(memory, *instance + kErrorTypeOffset);
  if (!error || *error > 64) return std::nullopt;
  return *error == kErrorTooManyCharacterToys || *error == kErrorTooManyCharacterToysForPlayer
             ? kCheckReasonTooManySkylanders
             : kCheckReasonOther;
}

void Watch(rex::memory::Memory* memory) {
  std::optional<uint32_t> waiting_for;  // player whose prompt is up
  Clock::time_point since{};
  bool opened = false;
  bool vehicle_request = false, vehicle_opened = false;
  Clock::time_point vehicle_since{};
  std::optional<Clock::time_point> error_since, check_shown;
  while (!g_stop.load()) {
    if (const auto error = CurrentError(memory)) {
      const auto now = Clock::now();
      if (!error_since) error_since = now;
      if (now - *error_since >= kErrorDelay && (!check_shown || now - *check_shown >= kErrorRepeat)) {
        REXLOG_INFO("Magic moment: the game shows its portal error (check {})", *error);
        skylanders::ShowPortalCheck(*error);
        check_shown = now;
      }
    } else if (error_since) {
      if (check_shown) skylanders::ClosePortalCheck();
      error_since.reset();
      check_shown.reset();
    }
    const std::optional<Prompt> prompt = CurrentPrompt(memory);
    const std::optional<uint32_t> player =
        prompt && !prompt->vehicle ? std::optional<uint32_t>(prompt->player) : std::nullopt;
    // The vehicle request: the menu opens on the vehicles and the Skylander stays on
    // (skylanders::AutoOpenPortalMenuForCompanion; opened with the button it lifted the Skylander,
    // and the vehicle went on only after the game stopped being busy, 10 s later).
    const bool vehicle = prompt && prompt->vehicle;
    if (vehicle != vehicle_request) {
      vehicle_request = vehicle;
      vehicle_since = Clock::now();
      vehicle_opened = false;
      if (!vehicle) skylanders::CompanionRequestEnded();
    }
    if (vehicle_request && !vehicle_opened && Clock::now() - vehicle_since >= kOpenDelay) {
      REXLOG_INFO("Magic moment: the game asks for a vehicle (reason {}; the menu opens on the "
                  "vehicles)", prompt->reason);
      skylanders::AutoOpenPortalMenuForCompanion(0, skylanders::portal::Element::kNone);
      vehicle_opened = true;
    }
    if (player != waiting_for) {
      if (waiting_for && opened) skylanders::CloseAutoOpenedPortalMenu();
      waiting_for = player;
      since = Clock::now();
      opened = false;
    }
    if (waiting_for && !opened && Clock::now() - since >= kOpenDelay) {
      REXLOG_INFO("Magic moment: the game asks player {} for a Skylander (reason {})",
                  *waiting_for + 1, prompt->reason);
      skylanders::AutoOpenPortalMenu(*waiting_for);
      opened = true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
  }
}

}  // namespace

void StartMagicMomentWatch(rex::memory::Memory* memory) {
  if (!memory || g_thread.joinable()) return;
  g_stop.store(false);
  g_thread = std::thread(Watch, memory);
}

bool MagicMomentActive() { return g_active.load(); }

void StopMagicMomentWatch() {
  g_stop.store(true);
  if (g_thread.joinable()) g_thread.join();
}

}  // namespace imaginatorsrecomp
