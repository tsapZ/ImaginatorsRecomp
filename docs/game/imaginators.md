# Skylanders: Imaginators (Xbox 360)

What we know about the game and the dump this project targets, and what still has to be checked.

## The game

- The sixth Skylanders game, 2016, published by Activision, developed by Toys for Bob, with creation crystals (Imaginators made by the player) and Sensei. It reads every earlier game's figures and its own.

## Target dump

| Property | Value | Source |
|---|---|---|
| Disc content | 5287 files | `extract-xiso`, measured |
| `default.xex` size | 25,739,264 bytes, in the disc root | measured |
| `default.xex` SHA-256 | `4298c4f6fb1684bed34dd8d2292b508782b37e3709ba1ed58b08ec334a9f7029` | measured, in `default.xex.sha256` |

Top level of the disc: `$SystemUpdate`, `MemoryPoolConfigs.igx`, `VramBudget.igx`, `archives`, `build.xml`, `cachenonce.txt`, `default.xex`, `nxeart`, `smoke_config.xml`, `version.xml`.

## Differences that matter

- **Scaffolded from GiantsRecomp** (app, portal, menus, tooling); none of its game hooks or file
  readers, which are for Giants' executable and its archives (IGA version 8).

## Languages: German and English from the PS3 disc

The target disc is a European pressing with **Spanish and Italian only** (EU Xbox 360 discs of
this game come in language pairs). The owner's PS3 disc of the same game has all ten languages.
Measured 2026-10-21 on both dumps:

| | Xbox 360 (target) | PS3 |
|---|---|---|
| Image | XISO, 5,287 files | UDF 2.50 ("PS3VOLUME"), 8,330 files, **not encrypted** |
| Layout | `archives/*.pak`, `build.xml`, `default.xex` | `PS3_GAME/USRDIR/archives/*.pak`, `build.xml`, `eboot.bin` |
| Archives | IGA version 11, big-endian | the same |
| Language files | 398 each for `es`, `it` | 398 each for all ten (`en`, `de`, `fr`, ...) |
| Voices | FMOD FSB5, codec XMA | FMOD FSB5, codec MPEG |

- 5,131 paths are on both discs, only 44 with the same size: the platform data differs.
- Every Italian file of the 360 disc has a German twin of the same name on the PS3 disc
  (`*_genbank_de.pak`, `*_bank_Lv_de.pak`, `Streamed_VO_*_de.pak`). All 398 are voice banks.
- The game's text is not in these per-language files; it is probably in the shared archives with
  per-language entries (to check with an IGA v11 reader).

**Done for the text (2026-10-21):** `tools/german_text.py` appends the PS3's German .lng files
to the 360 archives and points the Spanish entries at them (IGZ platform word 6 -> 5, nothing
else changed; the way back is saved per archive). 1304 of the 1306 Spanish text files in 414
archives get their German twin; the two left are the Xbox 360's own platform strings
(`strings/xenonPlatformStrings_es.lng` in permanent_xenon.pak: "Pulsa START", "Nueva partida"),
whose PS3 counterpart has other keys. Checked by script: the save slots, the difficulty, the
cutscene hint ("Drücke Y zum Überspringen"), the portal prompt ("Bitte platziere deinen
Skylander ...") and the figure intro come up in German. The licence's text is Spanish still
(not in these files). The voices stay Spanish.

What it means:

- **For this port:** German can come from the owner's PS3 files at run time (an overlay folder the
  game's file reads look into first), without changing the 360 dump. The voices need either the
  game's FMOD to play the PS3's MPEG banks (to test: FMOD has a software MPEG decoder on most
  platforms) or a re-encode to PCM or XMA; the text needs the German entries of the shared
  archives and the game to accept German (its language list in the XEX or `build.xml`).
- **Voice codecs, measured (2026-10-21):** every FSB5 bank header in the 360's archives says
  XMA (67,068 headers over all 5,128 archives, codec 10), so this game's FMOD build is never seen
  playing anything else. The SDK's FFmpeg (thirdparty/ffmpeg-overlay/codec_list.c) has the MP3
  decoder, so the recomp could turn a PS3 MPEG bank into a PCM16 bank (FSB5 codec 2) when the game
  reads it. Whether the 360 FMOD plays PCM16 banks is the first thing to test (one bank swapped by
  hand), before building the overlay.
- **For a German 360 ISO:** additionally XMA banks (the console has no MPEG playback in this
  game, unless its FMOD build has the codec) and the language unlocked in the executable; a
  rebuilt image then needs the 360's layout (extract-xiso can build one).
