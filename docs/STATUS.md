# Status

Updated at the end of every session. Only record what a build actually did.

## Current state (2026-10-21)

| Area | State |
|---|---|
| Repository | scaffolded from GiantsRecomp: build, app, portal (emulated, USB, hybrid), portal / player / settings menus, achievements, input scripts and test tools; none of its game hooks or file readers |
| Game data | disc extracted into `rom/` (5287 files), `default.xex` hash pinned |
| SDK | our fork `tsapZ/rexglue-sdk`, branch `skylanders`, shared with the other games |
| Codegen | works with 30 `config/default.toml` entries: function starts the analyzer put one instruction late |
| Boot | ✅ the Activision and Toys for Bob logos, the title screen ("Pulsa START", version 1.0.0), the licence agreement (48 pages, accepted with **Y**), the save slots, the difficulty, a new game's opening cutscene (Y skips) |
| In-game | ✅ the first level with a figure on the emulated portal: Spyro (a Spyro's Adventure figure) in the jungle with the HUD (portrait, health bar), checked by script |
| Languages | this disc has **Spanish and Italian only**; German and English are on the owner's PS3 disc: docs/game/imaginators.md |

## 2026-10-21: session log

- **Own repository**, like the other games: each playable on its own, later through the hub
  (Skylanders: Core of Light).
- **Scaffold:** GiantsRecomp's tracked files, renamed; its Giants-specific parts left out.
- **First boot:** "Call to invalid or unregistered function at guest address 0x82562CB8": a three-instruction setter (clrldi r11,r4,32; std r11,8(r3); blr) whose second instruction the analyzer had taken as the start. The same shape searched in the whole code (a dump of guest memory with the input script's `save`) gave 30 such starts, all registered; then the game boots to the licence agreement after the title.
- **Languages:** the target disc is Spanish / Italian; the owner's PS3 disc of the game (not encrypted, the same archive format) has all ten languages with a German twin for every Italian file. What a German version needs: docs/game/imaginators.md.
- **The licence agreement doesn't take the script's input:** the game runs at 30 FPS there, but neither A (Aceptar) nor the next-page button react to the input script, also held for 600 ms and with key events (the script device now sends them like the SDL driver; the SDK answers XInputGetKeystroke for any user with player 1). Next: try it with a real controller, then find what the screen polls.
- **Solved the same morning: the licence is accepted with Y, not A.** The page's hints are "(Y) Aceptar" and "(RB) Página siguiente" (seen enlarged in a screenshot). With `tap y` the game goes on to "Elige ranura de guardado" (four empty slots, "Nueva partida"), "Elige tu dificultad" and the opening cutscene of a new game, which shows "Pulsa (Y) para omitir".
- **First level:** from a fresh user data folder, Y on the licence, A on the slot and the difficulty, Y through the cutscenes and a Spyro figure on the emulated portal: the game goes through the portal's swirl into the first level, Spyro runs in the jungle with his portrait and health bar in the HUD (screenshots 150-220 s into the run). One Y too many opened the pause menu's "restart the level?", which A answered with No.
