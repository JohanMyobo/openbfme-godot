# BFME1 support

BFME1 is an experimental, separate game mode. It uses a BFME1 installation for
both menus and gameplay, without mixing in BFME2 or RotWK data.

The main menu loads BFME1's mapped textures from `art/textures`, including
JPEG colour with a companion PNG alpha mask. The backdrop and native
`RenderImage` logo use the same texture loader as the menu gadgets.
`Video FrontEnd` in BFME1's `Video.ini` identifies `big_movie_with_alpha.vp6`
as the movie for front-end menus. It loops over `ShellMapLowLOD`, preserves
its alpha, and stops when the shell backdrop is hidden for gameplay.
The movie fills the aspect-fitted backdrop beneath the APT menu and logo;
this placement and immediate start/stop are presentation inferences, not a
verified port of retail's display transitions.

Submenus retain their authored 3D front-end background. When the BFME1 main
menu becomes active again, it hides that background through its existing
APT instant-hide function so it no longer covers the continuing movie. This
main-menu activation rule is also a presentation inference; BFME2 and RotWK
keep their existing background behaviour.

`godot/tests/bfme1_menu_backdrop_test.gd` requires `BFME1_INSTALL` and a GPU.
It compares independently decoded JPEG/PNG and VP6 assets against the menu
at four resolutions, checks repeated Skirmish/Options returns, looping and
gameplay/menu transitions, and writes screenshots under `BFME1_MENU_TEST_DIR`
when that variable is set.

From the checkout root on **Linux or macOS (POSIX shell)**, with a matching
Godot 4.7 editor binary on `PATH`, the GDExtension already built into
`godot/bin/`, and BFME1 data in `games/BFME1`, launch:

```sh
BFME1_INSTALL="$PWD/games/BFME1" godot --path godot -- --game=bfme1
```

Alternatively pass `--bfme1-install="$PWD/games/BFME1"` alongside `--game=bfme1`.
Replace the folder relative to the checkout with your installation; `$PWD`
resolves it before Godot changes its working directory. On **Windows cmd.exe**, use
`build.bat` with MSVC/Ninja first, then run the Godot 4.7 console executable
with `--path godot -- --game=bfme1 --bfme1-install="%CD%/games/BFME1"`; the POSIX
environment-variable prefix above is not Windows command syntax.
The installation must contain its BIG archives, including `apt` and `lang`.
Installed patch archives are included in BFME1 archive order; launcher-generated
`!!!` graphics overrides are excluded and named in the mount report (`excluded_archives`). This mode fingerprints the installed
archives for content identity, but does not certify them against a retail 1.03
manifest. The existing RotWK verification policy is unchanged.

