# Otari Spellcasting Library — Student Guide

This package is **Part 4** of the Menace Under Otari engine: spells, spell slots,
and the two Level 1 casters (Kyra and Ezren). It also contains the shared
`Creature` base class and the shared dice roller that every part of the engine uses.

## What is in the package

```
include/otari/dice.hpp          shared dice roller (header only)
include/otari/creature.hpp      shared Creature base class (header only)
include/otari/spellcasting.hpp  the Part 4 contract: Spell, SpellCaster, free functions
lib/libotari_spells.a           the compiled spell code (Windows MSVC: otari_spells.lib)
examples/usage_example.cpp      a short fight that uses everything
tests/test_contract.cpp         tests that check the contract
```

You never see the spell source code. You use it only through `spellcasting.hpp`.

## Compile and link

Put the package folder inside your project, then compile with the include path and
the library:

```bash
# Linux / macOS / MinGW / WSL
g++ -std=c++17 -Wall -Wextra -Iinclude main.cpp -Llib -lotari_spells -o game
```

- `-Iinclude` lets you write `#include "otari/creature.hpp"`.
- `-Llib -lotari_spells` links `lib/libotari_spells.a`. Put it **after** your `.cpp` files.
- You must use `-std=c++17` or newer.
- Use the same compiler family the library was built with (your instructor will say
  which). A library built with g++ will not link with MSVC, and the reverse.

Try the example and the tests first:

```bash
g++ -std=c++17 -Iinclude examples/usage_example.cpp -Llib -lotari_spells -o usage_example
./usage_example 42

g++ -std=c++17 -Iinclude tests/test_contract.cpp -Llib -lotari_spells -o contract_tests
./contract_tests
```

With CMake, add this to your `CMakeLists.txt` (adjust the folder name):

```cmake
add_library(otari_spells STATIC IMPORTED)
set_target_properties(otari_spells PROPERTIES
  IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/otari-spells/lib/libotari_spells.a
  INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/otari-spells/include)
target_link_libraries(my_game PRIVATE otari_spells)
```

## The five-minute tour

```cpp
#include "otari/spellcasting.hpp"
using namespace otari;

dice().seed(42);                              // one shared dice roller; seed it once

SpellcasterConfig cfg;
cfg.owner = "Ezren";  cfg.spell_attack = 7;  cfg.spell_dc = 17;
cfg.cantrips = {spell_id::kLight};
cfg.prepared = {spell_id::kForceBarrage, spell_id::kForceBarrage};
auto magic = make_spellcaster(cfg);           // std::unique_ptr<SpellCaster>

CastResult r = magic->cast(spell_id::kForceBarrage, 2, ezren, {&goblin});
std::cout << r.text() << "\n";                // ready-to-print narration
if (!r.ok) std::cout << r.error << "\n";      // nothing was spent; show why
```

Key points:

1. **`Creature` is your base class.** Heroes (Part 3) and monsters (Part 2) derive
   from it. Spells call its virtual functions (`take_damage`, `is_undead`,
   `immune`, `resistance`, `weakness`) and never ask what *type* a creature is.
   That is polymorphism, and it is why a skeleton resists a slash and a zombie does not.
2. **Damage goes through `take_damage`.** Always. If you subtract from `hp` yourself,
   resistances and weaknesses stop working.
3. **All randomness uses `otari::dice()`.** Never call `rand()` or make your own
   random engine. Same seed, same game: that is what lets your team reproduce a bug.
4. **A failed cast changes nothing.** `ok == false` means no slot was spent and no
   dice were rolled. Print `error` and let the player pick again.
5. **Scrolls and rest are the caller's job.** `cast_from_scroll` does not remove the
   scroll; your inventory code does that when `ok` is true.
6. Use `options()` to build the spell menu; do not hard-code spell names.

## The spells

`all_spell_ids()` lists all 16, and `find_spell(id)->summary()` gives each one's short
rules text. Each spell's target rule is documented in `spellcasting.hpp`.

| Kind | Spells |
|---|---|
| Automatic damage | Force Barrage, Force Bolt |
| Saving throw | Breathe Fire, Daze, Command, Telekinetic Projectile, Vitality Lash |
| Attack roll | Gouging Claw |
| Healing | Heal |
| Buff | Mystic Armor, Runic Weapon |
| Utility | Detect Magic, Figment, Light, Message, Stabilize |

## Common mistakes

- Passing the wrong number of targets (see the target rules in the header).
- Forgetting that Command needs an order in the `option` argument (`"approach"`,
  `"flee"`, `"drop"`, `"prone"`, `"stand"`).
- Storing a `Creature` by value in a `std::vector<Creature>`. That slices off the
  derived class. Store `std::unique_ptr<Creature>` or `Creature*`.
- Linking order: `-lotari_spells` must come after the files that use it.
- Reading the library version: `spellcasting_library_version()` returns `"1.0.0"`.

## If you are the team of four

You do not get the library. You implement `spellcasting.hpp` yourself: a `Spell`
base class with derived classes, a `SpellCaster`, and the three free functions at the
bottom of the header. Your work is correct when `tests/test_contract.cpp` passes
against it. Parts 2 and 3 are written exactly as the other teams write them.
