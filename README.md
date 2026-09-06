# Arcade — Lyra co-op arena (code excerpt)

A code-focused excerpt of an Unreal Engine 5.8 project built on Lyra:
a networked listen-server co-op wave-survival mode added as a Game Feature
plugin (`Plugins/GameFeatures/Arcade`).

## What's here

- **`Source/LyraGame/Arcade/`** — gameplay C++
  - `AI/` — behaviour-tree tasks/services/decorators, config-driven enemy
    attack ability (`UAC_GameplayAbility_EnemyAttack`), wave spawner
    (`UAC_ArcadeWaveSpawner`), enemy stat component
  - `GameModes/` — character-select and co-op game modes
  - `Characters/`, `Player/`, `Data/`, `Teams/` — roster, data tables, teams
- **`Source/LyraGame/Character/Mover/`** — Lyra pawn on the experimental
  Mover plugin with Motion Matching / GASP integration
- **`Plugins/GameFeatures/Arcade/Content/`** — the plugin's own light
  gameplay assets (behaviour trees, abilities, data tables, experience,
  maps). Marketplace art and stock Lyra content are intentionally excluded.

## Not runnable as-is

Heavy/binary content (stock Lyra assets, marketplace creature packs) is not
redistributed here, so this repo compiles but will not play standalone. It
is meant to be read, not launched.
