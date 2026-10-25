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
| Portal menu | ✅ opens with Guide; shows the game's own collection icons for every toy, each figure's variant where the game has one; opens by itself when the game asks for a Skylander, the portal check on the "too many" screens; a sensei's card shows its battle class (the game's medallion and name), a vehicle's its terrain; the details view shows the hat (name and stats; the disc has only three hat pictures) and every Skylander's upgrade tree with the game's pictures and prices |
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
- **Native 3D picture, experimental** (src/hooks/scene_size_hook.cpp, `scene_size`, Graphics page,
  off by default): the 360 draws the 3D scene at 1040x620 (mainScene in standard_renderer.igz in
  LooseFiles.pak) and stretches it to 720p. SwapForceRecomp's hook, the same render code: the target
  size function (0x826F96D0) and the three EDRAM places (planning heap, tiling decision, depth base)
  found in a dump of this game's code by their instructions. With it on the scene is drawn at
  1264x704 with the SDK's 4096-tile EDRAM (D3D12). Checked by script in the first level: the target
  goes 1040x620 -> 1264x704, the scene draws right, the edges about 7 % sharper (mean gradient of
  the same frame), no crash. Loading the save three times with it (where Swap Force had crashed at
  the screen's exact size): loaded and played on each time.
- **Ultrawide** (src/hooks/ultrawide_hook.cpp, `ultrawide_fov`, on): a window wider than 16:9
  (`resolution`) shows more to the sides instead of black bars, as in SwapForceRecomp. The
  perspective builder (sub_82547C30) found by Swap Force's instructions in a dump of this game's
  code; the scene camera's call with a probe, a hook at each of the builder's callers logging its
  arguments in play: 0x82BA84D0 runs every frame with 16:9 and the camera's horizontal FOV. The hook
  widens the aspect and the FOV there, keeping the vertical FOV; the app turns the letterbox off.
  Checked by script: the first level in a 2560x1080 window: Spyro and the stones in their
  proportions, more of the jungle at both sides; the HUD stretched.
- **The portal menu's pictures the right way up and sharper** (src/game/toy_images.cpp): the
  collection icons are stored upside down, which the menu showed (the owner saw it, 2026-10-21; the
  code's comment had called them upright). They are flipped when read. The disc has only 88x88 of
  each figure (the HUD portraits, ui/HealthHud_2015/Portraits, are the same size), shown about twice
  as large, so the first time the menu asks for an icon it is enlarged 2x with a Lanczos-3 filter
  (on premultiplied alpha) and kept. Checked by script: Double Trouble and the magic items upright
  and crisper.
- **60 FPS and more** (src/hooks/frame_rate_hook.cpp, `present_interval`, F4 > Bildrate, off by
  default): the game swaps with presentation interval 2 like Swap Force (30.0 FPS with
  frame_rate_limit 60, 72.0 with 144); Swap Force's hook on the D3D library's swap (0x8287A750,
  found by its instructions) sets 1. With it, 144.0 FPS with frame_rate_limit 144 in the first
  level, and the game keeps its speed: the magic moment's states around a figure's arrival last as
  long as at 30 FPS (the figure's intro 5.36 against 5.42 s). The magic moment watch logs its state
  changes at info level now (a few lines per figure change), which is how the speed was timed.
- **The menus in the game's own fonts** (src/game/game_fonts.cpp from SuperChargersRecomp,
  skylanders-portal 465b4c1, SwapForceRecomp #54): fonts/ProximaNova-Title.igz (permanent.pak, 93
  glyphs, the rounded title letters of the game's own prompts; a 1024x1024 page with a second mip
  level) and fonts/ProximaNova-Body.igz (essentialui.pak, 172 glyphs with ä ö ü ß). The portal
  library draws the menus' titles and text with them, Segoe UI filling in missing glyphs. Checked
  by script: the portal menu's title in the same letters as the game's "BITTE PLATZIERE DEINEN"
  behind it; names, tabs and hints in ProximaNova.
- **The game's element badges and coin in the portal menu** (src/game/toy_images.cpp): the
  menu drew its own; the HUD's element badges (skystonesSmashPortraits.pak, ui/HealthHud_2016/
  Portraits/Elements, 128x128) and the ability shop's coin (JuiceDomain_ActionPacks.pak) are
  handed to it now. This disc has nine of the ten badges: Air's is in none of its archives (the
  other element sets lack it or Light, Dark or Water too), so the menu draws its own Air
  medallion.
- **Sharper icons where the disc has them:** the collection tracker (CollectibleTrackerIcons.pak)
  has 128x128 copies of 49 characters' icons (SuperChargers' and this game's senseis); those are
  taken instead of the 88x88 ones, and every icon is enlarged 2x.
