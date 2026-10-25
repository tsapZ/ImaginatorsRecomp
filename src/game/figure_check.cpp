// Which Imaginators toy files the game accepts, checked by the game's own code (Core of Light #34:
// its import takes only files the game will really read, so nobody has to clean up a collection).
//
// This game's own toys are signed by their maker: the toy parser (sub_831F27C0) treats a toy as
// signed when its id (bytes 16-18) is 600-698 or block 1's word at 28 has a top nibble of 5 or
// more; it then puts blocks 2, 4, 34 and 62 (the 64-byte Ed25519 signature R||S) and blocks 0 and 1
// (the 32 bytes signed: the UID, the toy id, ...) one after the other, 96 bytes, gets the public key
// for the toy's key byte (byte 63) from sub_831FDA90 (a table of the game's keys at 0x82203CC0, low
// nibble 1, high nibble 5-15), and checks with sub_831FD948 (TweetNaCl's crypto_sign_open: 0 =
// valid). A toy that fails is "damaged" in the game (docs/portal.md). Here the same two functions
// run on each file, before anything of the game has run: no key in this repository or the
// launcher, nothing made or changed, only the verdict the game itself would reach. Found
// 2026-10-25 from TweetNaCl's pack25519 (sub_831FBFB0) up to the parser; 472 real dumps passed, a
// flipped byte in the UID, in R or in S, or another key byte failed.
//
// The list file: one path per line (UTF-8). The result, <list>.result: one line per file,
// "ok\t<path>", "unsigned\t<path>" (not one of this game's signed toys: an earlier game's figure,
// a magic item; the launcher decides) or "bad\t<path>\t<why>".

#include "game/figure_check.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/ppc/func.h>
#include <rex/system/xmemory.h>

REXCVAR_DEFINE_STRING(check_figures, "", "Launcher",
                      "For the launcher: checks the toy files listed in this text file (one path per "
                      "line) as the game does and writes <file>.result, then quits");

REX_EXTERN(sub_831FDA90);  // the game's public key for a toy's key byte, or 0
REX_EXTERN(sub_831FD948);  // Ed25519 check of signature + message (TweetNaCl crypto_sign_open): 0 = valid

namespace imaginatorsrecomp {
namespace {

std::filesystem::path U8Path(const std::string& s) { return std::filesystem::path(std::u8string(s.begin(), s.end())); }

}  // namespace

void CheckFiguresIfRequested(rex::memory::Memory* memory) {
  const std::string list = REXCVAR_GET(check_figures);
  if (list.empty()) return;
  uint8_t* base = memory->virtual_membase();
  // A stack and buffers of their own in guest memory: the signed bytes, the key, the output.
  constexpr uint32_t kStackSize = 256 * 1024;
  const uint32_t stack = memory->SystemHeapAlloc(kStackSize, 0x1000);
  const uint32_t buffer = memory->SystemHeapAlloc(0x1000, 0x100);
  std::string out;
  int ok_count = 0, bad_count = 0;
  std::ifstream in(U8Path(list), std::ios::binary);
  std::string line;
  while (stack && buffer && std::getline(in, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    if (line.empty()) continue;
    std::ifstream f(U8Path(line), std::ios::binary);
    std::vector<uint8_t> toy((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (toy.size() != 1024) {
      out += "bad\t" + line + "\tnot 1024 bytes\n";
      ++bad_count;
      continue;
    }
    const uint32_t id = toy[16] | toy[17] << 8 | toy[18] << 16;
    const uint32_t word = toy[28] | toy[29] << 8;
    if (!(id >= 600 && id <= 698) && (word & 0xF000) < 0x5000) {
      out += "unsigned\t" + line + "\n";
      continue;
    }
    uint8_t* signed_bytes = base + buffer;
    std::memcpy(signed_bytes, toy.data() + 32, 16);       // block 2
    std::memcpy(signed_bytes + 16, toy.data() + 64, 16);  // block 4
    std::memcpy(signed_bytes + 32, toy.data() + 544, 16);  // block 34
    std::memcpy(signed_bytes + 48, toy.data() + 992, 16);  // block 62
    std::memcpy(signed_bytes + 64, toy.data(), 32);        // blocks 0 and 1
    PPCContext ctx{};
    ctx.r1.u64 = stack + kStackSize - 0x100;
    ctx.r3.u64 = toy[63];
    sub_831FDA90(ctx, base);
    const uint32_t key = ctx.r3.u32;
    if (!key) {
      out += "bad\t" + line + "\tunknown key\n";
      ++bad_count;
      continue;
    }
    ctx = PPCContext{};
    ctx.r1.u64 = stack + kStackSize - 0x100;
    ctx.r3.u64 = buffer + 0x200;  // the output (crypto_sign_open's m)
    ctx.r4.u64 = buffer;
    ctx.r5.u64 = 96;
    ctx.r6.u64 = key;
    sub_831FD948(ctx, base);
    if (ctx.r3.u32 == 0) {
      out += "ok\t" + line + "\n";
      ++ok_count;
    } else {
      out += "bad\t" + line + "\tsignature\n";
      ++bad_count;
    }
  }
  std::ofstream result(U8Path(list + ".result"), std::ios::binary | std::ios::trunc);
  result << out;
  result.close();
  // The flag's name in a string of the exe: the launcher looks for it before asking (an older
  // build would start the whole game instead); the setting's own name isn't stored as text.
  REXLOG_INFO("check_figures: {} accepted, {} not ({})", ok_count, bad_count, list);
  rex::FlushLogging();
  // Nothing of the game has run: no threads to stop. Without static destructors, as the settings
  // schema's quit does.
  std::_Exit(result ? 0 : 1);
}

}  // namespace imaginatorsrecomp
