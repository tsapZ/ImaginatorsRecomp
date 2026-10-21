# Status

Updated at the end of every session. Only record what a build actually did.

## Current state (2026-10-21)

| Area | State |
|---|---|
| Repository | scaffolded from GiantsRecomp: build, app, portal (emulated, USB, hybrid), portal / player / settings menus, achievements, input scripts and test tools; none of its game hooks or file readers |
| Game data | disc extracted into `rom/` (5287 files), `default.xex` hash pinned |
| SDK | our fork `tsapZ/rexglue-sdk`, branch `skylanders`, shared with the other games |
| Codegen | works with 30 `config/default.toml` entries (function starts the analyzer put one instruction late) and one hook (the movie skip) |
| Boot | ✅ the Activision and Toys for Bob logos (skipped by default, `skip_intro`), the title screen ("Pulsa START", version 1.0.0), the licence agreement (48 pages in Spanish, 52 in German, accepted with **Y**), the save slots, the difficulty, a new game's opening cutscene (Y skips; `skip_opening_movie` skips it, off by default) |
| In-game | ✅ the first level with a figure on the emulated portal: Spyro (a Spyro's Adventure figure) in the jungle with the HUD (portrait, health bar), checked by script |
| Portal menu | ✅ opens with Guide; shows the game's own collection icons for every toy, each figure's variant where the game has one; opens by itself when the game asks for a Skylander, the portal check on the "too many" screens |
| Languages | this disc has **Spanish and Italian only**; German text (menus, texts, subtitles, the title screen, the licence) from the owner's PS3 disc with tools/german_text.py, German voices experimental (`--voices`); German and English are on the owner's PS3 disc: docs/game/imaginators.md |

## 2026-10-21: session log

- **Own repository**, like the other games: each playable on its own, later through the hub
  (Skylanders: Core of Light).
- **Scaffold:** GiantsRecomp's tracked files, renamed; its Giants-specific parts left out.
- **First boot:** "Call to invalid or unregistered function at guest address 0x82562CB8": a three-instruction setter (clrldi r11,r4,32; std r11,8(r3); blr) whose second instruction the analyzer had taken as the start. The same shape searched in the whole code (a dump of guest memory with the input script's `save`) gave 30 such starts, all registered; then the game boots to the licence agreement after the title.
- **Languages:** the target disc is Spanish / Italian; the owner's PS3 disc of the game (not encrypted, the same archive format) has all ten languages with a German twin for every Italian file. What a German version needs: docs/game/imaginators.md.
- **The licence agreement doesn't take the script's input:** the game runs at 30 FPS there, but neither A (Aceptar) nor the next-page button react to the input script, also held for 600 ms and with key events (the script device now sends them like the SDL driver; the SDK answers XInputGetKeystroke for any user with player 1). Next: try it with a real controller, then find what the screen polls.
- **Solved the same morning: the licence is accepted with Y, not A.** The page's hints are "(Y) Aceptar" and "(RB) Página siguiente" (seen enlarged in a screenshot). With `tap y` the game goes on to "Elige ranura de guardado" (four empty slots, "Nueva partida"), "Elige tu dificultad" and the opening cutscene of a new game, which shows "Pulsa (Y) para omitir".
- **First level:** from a fresh user data folder, Y on the licence, A on the slot and the difficulty, Y through the cutscenes and a Spyro figure on the emulated portal: the game goes through the portal's swirl into the first level, Spyro runs in the jungle with his portrait and health bar in the HUD (screenshots 150-220 s into the run). One Y too many opened the pause menu's "restart the level?", which A answered with No.

- **The game's pictures in the portal menu** (src/game/toy_images.cpp, taken over from
  SuperChargersRecomp with its Alchemy readers): this disc has the same toy data layout,
  permanent.pak with ToyData/<Name>_ToyData.igz (IGZ version 9) and ToyCollectionMaterials.pak with
  the collection icons. 281 toys, 308 variants, 565 icons in 435 ms. Checked by script in the first
  level: the pages of every game show their pictures, the Imaginators page its creation crystals.
- **The portal menu opens by itself** (src/game/magic_moment_watch.cpp, SuperChargers' watch with
  this game's values): Ghidra project imported this morning; CMagicMoment's field names and u16
  offsets from the executable (state +0x40, reason +0x158, error type +0x1CC, player +0x1D0), the
  states numbered as in Swap Force (Idle 5, Error 16), the instance pointer 0x837F649C from
  snapshots of .data and the physical heap at "Coloca tu Skylander" and in play. Checked by script:
  the menu came up by itself at the prompt, A and A put Double Trouble on, and he played in the
  first level with his HUD portrait.
- **German text from the PS3 disc** (tools/german_text.py, stdlib Python + 7-Zip for the ISO):
  the 360 archives' Spanish .lng files get the PS3 archives' German twins appended and the entries
  pointed at them (1304 of 1306, in 13 s). A test with three archives first, then all: the menus,
  the cutscene hints, the portal prompt and the figure intro are German; "Pulsa START" / "Nueva
  partida" (the 360's own platform strings) and the licence text stay Spanish. Applied to this
  machine's rom/ (undo: `--restore`).
- **German voices, experimental** (`tools/german_text.py --voices`): the PS3's German voice
  banks are MP3 (FSB5 codec 11); the executable has FMOD's MP3 codec ("FMOD MPEG Codec",
  fmod_codec_mpeg.cpp), so the banks go in as they are, matched by name up to ".wav" (the encoding
  settings after it differ, 32000 / 48000 Hz). 17917 voices in 396 of 398 archives. A run through
  the intro, the cutscenes and the first level with them works (no error, same timing); one of
  them decoded with ffmpeg is a valid 22 s voice. Whether the game plays them is for the owner to
  hear. Applied to this machine's rom/ (undo: `--restore`, then text only without `--voices`).
- **Logo movies skipped** (src/hooks/intro_skip_hook.cpp, SuperChargers' hook): BinkOpen reads
  the 44-byte movie header into r1+112 in sub_8329E078 (`li r6,44` and the "KB2f" compare after
  it); the hook at 0x8329E248 sets the frame count of the two logo movies to 1 (`skip_intro`, on)
  and, with `skip_opening_movie` (off), of FMV01_Cinematic.bk2, the new game's opening movie. The
  movies are recognised by their header's size and frame count, unique among the disc's 133.
  Checked by script: the title screen 8 s after the start, all three skipped and the game goes on
  to the portal prompt.
- **The last Spanish texts German** (tools/german_text.py): the licence agreement is on the 360
  disc in German already (softwareLicense.pak, licenseAgreement_de.igz), so its Spanish entry now
  points there; the 360's own platform strings (xenonPlatformStrings_es.lng: "Pulsa START",
  "Nueva partida", the Xbox Live messages) get a German copy, built by key from the PS3 disc's PS3
  and PC platform strings where their Spanish matches the 360's (273 of 282) and 9 texts written
  for the tool. The .lng writer reproduces the game's files byte for byte; `--restore` now undoes
  several steps on one archive. Checked by script: "Drücke START", the licence in German,
  "Neues Spiel" and "Lizenz" on the save slots. Applied to this machine's rom/.