- **The later games' characters in their element groups:** the portal library knew the elements
  of the first three games only, so this game's senseis, villains and creation crystals (and
  Trap Team's and SuperChargers' characters) were under none. Its table now has them, read from
  the games' own files (here: the toy data's +0x0C); the crystals by toy id. Checked by script:
  the SuperChargers tab shows Splat, Soda Skimmer, Splatter Splasher under Magic.
- **The HUD at the window's edges with ultrawide** (`ultrawide_hud`, on; F4 > Grafik > "HUD bei
  Ultrawide"): the owner found the stretched HUD squashed. The UI (Juice) lays itself out on its
  contexts' expected screen width, as in Swap Force (whose hook was found first); the hook sets it
  to the window's width at the UI's height where the sizes are read for drawing, so the HUD and the
  menus keep their proportions at the window's edges. Checked by script in a 2560x1080 window:
  the pause menu (its progress screen), nothing left uncovered.
- **Movies on ultrawide** (2026-10-21): the logo movies and the opening movie are 16:9 Bink
  movies; while one plays the presenter shows it with black bars at the sides instead of
  stretched (present_use_actual_aspect off and present_letterbox on for that time, not saved).
  Start: BinkOpen's header hook; end: a hook at the start of BinkClose (sub_832A04B8, called only
  from the game's movie close). Checked by script in a 2560x1080 window with skip_intro off: the
  logos 16:9 in the middle, the screen after them fills the window again.

## 2026-10-22: session log (night)

