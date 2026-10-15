# Menace Under Otari: engine reference

Shared code for the Menace Under Otari group project.

## Where to start

- @ref otari_shared : `Dice` and the `Character` base class that every hero and monster derives from.
- @ref otari_spells_api : the Part 4 contract, with `Spell`, `SpellCaster`, `SpellcasterConfig`, `CastResult` and the free functions.

## Rules of the road

1. Hold characters by pointer or reference, never by value.
2. Every random number comes from `otari::dice()`.
3. Every point of damage goes through `Character::take_damage()`.
4. A refused cast changes nothing.

See `STUDENT_README.md` for the compile and link line.