The launcher offers **Battle for Middle-earth I** in the Play screen's game selector
and passes the asset folder when starting BFME1 (`BFME1_INSTALL`, a remembered
`BFME1_INSTALL=` in the game's `install-paths.cfg`, `downloaded-installs.cfg`
or `downloaded-bfme1.cfg`, the default games folder's `BFME1`, or an existing
macOS AIO installation). The command-line equivalent is `--game=bfme1 --play` on
the launcher; the selection persists. With **Battle for Middle-earth I** chosen,
**Game files** offers an OPTIONAL download of BFME1 1.03 (the `original-BFME1`
package, from BFME Arena / bfmeladder.com, a third party: OpenBFME does not host
it); it goes into `<folder>/BFME1` and is found from then on
([AIO.md](AIO.md#bfme1-optional-community-pr-36-restored-2026-10-18), "BFME1 (optional)").

BFME1 has separate `bfme1-Options.ini` and `bfme1-Skirmish.ini` settings.
`--game=bfme1` and `--bfme1-install=...` are normal player options, so profiles persist between launches.
Automation flags such as `--auto` still use an isolated temporary profile file.
Its archive fingerprint cache is `bfme1-archive-cache.cfg` in the user data folder.

## Source and implementation

The BFME1 donor was inspected at `77b050212e`. Keep a local reference checkout
under `workspace/reference/open-bfme-1`; paths below are relative to that
donor, not this checkout. Relevant sources:

- `game/GameEngineDevice/Source/Win32Device/Common/Win32BIGFileSystem_loadBigFiles.cpp`:
  descending archive order, first archive wins.
- `game/GameEngine/Source/GameClient/GameText.cpp`, `parseCSF` at `0x00437570`:
  complemented UTF-16 strings, first string per label, optional voice records.
- `game/GameEngine/Source/GameClient/GUI/AptMainMenuConstructor.cpp` and
  `BfmeAptScreenMainMenuProvider.cpp`: BFME1 menu commands and providers.
- `game/GameEngine/Source/Common/Thing/ThingTemplate.cpp` and
  `GameClient/Drawable/Behavior/RandomSoundSelectorFieldParse.cpp`:
  legacy object and voice fields.
- `game/GameEngine/Source/Common/INI/INIAudioSettings.cpp` and
  `GameClient/Eva.cpp`: BFME1 audio settings and predefined EVA names.

The shared engine reads BFME1's subsystem legend and omits expansion-only
Stances/Create-a-Hero initialization. Language files under `lang/english` are
exposed at the root, as expected by existing menu readers. Both the shell and
in-game text consumers understand CSF.

## Skirmish profile rank thresholds

The profile table `{0,5,10,30,50,150,300,500,800,1500}` comes from BFME1
1.03's engine constants. The reading aid is
[Open-BFME-1 at `f633d0e97a953289405438821be0c09b28527bbb`](https://github.com/Open-BFME/Open-BFME-1/tree/f633d0e97a953289405438821be0c09b28527bbb/game/GameEngine/Source/Common),
with the following source files relative to `game/GameEngine/Source/Common/`.
All addresses here are **RVAs**; the inspected image base is `0x00400000`.

- `Rva004D8EE0Ctor.cpp`, RVA `0x004D8EE0` (VA `0x008D8EE0`), writes those ten
  integers in order at offsets `+0x00` through `+0x24`.
- `SkirmishBattleHonorsConstructor.cpp`, RVA `0x0009E130`, passes `this+0x14`
  to that initializer through the jump thunk at RVA `0x00010807`. This ties
  the table to the skirmish honors object, beyond the address-based name.
- `SkirmishBattleHonors_rankHelpers.cpp`, `getRank` at RVA `0x0009D2F0`,
  scans the nine promotion thresholds starting at `this+0x18` (the value 5),
  advances while `points >= threshold`, and returns a rank from 1 to 10.
- `SkirmishBattleHonors_getPointsToNextRank.cpp`, RVA `0x0009D390`, subtracts
  points from the next threshold; `SkirmishBattleHonors_getRankWidth.cpp`,
  RVA `0x0009D4E0`, subtracts the current threshold from the next. Both
  return zero for rank 10.

The instructions were inspected in the donor's `bfme1.retail-1.03-unpacked`
baseline, `inputs/baselines/bfme1/retail-1.03-unpacked/files/lotrbfme.exe`,
SHA-256 `1fbdc0ced8f283732c094c4f0804ce8dc1e3e3675b720bcab066c94fed964e75`.
Its manifest describes a SafeDisc-unpacked image with crack patches reverted,
and records remaining repairs/unknown regions. None of its `reverted_patches`
or `no_ground_truth` ranges overlaps the constructors, thunk or rank helpers
cited above. This is static evidence from that identified baseline, rather
than independent certification of a pristine retail image or a new byte-exact
recovery claim.

The profile display preserves its existing boundaries: reaching a threshold
promotes immediately. Below rank 10, progress resets to zero at the start of
the new interval; rank 10 keeps zero points remaining and 100 percent progress.
These constants and this documentation apply only to the BFME1 profile display.

## Isolation

BFME1 support never changes RotWK / BFME2 behaviour (community PR #36 review). `Common/BfmeGame.h`:

- **Which game:** `isBfme1Data(fs)` (BFME1's `SubsystemLegend.ini` without
  `SubsystemLegendExpansion1.ini` or BFME2's `AwardSystem.ini`), cached per INI
  environment (`INIEnvironment::bfme1Data`).
- **INI rows:** BFME1's extra rows are registered per table (`RegisterBfme1FieldRows`) and found only in BFME1 data; RotWK's tables stay
  the binary's, so RotWK data still fails on these fields as retail does. Value-level differences (AudioEvent `Control` names, special power
  `Enum` names, particle priorities, OCL dispositions, command points) ask `INI::bfme1Data()`.
- **Simulation:** differences ask the template (`ThingTemplate::bfme1Data()`, set by a ThingFactory built for BFME1 data): the vision default,
  build plots under their occupant, the castle flags. A RotWK game's state hash and saves are unchanged.
- **Files read through a file system ask that file system:** map chunks (`MapReadOptions::bfme1`, set by every map / base / library reader
  from its file system), the map cache, and the W3D model and texture layout (`W3DFileSource::FlatAssetLayout`, `Resolve_W3D_Texture`'s
  `flatLayout`), which the simulation's asset manager (launch bones) also reads.
- **Client paths with nothing to ask** (the texture decoder's PNG / JPEG sniffing, the Apt VM's type-8 constants, floor picking) ask
  `bfme1Mounted()`, set by the BFME1 mount once its archives pass `isBfme1Data` and cleared otherwise. It never decides a game rule.
- What BFME1 mode infers rather than ports is stop **S-5017** ([STOPS.md](STOPS.md)), in the BFME1 mount's report.

## How the BFME1 data differs

These are the places where BFME1's data or menus differ from RotWK's. Each one is handled when the BFME1 archives are mounted
(`isBfme1Data`, `Common/BfmeGame.h`: `SubsystemLegend.ini` without
`SubsystemLegendExpansion1.ini` or `AwardSystem.ini`):

- **Models and textures** are flat: `art\w3d\<name>.w3d` and `art\textures\<name>.dds`, with no two-letter folders
  (`W3D_Set_Flat_Asset_Layout`, set by the mount).
- **Command points:** GameData has `GoodCommandPointsMP56` / `MP78` (5–6 and 7–8 players), not one row per count
  (Open-BFME-1 `Player_newMap.cpp`).
- **AIData has no `DeepWaterDepth`, and GameData has no `MaxCells*` pathfinder limits.** BFME1 compiles its limits in:
  5000 cells per frame, 400 adjust tries, 2000 patch path cells, 2500 attack path cells, and 30000 cell infos
  (Open-BFME-1 `AIPathfind.cpp`). INFERENCE: the horde-melee, melee-engagement, possible-destination and towards-goal
  budgets have no located constant, so they keep RotWK 2.01's values.
- **Script libraries** use `MPPositionInfo` version 0: the two flags and the team, with no LoadAIScript flag and no
  side restrictions.
- **Victory:** GameData has no `VictoryCondition*ObjectFilter` rows. INFERENCE: a base is a `STRUCTURE` that is not
  `IGNORE_FOR_VICTORY`, and an army is any unit that is not a `DOZER` (the compiled rule isn't in Open-BFME-1).
- **Vision defaults:** an omitted `ShroudClearingRange` defaults to `VisionRange`, as in Open-BFME-1 `Object.cpp`.
  Gondor's keep relies on this; treating it as zero hid the keep and castle floor behind shroud.
- **Last map:** selecting a skirmish map immediately saves the current profile's lobby. The preferred map is restored
  even when the remaining saved lobby settings cannot be reused.
- **Start castles:** a match starts with each player's castle standing. INFERENCE: a castle flag that uses
  `UseTheNewCastleSystemInsteadOfTheClunkyBuildList` (no RotWK castle has that row), and whose owner's faction has a
  `CastleToUnpackForFaction` entry, unpacks that castle at once and for free on its first update. Neutral flags keep
  the normal unpack price. The retail trigger (the caller of `CastleBehavior::initiateUnpack`) isn't in Open-BFME-1.
- **Castle layouts:** BFME1's new castle system places its authored layout without per-piece overlap checks;
  adjacent wall and gate geometry deliberately overlaps. Applying the RotWK placement check drops those pieces.
- **Occupied plots:** starting construction hides and deselects a regular build plot, closing its commands. The plot returns when its occupant is removed.
- **Ground texture alpha:** JPEG ground textures take priority over paired PNG alpha masks and load their alpha through the same packed-texture merge used by the UI.
- **Build-plot picking:** plot surfaces use `W3DFloorDraw`, so mouse picking tests their visible floor triangles
  as well as scripted model draws. Hidden floors remain unpickable.
- **Skirmish maps:** the lobby lists the mapcache's multiplayer maps, as for RotWK (Sol review r1: no file-name filter; in BFME1 1.03 every
  multiplayer entry is a `map mp` map with two or more players).
- **Skirmish presentation:** gadget atlases use BFME1's flat `art/textures` packed textures, including TGA data stored under `.dds` names. The map list uses the retail two-column
  badge/count-bearing-name layout and BFME1 difficulty badges; the profile and map-type dropdowns bind their actual choices. Profile text,
  faction presentation and rank read the selected profile's `SkirmishStats.ini`; unavailable historical values are not invented.
  New personas start with fresh statistics even when reusing a deleted name; map badges refresh with profile changes and recorded results.
  Faction presentation uses the loaded `PlayerTemplate` index and `Side` (`APT:<Side>Caps`), not a retail-faction table. Rank artwork follows
  BFME1 `Rva0046F910RankDisplay.cpp`: use the mounted `AptRankIcon<Side><Rank>` image when present, otherwise the native generic `AptRankIcon<Rank>`.
- **Texture replacements:** PNG/JPEG files are accepted by BFME1 texture lookup (including `GBCampFloor`), and image signatures are recognized even when replacements retain `.dds` filenames.
- **Palantir commands:** BFME1 uses six fixed `CommandButtons.Command1` … `Command6` slots and `SetCommandButtonState`, rather than BFME2's dynamic content frames. Slot state updates only run when content changes, so show animations can finish.
- **Command tooltips:** BFME1's legacy cursor blocks are skipped to their `End` when loading help fonts. A native help panel displays the existing command-help text because BFME1 has no BFME2 `OnHelpBoxLoaded` content-frame callback.
- **Pause menu:** BFME1 uses type-8 `PushData` constants to reference the active constant pool. Resolving these initializes each button's own text and callback, including the Restart/Exit confirmations.
- **Keys and voices:** only `Data\INI\CommandMap.ini` exists (there's no root `CommandMap.ini`), and there's no
  `CrowdResponse.ini`, because BFME1 has no crowd responses.
- **Siege initiation sounds:** the update-based siege deploy powers retain BFME1's `InitiateSound2` alongside
  `InitiateSound` in their own base parser. BFME2/RotWK still reject the extra row. This fixes siege-ladder object
  loading; initiation-sound playback remains unimplemented under S-4942.
- **Legacy behavior fields:** `DynamicPortalBehaviour` reads `AllowKindOf` and `RejectKindOf` only for BFME1 data.
  A unit must match any allowed kind and no rejected kind; an explicitly empty allow mask admits nothing.
  These become the shared waypoint filter with an ANY rule, not ordinary Object fields. Sources: Open-BFME-1
  `DynamicPortalBehaviourRva001F96E0.cpp` and `Map/Waypoint_method001ABBB0.cpp`.
  The default allows all defined kinds and rejects none. Rejecting `CAVALRY` also rejects units with the
  `MOUNTED` model condition, even without the cavalry kind. This does not establish full BFME1 waypoint parity.
- **Automatic pickup:** BFME1 selects its own typed `AutoPickUpUpdate` data and runtime variant. Its
  `PickUpKindOf` mask admits any matching kind, retains `+`/`-` edits and clears with `NONE`.
  `Bored` and `BoredFilter` run the integrated allied-target boredom scan after eating and before ordinary
  pickup, using the same scan/button gate. Sources: BFME1 field table VA `0x010BB9D0` and Open-BFME-1
  `GameLogic/Object/Update/AutoPickUpUpdateUpdate.cpp`, RVA `0x00281F40`. The port maps the boredom power's
  name to the shared enum ordinal; BFME1's binary ordinal differs. BFME2/RotWK retain `PickUpFilter`
  (RW `0xC65B60`) and their separate `BoredUpdate`; they reject the BFME1-only fields. `PickUpKindOf2`
  is absent from BFME1's verified table and is rejected. The shared tree-point/automatic-throw limitations
  remain reported by S-7160; BFME1's broader simulation compatibility remains S-5017.
  `BloodthirstyUpdate` also reads BFME1's `NumToSacrifice` (default 1) and `InitiateVoice2` without widening
  BFME2/RotWK's field tables. The second initiation voice plays alongside the first, independently of crowd responses
  (`MessageStream/CommandXlatVoice.cpp`, case `0x7E9` and its two-slot playback tail). Voice voting groups the ordered
  pair of initiation sounds case-insensitively, including choices with only a second sound.
  The recovered BFME1 `Update/Rva00287290Update.cpp` awards experience on every accepted sacrifice, as the shared
  runtime does; it does not gate experience on `NumToSacrifice`. That field is retained, but any further purpose is
  unverified, so no quota-based veterancy rule is invented.
- **Main menu:** `MainMenu.apt` shows its menu bar only when `ShowMainMenu()` is called. INFERENCE: BFME1's
  `OnInitialized` makes that call (Open-BFME-1 declares `_bfme_onInitialized` without its body). Its button instance
  names don't match the buttons: the clip named `Skirmish` is the Campaign button, and `Tutorial` is Skirmish. Scripts
  find buttons by their `buttonName` label (`$SoloPlay`, `$Skirmish`), as a player does.
- **Skirmish lobby:** `Skirmish.apt` opens with `OpenDefault`, its persona popup opens with `PopUpPersona` and closes
  with `PopUpControl("close")`, and it sends `Skirmish::PersonaAccept` / `PersonaCancel` / `PersonaRemove` /
  `PersonaOk`. The Start button is `bottomNav.StartGame`.

## Limits

This is not yet a complete or retail-compatible BFME1 implementation. The shared
simulation still has unported modules, and BFME1-specific castle, horde, audio,
and combat behavior needs further comparison with the original game. Legacy
fields that lack runtime consumers are retained rather than discarded.
BFME1 particle priorities currently map to the shared renderer's quality tiers;
its original particle budget is not reproduced. One shipped unresolved mounted
Faramir voice reference (`UnitSpecificSounds` `VoiceGarrison = FaramirKnightVoiceGarrison`, which no AudioEvent defines) loads as BFME1 loads
it: BFME1's `UnitSpecificSounds` lines store the event name without looking it up (Open-BFME-1 `ThingTemplate_parseArbitrarySoundsIntoMap.cpp`).

The Good and Evil campaigns are scripted acts, not BFME2/RotWK's turn-based War of the Ring:
`GameLogic/LivingWorld/ActCampaign.h` reads the campaign blocks and runs the acts, armies and regions, with its
target facts per address; `godot/scripts/act_campaign.gd` draws the world screen and `campaign_flow.gd` plays a
region's mission. `LivingWorldStores` still holds recording handlers for those blocks (BFME2/RotWK keep their typed
parsers and S-1321 errors, and their schemas are not widened to accept BFME1 campaign rows).
Under S-5017, not ported: the campaign's saves (Continue and Load Campaign show the unavailable screen), the world
screen's Apt interface (the region window is provisional), the player's army moves, the eye tower, animated objects,
and `ScoreScreen.apt`. An SPS region map starts as BFME1's single-player skirmish (mode 6): `ActCampaignLogic::skirmishSlotsOf` picks the
region's `SkirmishOpponent` entries for the current act, each slot takes over the map side of its name, and the player takes `PlayerHuman`.
The player's faction does not yet follow the invading army, since the player does not move armies.
BFME1 currently uses the English language view.

## Checks

When `BFME1_INSTALL` is set, the release gate (`tools/release/test_export.sh`)
checks the packaged executable reaches `MainMenu.apt`, then runs the packaged
`bfme1_test.gd` against that package's library and resources. An invalid install,
a skipped test, a missing success marker, a script error or a nonzero exit fails
the gate. When `BFME1_INSTALL` is unset or empty, both BFME1 checks print `SKIP`
without failing publication; BFME1 packaged startup is unverified on that builder.
Both nightly and stable candidates use these conditional checks.

Synthetic CSF tests are included in `openbfme_tests --test-case="GameText:*"`.
Legacy portal filtering, both bloodthirsty voice slots, per-sacrifice experience, and BFME2/RotWK rejection are
covered by `openbfme_tests --test-case="bfme1 isolation:*"`. A core-only installed-data check can run without Godot:

```sh
BFME1_INSTALL="$PWD/games/BFME1" engine/build/openbfme_tests \
  --test-case="bfme1 isolation retail: installed BFME1 object world*"
```

It mounts BIG archives in BFME1 order (excluding launcher-generated `!!!` overrides), checks every subsystem parse
error, and verifies the castle-upgrade parent and child templates load. Without `BFME1_INSTALL` it prints SKIP;
that is not a passed installed-data check. It does not launch gameplay or write player settings.

Run `engine/build/openbfme_tests --test-case="GameText:*"` from the checkout
root on Linux after building the core tests (Windows: `engine\build\openbfme_tests.exe`
with the same argument). No retail data is needed for these synthetic tests.
The following **Linux/macOS POSIX-shell** checks use the editor binary, not an
export template, and require the built extension and BFME1 data described above.
The installed-data smoke test is:

```sh
BFME1_INSTALL="$PWD/games/BFME1" godot --headless --path godot \
  --script res://tests/bfme1_test.gd
```

It checks object loading, the real main menu and Skirmish shell, and a
Gondor-versus-Mordor Fords of Isen game. It prints `BFME1 SKIP` and exits 0
when `BFME1_INSTALL` is unset; that exit is not a passed installed-data test.

The whole launch, scripted with real mouse events (main menu → Solo Play → Skirmish → persona → Start → 10 s of play):

```sh
BFME1_INSTALL="$PWD/games/BFME1" godot --headless --path godot -- --game=bfme1 --auto --check --advance=10 \
  "--map=maps/map mp fords of isen/map mp fords of isen.map" --faction=FactionGondor
```

`godot/tests/bfme1_quit_test.gd` checks the camp-floor material and all six pause-menu buttons with real movie clicks, including confirmations.

## Armée de campagne et terrain masqué (Moria, 05/11/2026)

- L'armée d'une région (Region +0xB8, direction +0xBC) vient de `ForceBattle` (`UseArmy`, `ArmyAttackDirection`) ou de la fin de marche d'une armée du joueur (0x7A4FD0). Elle passe à la mission par 0x7C3850 -> 0x787060 (TheGameLogic +0x174), puis 0x795E76 -> 0x767470 -> 0x767010 crée les objets et 0x760400 les fait marcher depuis `InitialWalkOn*` ou `<Top|Left|Right|Bottom>Army{Spawn,Gather}Point`. Port : `ActCampaignLogic::walkOnArmyOf`, `StartingBase::placeInitialWalkOn`.
- INFÉRENCE : le joueur ne déplace pas encore d'armée sur la carte (S-5017) : sans `ForceBattle`, l'armée est celle qu'il possède et contrôle, la plus proche du centre de la région.
- Non porté (S-5018) : couleur de l'armée, tri des entrées, argent et sciences de campagne (0x767470), renforts appelés (0x767330), `TEAM_ASSIMILATE_*`, contenu suivant `MergePlayerArmy` (0x7B4510).
- `TERRAIN_RENDER_DISABLE` (372, 0x709B83 -> 0x6EFC30) : octet TheTerrainLogic +0x18F4, sauvegardé (0x5AD924) ; 0xB14405 saute l'objet de rendu de classe 4. Port : `GameLogic::setTerrainRenderDisabled`, version 5 de `obfme:GameLogic`, nœud `Terrain/Terrain` masqué.

## Retours de jeu sur la Moria (05/11/2026)

Faits cible, vus dans `lotrbfme.exe` (VA = offset fichier + 0x400000) :

- **Son.** Table Control en 0x12A8274 : LOOP (1), RANDOM (2), RANDOMSTART (4), INTERRUPT (8), FADE_ON_KILL (0x10), FADE_ON_START (0x20). generateFilename 0x4B3970 teste le bit 2 en 0x4B3A37 : levé, tirage au hasard ; baissé, fichier suivant. `DefaultSoundEffect` a `Control = random`, hérité partout. Le RANDOM de BFME1 tombait sur 0x40 (`AC_NOT_USED_BY_INI`) : les fichiers perdaient dossier et extension, d'où l'absence de tout bruitage et de toute voix.
- **Clic droit.** Avec la souris alternée (par défaut), 0x9B0690 fait porter la commande en attente par le clic droit, 0x9B0805 ignore le clic gauche et 0x9B9160..0x9B916E n'annule pas au relâchement. RotWK fait l'inverse : c'est ce qui annulait le pouvoir d'un héros et lançait le défilement de la caméra.
- **Portrait de groupe.** 0x996E85..0x99703C : table en dur camp vers `UPMordor_Army`, `UPGondor_Army`, `UPIsengard_Army`, `UPRohan_Army` ; `MultiPortrait` (l'image « Lots of Guys ») seulement pour un autre camp.
- **Niveau des héros.** 0x996081..0x9960E0 écrit dans `APT:PalantirHero%dRank` (0x110C084, numéro de bouton + 1) ; `Palantir.apt` lit `$Palantir<bouton>Rank`.
- **Brouillard en campagne.** La révélation de 0x62F91A ne concerne que les emplacements d'une partie configurée. La Moria appelle l'action 483 (`SET_LOGIC_FOG_STATE`, « Default TRUE ») et ses scripts révèlent des zones (`MAP_REVEAL_PERMANENTLY_AT_WAYPOINT`). Notre drapeau `ShroudManager::displayed` n'était levé que pour une escarmouche : le joueur voyait des ennemis que la logique tient pour cachés, et l'ordre d'attaque était refusé sans rien dire (VIS-1). Il est maintenant levé aussi en campagne.
- **Modules de la Moria.** `moriamodules.ini` : `KindOf = ... INERT INERT_SHROUD_REVEALER`, boîte de 180 autour de l'origine, maillages bien plus longs. 0x5CA2E0 les enregistre dans la partition ; 0x5C7B30 ne les exempte pas du statut.

Fait donneur (Open-BFME-1, `W3DScene.cpp:538`) : le jeu assombrit par pixel les objets non dégagés avec `m_shroudMaterialPass`.

Inférence, provisoire : faute de cette passe sur nos modèles, les objets `INERT_SHROUD_REVEALER` restent affichés sur une carte au terrain masqué (`LogicSnapshot.cpp`) ; la logique garde leur vrai statut. Sans cela, le sol disparaissait sous les héros (`MMCNatBridgeA01` : Gandalf à 493 de l'origine, statut caché). Conséquence connue : **les murs et le sol de la Moria ne sont pas assombris par le brouillard, et se voient même loin des héros** ; les ennemis, eux, sont bien cachés. À remplacer par la passe : `material_overlay` multiplicatif lisant `shroud_tex`, à brancher dans `GodotW3DInstancer`.

Reste à faire : la sélection de l'armée sur la carte (S-5017), les plans bleu-vert en haut à gauche de la salle d'entrée, l'objet à fond blanc-bleu (bouclier et lances) près du pont, et les actions non portées vues à la Moria : `CAMERA_RESTRICT_TO_AREA`, `LOCK_CAMERA_ANGLE_AND_HEIGHT`, `COMMANDBAR_REMOVE_BUTTON_OBJECTTYPE`, `MUSIC_PUSH_TRACK_FINITE_TIMES_AND_NOTIFY`, `UNIT_SET_MODELCONDITION`, `AUDIO_SET_REVERB_ROOM_TYPE`, `SET_LOGIC_FOG_STATE`.

## Moria : zones de script, voix, musique, brouillard (06/11/2026)

- **Zones de script.** Les cartes de BFME1 n'ont pas de bloc `TriggerAreas` : leurs zones sont dans `PolygonTriggers` (`MapChunks.cpp`). Aucune n'arrivait aux scripts : troll invulnérable, tombeau de Balin jamais lancé, sur toute carte de campagne.
- **Durée des voix.** `HAS_FINISHED_AUDIO` mesurait le chemin non localisé, donc 0 ms. `ScriptAudioLength` cherche aussi sous `lang\<langue>\` (0x4B3730).
- **Récit de région (inférence, S-5017).** `SkirmishVoiceTrack` (Galadriel à la Moria) passe après le film, sur l'image de chargement, et se coupe d'un clic. Ordre repris du chargement de mission de Zero Hour, à confirmer sur `lotrbfme.exe`.
- **Musique (inférence, S-5017).** `AUDIO_PUSH_MUSIC`, `AUDIO_POP_MUSIC`, `MUSIC_PUSH_TRACK_FINITE_TIMES[_AND_NOTIFY]` : pile côté client. Le drapeau se lève à la durée de la piste, comptée depuis la demande : une piste poussée par-dessus ne suspend pas ce décompte.
- **Brouillard par pixel.** Les modèles lisent la grille du brouillard (`w3d_shroud_on`, `w3dshadergen.cpp`) quand le terrain est masqué, comme `m_shroudMaterialPass`. L'exception de `LogicSnapshot.cpp` reste : le décor est dessiné, puis assombri.
- **Tombeau de Balin.** `Balins_Final_Check` attend les 9 héros dans la salle, plus le troll et `Throne_Room 1` dehors (ou tous deux morts). `NAMED_OUTSIDE_AREA` (RW 0x7E72B2, lotrbfme 0x324AA0) et `TEAM_OUTSIDE_AREA_ENTIRELY` (lotrbfme 0x324BD0) sont des négations, vraies pour un mort : notre port répondait faux, et un troll mort avec des gobelins encore vivants bloquait la mission pour de bon.
- **Autres ports (inférence, S-5017, à comparer au décompilé).** `TEAM_TELEPORT_TO_WAYPOINT` (lotrbfme 0x2F7280), `UNIT_COMPARE_RANK`, `NAMED_RANK_LEVEL` (lotrbfme 0x327560), `ROTATE_CAMERA_TO_ANGLE`.
- **Provisoire.** Un clic près de l'icône d'une armée la sélectionne et l'annonce (`act_campaign.gd`) ; la marche choisie par le joueur n'existe pas encore.
- **Non portés, vus à la Moria.** `CAMERA_RESTRICT_TO_AREA`, `CAMERA_REMOVE_AREA_RESTRICTION`, `LOCK_CAMERA_ANGLE_AND_HEIGHT`, `COMMANDBAR_REMOVE_BUTTON_OBJECTTYPE`, `SET_LOGIC_FOG_STATE`, `UNIT_SET_MODELCONDITION`, `AUDIO_SET_REVERB_ROOM_TYPE`, `TEAM_ATTACKED_BY_OBJECTTYPE` (le troll ne réagit donc pas à un tir de loin avant qu'on entre dans sa zone).


## Pouvoirs visés de BFME1 : l'épée de foudre de Gandalf (06/11/2026)

Trois défauts distincts bloquaient le 2e pouvoir de Gandalf à la Moria. Le pointeur était caché, aucun cercle n'apparaissait, le clic droit était refusé et aucun clic ne sortait de la visée.

- **Surcharges de `map.ini` (commun aux trois jeux).** La recherche d'un pouvoir renvoie l'entrée du magasin, pas sa dernière surcharge (RW 0x7B1D71, 0x7B1AFB ; lotrbfme 0x0EB6EA). Le module garde cette entrée et `getSpecialPowerModule` compare ce pointeur (RW 0x8969D1). Seules les valeurs viennent de la dernière surcharge (`getFO` de Zero Hour). Le port renvoyait la surcharge : tout pouvoir redéfini par une carte devenait introuvable, et les modules lisaient les valeurs de base. Les champs de `SpecialPowerTemplate` sont maintenant privés, le compilateur refuse toute lecture directe.
- **Rôles des clics (lotrbfme 0x5B1C70, `Rva005B1C70.cpp`).** Avec la souris alternée, le clic droit exécute et un clic gauche non glissé annule toute commande en attente. Ce clic poursuit son chemin : sur le sol vide, il désélectionne aussi (lecture du code, à confirmer en jeu). Le pouvoir du grimoire suit la même règle.
- **Cercles de visée.** BFME1 n'a pas de bloc `RadiusCursorTemplate` : chaque type a son champ dans `InGameUI` (table 0xCF4158, 0x30 octets par type à partir de + 0x8C4, dans l'ordre des noms à 0x12B6078). Les 52 lignes sont dans `RadiusCursor.cpp`. `NONE` est l'indice 0 : pas de cercle, le pointeur de la commande reste.
- **Terrain masqué.** `TERRAIN_RENDER_DISABLE` saute le maillage, pas les décalques, dessinés à partir de la carte des hauteurs (ZH `W3DProjectedShadow.cpp`, `queueDecal`). Ses morceaux prennent `terrain_hidden_decals.gdshader`, qui ne montre que le cercle. Inférence : le décalage de 4 unités vers la caméra (`DECAL_LIFT`) ; le test de profondeur de la cible n'est pas repris. Les décalques de sélection ne passent pas encore par ce matériau.

## La scène du Balrog à la Moria (06/11/2026)

- **Invocation.** `FIRE_SPECIAL_POWER_ON_TEAM` (lotrbfme 0x6F8340) lance le pouvoir qui crée `MordorBalrogEgg`. Après `DestructionDelay = 4000`, `OCL_BalrogSummoningFinish` crée `MordorBalrog` (`UseJustBuiltFlag`, `FadeIn`, `FadeTime = 10000`).
- **Armes de `map.ini` (commun aux trois jeux).** Les armes se relisent par l'entrée courante du magasin (`WeaponSet::updateWeaponSet`, lotrbfme 0x5EBBC0, RW 0x6C9ADC). Sans cela, un coup du Balrog infligeait 3600 au lieu de 300.
- **Particules.** BFME1 range ses textures à plat dans `art\textures\`, sans `art\compiledtextures\xx\` : toutes les particules étaient des carrés blancs. Certains `.dds` sont en réalité des TGA, PNG ou JPEG ; la signature décide (`Decode_Retail_Texture`).
- **Corps du Balrog (commun aux trois jeux).** `JUST_BUILT` tient `MUBALROG_RORA` à l'image 0 en MANUAL, sous le sol, puis `TRANS_JustBuilt_To_Idle` le fait jaillir. RW 0x4BF560 (`advance`) tourne pour chaque modèle, le port le réservait aux modèles animés : une piste 0 MANUAL n'en sortait jamais. `needsAdvance()` compte maintenant le mélange en cours et l'état en attente.
- **Fondu des OCL (commun aux trois jeux).** RW 0x5F1678 .. 0x5F17B7 : `FadeIn` s'applique au drawable de l'objet ou, pour une horde (KindOf bit 109), à chacun de ses membres ; `FadeOut` au drawable de l'objet. `FadeSound` reste non porté et remonte au rapport. L'effet visible des 10 s n'a pas été mesuré à l'écran.
- **Données du combat.** Armure `BalrogMoriaArmor` : `MAGIC` 100 %, `SIEGE` 25 %, `HERO` 2 %, le reste 1 %. Le Balrog a 2350 PV sur cette carte, les coups normaux de Gandalf (`HERO`) ne lui en ôtent que 2. L'épée de foudre (`GandalfLightningSwordBlastWeapon`, `MAGIC`) tire au plus 5 coups de 100, recharge 20 s. Les scripts `Gandalf Invulnerability` et `Gandalf Vunerable` sont inactifs et ne s'activent que l'un l'autre : Gandalf peut mourir, et sa mort fait perdre la mission.
- **Mesures du combat (sonde, clics réels, Gandalf monté niveau 10).** Chaque épée ôte 1000 PV (5 coups de 200) : le double des 100 de `map.ini`, cohérent avec ses coups normaux à 4 au lieu de 2 ; le bonus de niveau n'a pas été lu dans les données, ni l'épée mesurée à bas niveau. 26 s entre deux épées (20 s de recharge et l'animation). Fin de mission vue jusqu'au retour à la carte, films coupés. L'équilibre du combat n'a pas été comparé au jeu d'origine.

## La fin de la Moria (06/11/2026)

- **Enchaînement de la carte.** `Mission Won`, voix, `SPEECH_PLAY GoodMoria2Cine`, `CAMERA_FADE_SUBTRACT`, 5 logiques plus tard `VICTORY_SCREEN`, 25 logiques plus tard `PLAY_MOVIE_IN_GAME Good_AmonHen_Outro` et `QUICKVICTORY`.
- **Bannière de victoire.** `ScriptActions::doVictory` (décompilé open-bfme-1) : `showScreen("APT:EndVictorious", camp, "Gui_VictoryScreen", cri)`. `VICTORY_SCREEN` ne met pas fin à la partie et ne démarre pas le minuteur de fin (RW 0x7C45E0 → 0x602FFE, action VICTORY seule). Le port passe par le compteur `ScriptEngine::victoryScreenRequests` et `EndGameView::scriptVictoryScreens`.
- **Fondu sous l'interface.** Le quad du fondu fait partie du rendu 3D (fin de `W3DStatusCircle::Render`, RW 0x48E0EE) : l'interface Apt se dessine par-dessus. `campaign_cine.gd` le place en couche -1, ce qui vaut pour les trois jeux.
- **Brouillard global des modèles.** `w3d_shroud_on` touche tous les matériaux W3D, y compris ceux d'une scène séparée. Le shader exempte les instances à `W3D_NO_SCENE_FOG_BIT`, posé sur le globe du Palantir (INFERENCE, constructeur de scène RW 0x5424E0). `clear_game_data` éteint le brouillard : allumé après la mission, il rendait la carte des actes noire.
- **`SummaryEvent` de la carte.** Ajout 0x7C2280 de `lotrbfme.exe` : un événement `SummaryEvent` va dans la file +0x5C, jouée uniquement après échange au chargement d'une sauvegarde (0x7C1190, appelé à 0x7C45E7). Il n'est donc pas joué en partie normale. Le résumé au chargement n'est pas porté.
- **Grimoire.** Les pouvoirs achetés s'affichent et se lancent (`syncSpellBookBfme1`). `CAMEO_FLASH` n'est pas porté : le bouton ne clignote pas.
- **Reste à porter.** `ScoreScreen.apt` : retour direct à la carte, sans écran des scores. Voix de l'acte One : `m_timed` reste figé pendant la mission, donc une voix pas encore échue à l'entrée de la Moria part au retour sur la carte (vu avec le raccourci d'entrée à la 5e image). La fonction de vidage retail vers 0x7C0F70 n'a pas été étudiée.
- **Non reproduit.** Gandalf qui glisse : animation correcte en sonde au niveau 1, au niveau 5, après la scène, après l'épée et en poursuivant le Balrog.
- **Défaite, à vérifier.** Le port traite `DEFEAT` par `addEndRequest(false)` (RW 0x7BF15E) et le journal quitte aussitôt la partie vers la carte, sans trace de bannière. `ScriptActions::doDefeat` (décompilé open-bfme-1) ouvre pourtant un écran `Gui_DefeatScreen`. L'écran n'a pas été regardé.