- **Senseis' battle classes and vehicles' terrain** (79c87b6): the game's class table LooseData/CYOSClassDataList.igz in JuiceDomain_FrontEnd.pak (ten classes, 30 senseis; Kaos has a class of his own without a symbol), the medallions cut out of the sensei shrine pictures in CRM.pak, the German class names from the game's text (Ritter, Bogenkämpfer, Zauberer, ...); the terrain at CVehicleToyData +0x34 with the blueprint screen's colour symbols from E3PressStart.pak. Checked: Buckshot "Bogenkämpfer", Mysticat a sorcerer, Pain-Yatta a smasher, Soda Skimmer the sea symbol.
- **Hats in the details view** (src/game/hats.cpp, the same code as SuperChargers' with this disc's offsets: number at +0x9C, attributes +0x98, icon +0x10): 266 hats, 32 attribute names; only three hats have a picture on this disc (the game shows hats as 3D models), the others get a plain hat symbol.
- **Upgrades in the details view** (src/game/toy_upgrades.cpp, shared with SuperChargers; this disc's character data has the same layout, King Pen and Spyro checked). Checked on a copy of a Chop Chop: the whole tree with the pictures, German texts and prices. A sensei's Sky-Chi is shown as the Soul Gem path's second upgrade.
- **Portal library** (83cc22e, 141d32e): German card subtitles in the singular ("Sensei", "Schurken-Sensei", "Kreationskristall"); the hat symbol.
- **New issues:** #3 bloom and motion blur options, #4 a creation crystal's class, #5 the game's controller picture (on the disc: Controller_SchemeXenon.pak).

## 2026-10-22: later the same night

- The settings example and the installer's template set skip_intro twice (an invalid file); fixed, and the example says 60 FPS.
- **Settings file read even with a repeated key** (SDK fork 8761248): a key set twice in a table makes the whole TOML file invalid, and the game then ignored every setting in it without a visible error (the error went to a log that didn't exist yet). The SDK now reads such a file with the repeats left out and logs which key was repeated.
- **`--settings_schema=<file>`** (skylanders-portal 4521ced): the game writes its F4 menu's pages with their values and quits before a window opens; the Core of Light launcher shows them as the game's settings and writes into this game's settings file.
- **Figure pictures for the launcher** (skylanders-portal e17786b): while the game runs, every figure picture it reads from the disc goes into `Documents\Skylanders\Pictures\<game>` once, for the launcher's figure library; later the same night (skylanders-portal 6c3c6a3) also the hats it knows (hats.txt, their pictures) and the upgrade trees of the figures in the figure folder; and
(skylanders-portal f571d26) the game's achievement list with the title's own pictures, for the
launcher's achievements screen.
- **Bloom strength (F4 > Grafik > Leuchten (Bloom)), as in Swap Force and SuperChargers:** the
  same environment class (`_bloomScale` at +0x3A4), the hooks at the end of its zone blend
  (`sub_82A92408`, 0x82A94FC0) and after the per-zone blend (`sub_82A94FC8`, 0x82A95A18;
  `src/hooks/post_effects_hook.cpp`). Checked in the first level: the hook runs every frame, and
  a low cutoff set through it made the scene glow; with the level's cutoff of 5 the setting shows
  most in bright scenes.
- **Motion blur (F4 > Grafik > Bewegungsunschärfe), as in Swap Force and SuperChargers, off by
  default:** the motion blur material's `ig_motion_blur_exposure_params` (+96), scaled at the start
  of its parameter setter (`sub_82899870`, 0x82899870), the same function as SuperChargers'.
  Checked in the first level, Spyro running with the camera turning: the picture's mean gradient
  3.53 at 0, 3.29 at the game's 1, 3.14 at 2 (the leaves visibly softer), and equal while standing.
- **Skylander-Vorstellung: Kurz (F4 > Spiel), as in Swap Force (SwapForceRecomp #60):** the
  figure's intro after the placement ends after 1.5 s. The intro's time check (`sub_829FF1F0`,
  0x829FF268) alone doesn't end it: the magic moment's update in state 2 (`sub_82A4BC28`) waits
  for the figure's intro animation (`sub_82D90538`) and then sets the state (+68) to 4; a write
  watch on the state found that, as in SuperChargers. The hooks shorten the time and, once the
  intro ran 1.5 s, answer "over" for the animation (0x82A4C114, 0x82A4BE58), so the game's own
  ending runs. Checked in the first level with Spyro placed, a screenshot a second: the name
  shows for one picture instead of four and the game starts about 2 s earlier.
- **Nothing skipped by default** (the owner's choice, 2026-10-22): `skip_intro` is off, the logo
  movies play as on the console (the option skips them); the settings templates say so too. The
  16x texture filter now comes from skylanders-portal (`ApplyGraphicsDefaults`), so the Core of
  Light launcher shows 16x as the game uses it.

## 2026-10-23: session log (night)

- **Title music from the disc** for the Core of Light launcher (skylanders-portal 6e5c284,
  rexglue-sdk 63e87be): `--export_title_music=<file.wav>` decodes the track the title screen
  streams (found with a run logging the game's file reads) from XMA2 with the SDK's FFmpeg and
  quits before a window opens, in under a second; each normal start leaves `title_music.wav` in
  the game's picture folder once. Checked: mu_main_menu_2016_apm_heroes_all of archives/INTER_Streamed_bank.pak (43.9 s); identical to FFmpeg 8.1's own decode of the
  track (-63 dB).
- **Leaving the game with the pad:** F4 > Spiel ends with "Spiel beenden" and "Spiel neu
  starten" (both ask, "Nein" first; restart not offered when the launcher started the game; the
  new instance waits for the old one), the player menu is a row on the Portal page, the settings
  menu's buttons have no "Aus" any more (it locked a pad out), "Alle Einstellungen" says it is for
  keyboard and mouse (skylanders-portal 5fff7aa).
- **HD texture packs:** rexglue-sdk 37d3e77 dumps the game's BC1-BC3 textures
  (`texture_dump_dir`) and loads `<hash>.dds` replacements of any size (`texture_replace_dir`);
  a pack in `Documents\Skylanders\Textures\Imaginators` is used by itself (skylanders-portal 94175f0).
  Checked in Swap Force's hub (see SwapForceRecomp docs/texture-packs.md, #62).
- **F4 > Grafik:** "Schärfe-Stärke" (CAS's additional sharpness) and "Farbverläufe glätten
  (Dithering)", dithering on by default.
- **Installer:** its own AppId (all six shared one); `/NOROM=1` installs without the disc for the
  launcher (the settings file is still written); reasons a silent install stops go into its log.
  Silent install from the owner's ISO, start (German title screen), silent uninstall: checked.
- No "Figuren späterer Spiele" row (there is no later game).
- The installer from the Es/It ISO gives a Spanish game ("Pulsa START"); German needs the
  German-patched ISO (tools/german_text.py), as before.
- **German names of the magic items and traps** in the portal menu (it showed the catalog's
  English ones): from the toys' own text files next to their toy data
  (ToyData/<Name>_ToyData_de.lng, else _es; "Kampfhammer", "Schicksalshand", "Sonnenkratzer
  (LightCore)"). The German disc keeps the Skylanders' and vehicles' English names, so those stay.
  Checked by script in F6 > Magische Objekte (91 names).
- **Long falls fixed (`fix_stuck_falling`, on), as in Swap Force and SuperChargers:** the player's
  jump component (sub_82EEB308) counts a fall as stuck when the player moved less than 20 units
  (or the component's own override) since the last frame, and after 3 s of that puts the player
  back; 20 per frame was tuned for 30 FPS. Measured in the first level, the player launched with
  the jump component's bounce pad fields at about 155 FPS: without the fix a launch at 5000 and one
  at 8000 each ended with the player put back after 3 s; with it never (normal jumps 0.06-0.35 s
  counted as stuck instead of 0.43-0.45 s).

## 2026-10-23: session log (day)

- **This game's toys only with a file of them** (skylanders-portal 3844b6f): senseis, creation
  crystals and Mystery Chests carry a 32-byte Ed25519 signature in sector 0 (blocks 2 and 4) that
  the game checks (TweetNaCl in the game, see docs/portal.md); a blank one the menu made was
  "damaged". The menu now lists them only when there is a file (e.g. from the real portal in
  hybrid mode). Checked: with one Air Strike file the Imaginators tab shows only Air Strike; a real
  Bronze Mystery Chest opens and gives its parts (Ohrenwärmer, Doomlander-Barbaren-Beine); blank
  Air Strike, blank crystal, blank chest and a real Air Strike with zeroed blocks 2 and 4 are all
  "damaged".
- **Swapper builder** as picture grids, LB / RB on a recent swapper (skylanders-portal b2e7d07).

## 2026-10-24: 0.1.0-beta.7 (a colleague's evening on SteamOS)

- **Ultrawide by itself in fullscreen** (skylanders-portal bd1b318): the ultrawide camera and the
  presenter take the aspect from `resolution`, so "Fenstergröße: Automatisch" stayed 16:9 on a
  21:9 screen. `ResolutionFromScreenIfAutomatic`, first thing in OnPreSetup, sets it for the start
  to the primary screen's mode (nothing written; not for the launcher's schema).
- **Window sizes** add 2560x1080 and 3440x1440 (21:9), 3840x1600 (24:10) and 5120x1440 (32:9).
- **"Tastatur spielt als"** is greyed and can't be changed while "Tastatur und Maus" is off
  (skylanders-portal 10a00cd, `enabled_by`, also in the schema for Core of Light).

## 2026-10-24: 0.1.0-beta.8

- **Later games' versions in the portal menu's version choice** (skylanders-portal 202b4df): Y on a
  Skylander lists its versions from the later games too (Spyro in Spyro's Adventure: Series 2 and
  3, LightCore, Eon's Elite; the game plays them by their id as its own; the owner's wish). The
  tabs stay this game's own; "Figuren späterer Spiele" now only adds the later games' tabs.
- Ultrawide checked in a 2560x1080 window from the title to the first menus in Spyro's Adventure,
  Giants, Swap Force, SuperChargers and Imaginators (menus centred, nothing stretched, movies with
  bars); Trap Team's window couldn't be captured here (black at 16:9 too), so not by picture.

## 2026-10-24: 0.1.0-beta.9

- **One data folder for every game** (the owner's call; skylanders-portal data_folder.h): the saves,
  achievements and shader cache are in `Documents\Skylanders\Imaginators` now, next to Figures and
  Pictures, or in the folder picked in Core of Light (`%APPDATA%\Skylanders\data-folder.txt`). The
  first start copies the old `Documents\imaginatorsrecomp` there (complete or not at all) and leaves a
  note in it; Core of Light asks before the old folder goes. `--user_data_root` works as before.
- **Keyboard keys changeable in F4** (SwapForceRecomp #63): a "Tastatur" page after "Steuerung", a
  row per controller button; A and then the key. Y also saves with a German keyboard now (the
  menu reads keys by their US place, where that key is Z).
- The portal menu's Portal of Power: a band of light round its side, ridges on the rim, rings of
  light running out over the floor (SwapForceRecomp #57).
- Checked: every game boots to its first menus on fresh scratch data with the new data folder
  code (with --user_data_root, so the copy itself was checked in the launcher on a scratch
  Documents).

## 2026-10-25: 0.2.0-beta.1

- **Vehicles next to the Skylander** (SuperChargersRecomp #6; skylanders-portal 2278b99): each
  player's vehicle goes in a slot of its own and no longer lifts the player's Skylander.
- **The old sidekicks are Minis here** (Core of Light #32): they go on as a player's Skylander.
- **Bowser, Donkey Kong and their vehicles hidden** (#7): the 360 disc doesn't have them;
  `portal_menu_nintendo = true` lists them.
- **One "Bildrate" row as in the other games**: the "60 FPS und mehr" switch is gone.
- **Mouse wheel in the menus** (SwapForceRecomp #65; skylanders-portal ccc7009): F4's rows, the
  portal menu's cards and its variant list scroll a row per notch; the chosen row stays, and the view
  follows it again once the keys or the pad move it.
- **A chosen 4x texture filter stays 4x** (Core of Light #21, a colleague's report and fix;
  skylanders-portal 23be5f2): 4x is the SDK's own default, so it counted as unset and became 16x.
