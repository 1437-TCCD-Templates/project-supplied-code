/**
 * @file usage_example.cpp
 * @brief How Part 3 and Part 4 fit together.
 *
 * Shows `Character` subclasses (the Part 3 side), the two Level 1 casters, and
 * a short fight resolved through the spellcasting interface. Every spell is
 * called through a base-class pointer; nothing here asks what type a character
 * is.
 *
 * Run it with different seeds to see different fights: `./usage_example 42`
 */

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "otari/character.h"
#include "otari/dice.h"
#include "otari/spellcasting.h"

using namespace otari;

// ---- Part 3 would write classes like these (yours will have more in them)
// ----

/** @brief A hero with no special rules. Part 1 would add classes, gear and
 * levels. */
class Hero : public Character {
public:
  /** @brief Construct a hero with Perception +5 and the given defenses. */
  Hero(const std::string &n, int hp, int ac, int fort, int ref, int will)
      : Character(n, hp, ac, 5, fort, ref, will) {}
};

/** @brief Undead; immune to mental, poison and bleed; resists slashing and
 * piercing 5. */
class Skeleton : public Character {
public:
  Skeleton() : Character("Skeleton Guard", 4, 16, 2, 5, 3, 2) {}
  bool is_undead() const override { return true; }
  bool immune(DamageType t) const override {
    return t == DamageType::Mental || t == DamageType::Poison ||
           t == DamageType::Bleed;
  }
  int resistance(DamageType t) const override {
    return (t == DamageType::Slashing || t == DamageType::Piercing) ? 5 : 0;
  }
};

/** @brief A goblin with no special rules. */
class Goblin : public Character {
public:
  /** @brief Construct a goblin with the given name. */
  Goblin(const std::string &n) : Character(n, 6, 16, 2, 4, 5, 2) {}
};

/** @brief Print a heading and the narration for a cast. */
static void show(const std::string &heading, const CastResult &r) {
  std::cout << "[" << heading << "]\n" << r.text() << "\n\n";
}

/**
 * @brief Run the example fight.
 * @param argc Argument count.
 * @param argv Optional seed as the first argument (default 1).
 */
int main(int argc, char **argv) {
  const std::uint64_t seed =
      (argc > 1) ? std::strtoull(argv[1], nullptr, 10) : 1;
  dice().seed(seed);
  std::cout << "Seed " << seed << ", library " << spellcasting_library_version()
            << "\n\n";

  // ---- the two casters. Spell attack +7, spell DC 17 at Level 1. ----
  SpellcasterConfig kyra_cfg;
  kyra_cfg.owner = "Kyra";
  kyra_cfg.spell_attack = 7;
  kyra_cfg.spell_dc = 17;
  kyra_cfg.cantrips = {spell_id::kDaze, spell_id::kLight, spell_id::kStabilize};
  kyra_cfg.prepared = {spell_id::kCommand, spell_id::kCommand};
  kyra_cfg.font_slots = {{spell_id::kHeal, 4}};

  SpellcasterConfig ezren_cfg;
  ezren_cfg.owner = "Ezren";
  ezren_cfg.spell_attack = 7;
  ezren_cfg.spell_dc = 17;
  ezren_cfg.cantrips = {spell_id::kDetectMagic, spell_id::kLight,
                        spell_id::kTelekineticProjectile};
  ezren_cfg.prepared = {spell_id::kForceBarrage, spell_id::kForceBarrage,
                        spell_id::kMysticArmor};
  ezren_cfg.school_spells = {spell_id::kForceBolt};
  ezren_cfg.focus_points = 1;
  ezren_cfg.arcane_bond = true;

  // One SpellCaster per caster, held by unique_ptr.
  std::unique_ptr<SpellCaster> kyra_magic = make_spellcaster(kyra_cfg);
  std::unique_ptr<SpellCaster> ezren_magic = make_spellcaster(ezren_cfg);

  // The Character side of each hero.
  Hero kyra("Kyra", 17, 17, 6, 4, 7);
  Hero ezren("Ezren", 15, 15, 4, 5, 8);
  Skeleton skeleton;
  Goblin goblin("Goblin Warrior");

  // A menu is built from options(); here we just print it.
  std::cout << "Ezren can cast:\n";
  for (const CastOption &o : ezren_magic->options()) {
    std::cout << "  " << find_spell(o.spell_id)->name() << "  ("
              << o.min_actions << "-" << o.max_actions << " actions, ";
    if (o.uses_left < 0)
      std::cout << "unlimited)\n";
    else
      std::cout << o.uses_left << " left)\n";
  }
  std::cout << "\n";

  // Buff on self: Mystic Armor changes Ezren's AC through the Character
  // interface.
  std::cout << "Ezren AC before: " << ezren.armor_class() << "\n";
  show("Mystic Armor", ezren_magic->cast(spell_id::kMysticArmor, 2, ezren, {}));
  std::cout << "Ezren AC after: " << ezren.armor_class() << "\n\n";

  // Automatic damage: all missiles at one target, or one target per missile.
  show("Force Barrage, 3 missiles at the goblin",
       ezren_magic->cast(spell_id::kForceBarrage, 3, ezren, {&goblin}));

  // Saving throw: the skeleton is immune to mental damage.
  show("Daze on the skeleton",
       kyra_magic->cast(spell_id::kDaze, 2, kyra, {&skeleton}));

  // Attempt a cast that cannot work: the result explains why, and nothing is
  // spent.
  CastResult bad = kyra_magic->cast(spell_id::kCommand, 2, kyra, {&skeleton});
  show("Command with no order", bad);

  // Healing. Kyra heals Ezren, then heals the whole party-and-enemies with 3
  // actions.
  ezren.take_damage(9, DamageType::Untyped);
  std::cout << "Ezren HP before heal: " << ezren.hp() << "/" << ezren.max_hp()
            << "\n";
  show("Heal, 2 actions", kyra_magic->cast(spell_id::kHeal, 2, kyra, {&ezren}));
  std::cout << "Ezren HP after heal: " << ezren.hp() << "/" << ezren.max_hp()
            << "\n";
  std::cout << "Heal slots left: "
            << kyra_magic->slots_remaining(spell_id::kHeal) << "\n\n";

  // Heal harms undead: 3 actions affects everyone listed.
  show("Heal, 3 actions, ally and skeleton",
       kyra_magic->cast(spell_id::kHeal, 3, kyra, {&ezren, &skeleton}));

  std::cout << "Dice rolled this run: " << dice().rolls_made() << "\n";
  return 0;
}
