/**
 * @file test_contract.cpp
 * @brief Contract tests for the Part 4 spellcasting interface.
 *
 * Uses only the public headers (`otari/dice.h`, `otari/character.h`,
 * `otari/spellcasting.h`). Teams of 3 can run this against the precompiled
 * library to see the contract in action. The team of 4 runs it against their
 * own implementation of the same header: every test here should pass for both.
 *
 * Build and run: see STUDENT_README.md.
 */

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "otari/character.h"
#include "otari/dice.h"
#include "otari/spellcasting.h"

using namespace otari;

// ---------------------------------------------------------------- mini harness
static int g_checks = 0;   ///< Total checks evaluated.
static int g_failures = 0; ///< Checks that failed.
static std::string
    g_current; ///< Name of the test now running, for failure messages.

/** @brief Record a failure if @p cond is false. */
#define CHECK(cond)                                                            \
  do {                                                                         \
    ++g_checks;                                                                \
    if (!(cond)) {                                                             \
      ++g_failures;                                                            \
      std::cout << "  FAIL [" << g_current << "] line " << __LINE__ << ": "    \
                << #cond << "\n";                                              \
    }                                                                          \
  } while (0)

/** @brief Record a failure, printing both values, if `a == b` is false. */
#define CHECK_EQ(a, b)                                                         \
  do {                                                                         \
    ++g_checks;                                                                \
    const auto va_ = (a);                                                      \
    const auto vb_ = (b);                                                      \
    if (!(va_ == vb_)) {                                                       \
      ++g_failures;                                                            \
      std::cout << "  FAIL [" << g_current << "] line " << __LINE__ << ": "    \
                << #a << " == " << #b << "  (got " << va_ << " vs " << vb_     \
                << ")\n";                                                      \
    }                                                                          \
  } while (0)

/** @brief Record a failure unless evaluating @p expr throws
 * `std::invalid_argument`. */
#define CHECK_THROWS(expr)                                                     \
  do {                                                                         \
    ++g_checks;                                                                \
    bool threw_ = false;                                                       \
    try {                                                                      \
      (void)(expr);                                                            \
    } catch (const std::invalid_argument &) {                                  \
      threw_ = true;                                                           \
    }                                                                          \
    if (!threw_) {                                                             \
      ++g_failures;                                                            \
      std::cout << "  FAIL [" << g_current << "] line " << __LINE__            \
                << ": expected std::invalid_argument from " << #expr << "\n";  \
    }                                                                          \
  } while (0)

/**
 * @brief Run one test, catching stray exceptions, and print PASS or FAIL.
 * @param name Test name for the report.
 * @param test The test function.
 */
static void run(const char *name, void (*test)()) {
  g_current = name;
  const int before = g_failures;
  try {
    test();
  } catch (const std::exception &e) {
    ++g_failures;
    std::cout << "  FAIL [" << name << "] unexpected exception: " << e.what()
              << "\n";
  }
  std::cout << (g_failures == before ? "PASS  " : "FAIL  ") << name << "\n";
}

// -------------------------------------------------------------- test
// characters These are the kind of subclasses Part 3 writes. They show the
// contract: spells never ask "what kind of character is this?", they call
// virtual functions.

/** @brief A plain character with 15 AC and no special rules. */
class Dummy : public Character {
public:
  /** @brief Construct with a name and HP; AC 15 and zero modifiers. */
  explicit Dummy(const std::string &n = "Dummy", int hp = 100)
      : Character(n, hp, 15, 0, 0, 0, 0) {}
};

/** @brief Undead; immune to mental and poison; resists slashing and piercing 5.
 */
class Skeleton : public Character {
public:
  Skeleton() : Character("Skeleton", 100, 15, 0, 0, 0, 0) {}
  bool is_undead() const override { return true; }
  bool immune(DamageType t) const override {
    return t == DamageType::Mental || t == DamageType::Poison;
  }
  int resistance(DamageType t) const override {
    return (t == DamageType::Slashing || t == DamageType::Piercing) ? 5 : 0;
  }
};

/** @brief Undead; weak to slashing 5. */
class Zombie : public Character {
public:
  Zombie() : Character("Zombie", 100, 15, 0, 0, 0, 0) {}
  bool is_undead() const override { return true; }
  int weakness(DamageType t) const override {
    return t == DamageType::Slashing ? 5 : 0;
  }
};

/**
 * @brief A character with extreme save modifiers so saves are predictable.
 *
 * A large bonus never fails and a large penalty always fails; a natural 20 or 1
 * shifts at most one degree.
 */
class SaveBot : public Character {
public:
  /**
   * @param n          Name.
   * @param save_bonus Modifier applied to all three saves.
   * @param undead     Whether the character counts as undead.
   */
  SaveBot(const std::string &n, int save_bonus, bool undead = false)
      : Character(n, 200, 15, 0, save_bonus, save_bonus, save_bonus),
        undead_(undead) {}
  bool is_undead() const override { return undead_; }

private:
  bool undead_; ///< Returned by is_undead().
};

/** @brief Kyra's Level 1 configuration: cleric cantrips, two Commands, four
 * font Heals. */
static SpellcasterConfig kyra_config() {
  SpellcasterConfig c;
  c.owner = "Kyra";
  c.spell_attack = 7;
  c.spell_dc = 17;
  c.cantrips = {spell_id::kDaze, spell_id::kLight, spell_id::kStabilize};
  c.prepared = {spell_id::kCommand, spell_id::kCommand};
  c.font_slots = {{spell_id::kHeal, 4}};
  return c;
}

/** @brief Ezren's Level 1 configuration: wizard cantrips, prepared attack and
 * armor spells, Force Bolt, arcane bond. */
static SpellcasterConfig ezren_config() {
  SpellcasterConfig c;
  c.owner = "Ezren";
  c.spell_attack = 7;
  c.spell_dc = 17;
  c.cantrips = {spell_id::kDetectMagic, spell_id::kLight,
                spell_id::kTelekineticProjectile};
  c.prepared = {spell_id::kForceBarrage, spell_id::kForceBarrage,
                spell_id::kMysticArmor};
  c.school_spells = {spell_id::kForceBolt};
  c.focus_points = 1;
  c.arcane_bond = true;
  return c;
}

// ---------------------------------------------------------------------- dice
/** @brief Same seed gives the same rolls; a different seed does not; reseeding
 * restarts the sequence. */
static void test_dice_determinism() {
  Dice a(12345), b(12345), c(54321);
  std::vector<int> ra, rb, rc;
  for (int i = 0; i < 50; ++i) {
    ra.push_back(a.d20());
    rb.push_back(b.d20());
    rc.push_back(c.d20());
  }
  CHECK(ra == rb);
  CHECK(ra != rc);

  a.seed(12345);
  std::vector<int> again;
  for (int i = 0; i < 50; ++i)
    again.push_back(a.d20());
  CHECK(again == ra);
  CHECK_EQ(a.seed_value(), static_cast<std::uint64_t>(12345));
  CHECK_EQ(a.rolls_made(), static_cast<std::uint64_t>(50));
}

/** @brief Rolls stay within range and every face of a d6 appears. */
static void test_dice_range() {
  Dice d(7);
  for (int i = 0; i < 2000; ++i) {
    const int v = d.roll(6);
    CHECK(v >= 1 && v <= 6);
  }
  for (int i = 0; i < 500; ++i) {
    const int v = d.roll(3, 4);
    CHECK(v >= 3 && v <= 12);
  }
  // All faces of a d6 should appear in 2000 rolls.
  std::set<int> seen;
  for (int i = 0; i < 2000; ++i)
    seen.insert(d.roll(6));
  CHECK_EQ(seen.size(), static_cast<std::size_t>(6));
}

/** @brief Expression parsing, bounds, error cases, and rolling stays within
 * range. */
static void test_dice_expressions() {
  const DiceExpression e = Dice::parse("2d6+3");
  CHECK_EQ(e.count, 2);
  CHECK_EQ(e.sides, 6);
  CHECK_EQ(e.bonus, 3);
  CHECK_EQ(e.minimum(), 5);
  CHECK_EQ(e.maximum(), 15);

  const DiceExpression f = Dice::parse("d8 - 1");
  CHECK_EQ(f.count, 1);
  CHECK_EQ(f.sides, 8);
  CHECK_EQ(f.bonus, -1);

  const DiceExpression g = Dice::parse("5");
  CHECK_EQ(g.count, 0);
  CHECK_EQ(g.bonus, 5);

  CHECK_THROWS(Dice::parse(""));
  CHECK_THROWS(Dice::parse("2d"));
  CHECK_THROWS(Dice::parse("2d6+"));
  CHECK_THROWS(Dice::parse("banana"));

  Dice d(99);
  for (int i = 0; i < 200; ++i) {
    const int v = d.roll_expression("2d6+3");
    CHECK(v >= 5 && v <= 15);
  }
  CHECK_EQ(d.roll_expression("4"), 4);
}

/** @brief Flat checks at the extremes, and history recording and clearing. */
static void test_dice_flat_check_and_history() {
  Dice d(3);
  CHECK(d.flat_check(1));   // always passes
  CHECK(!d.flat_check(21)); // never passes
  d.set_history_enabled(true);
  d.clear_history();
  d.roll_expression("1d4");
  d.d20();
  CHECK_EQ(d.history().size(), static_cast<std::size_t>(2));
  d.clear_history();
  CHECK(d.history().empty());
}

/** @brief `otari::dice()` is one shared instance controlled by one seed. */
static void test_global_dice_is_shared() {
  dice().seed(2024);
  const int first = dice().d20();
  dice().seed(2024);
  CHECK_EQ(dice().d20(), first);
}

// ------------------------------------------------------------------ character
/** @brief HP bookkeeping: damage, healing capped at max HP, unconscious at 0.
 */
static void test_character_hp() {
  Dummy d("Dummy", 20);
  CHECK_EQ(d.take_damage(5, DamageType::Untyped), 5);
  CHECK_EQ(d.hp(), 15);
  CHECK_EQ(d.heal(100), 5); // returns HP actually restored
  CHECK_EQ(d.hp(), 20);
  CHECK_EQ(d.take_damage(500, DamageType::Untyped),
           20); // can't lose more than it has
  CHECK(d.is_unconscious());
}

/** @brief Resistance subtracts, weakness adds, immunity blocks; resistance
 * cannot heal. */
static void test_resistance_and_weakness() {
  Skeleton skeleton;
  Zombie zombie;
  CHECK_EQ(skeleton.take_damage(8, DamageType::Slashing), 3);
  CHECK_EQ(zombie.take_damage(8, DamageType::Slashing), 13);
  CHECK_EQ(skeleton.take_damage(3, DamageType::Slashing),
           0); // resistance can't heal
  CHECK_EQ(skeleton.take_damage(10, DamageType::Mental), 0); // immune
  CHECK_EQ(skeleton.take_damage(10, DamageType::Bludgeoning), 10);
  CHECK_EQ(zombie.take_damage(0, DamageType::Slashing),
           0); // weakness needs damage
}

/** @brief Only the best bonus and worst penalty of each type apply; different
 * types add. */
static void test_typed_bonus_stacking() {
  Dummy d;
  CHECK_EQ(d.armor_class(), 15);
  d.add_effect(
      {"Shield", BonusType::Circumstance, {{Stat::ArmorClass, 1}}, false, -1});
  CHECK_EQ(d.armor_class(), 16);
  // A second circumstance bonus of the same size does not stack.
  d.add_effect(
      {"Cover", BonusType::Circumstance, {{Stat::ArmorClass, 1}}, false, -1});
  CHECK_EQ(d.armor_class(), 16);
  // A larger one replaces the smaller.
  d.add_effect({"Greater cover",
                BonusType::Circumstance,
                {{Stat::ArmorClass, 2}},
                false,
                -1});
  CHECK_EQ(d.armor_class(), 17);
  // A different type does stack.
  d.add_effect(
      {"Armor rune", BonusType::Item, {{Stat::ArmorClass, 1}}, false, -1});
  CHECK_EQ(d.armor_class(), 18);
  // Penalties of the same type: only the worst applies.
  d.add_effect(
      {"Sickened", BonusType::Status, {{Stat::ArmorClass, -1}}, false, -1});
  d.add_effect(
      {"Worse", BonusType::Status, {{Stat::ArmorClass, -2}}, false, -1});
  CHECK_EQ(d.armor_class(), 16);
  d.remove_effect("Worse");
  CHECK_EQ(d.armor_class(), 17);
}

/** @brief Timed effects and conditions expire through `end_turn()`; condition
 * details work. */
static void test_timed_effects_and_conditions() {
  Dummy d;
  d.add_effect({"Brief", BonusType::Status, {{Stat::Attack, 1}}, false, 2});
  CHECK(d.has_effect("Brief"));
  d.end_turn();
  CHECK(d.has_effect("Brief"));
  d.end_turn();
  CHECK(!d.has_effect("Brief"));

  d.add_condition(Condition::Stunned, 1, "", 1);
  CHECK(d.has_condition(Condition::Stunned));
  CHECK_EQ(d.condition_value(Condition::Stunned), 1);
  d.end_turn();
  CHECK(!d.has_condition(Condition::Stunned));

  d.add_condition(Condition::Commanded, 1, "flee");
  CHECK_EQ(d.condition_detail(Condition::Commanded), std::string("flee"));
  d.remove_condition(Condition::Commanded);
  CHECK(!d.has_condition(Condition::Commanded));
}

/** @brief Same-type persistent damage does not stack; ticks hurt and end by
 * flat check. */
static void test_persistent_damage() {
  dice().seed(1);
  Dummy d("Dummy", 1000);
  d.add_persistent_damage(DamageType::Bleed, "1d4");
  d.add_persistent_damage(DamageType::Bleed, "1d4"); // same type: not stacked
  CHECK_EQ(d.persistent_damage().size(), static_cast<std::size_t>(1));
  d.add_persistent_damage(DamageType::Fire, "1d4");
  CHECK_EQ(d.persistent_damage().size(), static_cast<std::size_t>(2));
  CHECK(d.has_persistent_damage());

  // It always hurts at least once per tick, and ends eventually (flat check DC
  // 15).
  int ticks = 0;
  while (d.has_persistent_damage() && ticks < 500) {
    CHECK(d.tick_persistent_damage() >= 2);
    ++ticks;
  }
  CHECK(!d.has_persistent_damage());
  CHECK(ticks < 500);
}

/** @brief `Character` cannot be copied by value, preventing slicing. */
static void test_character_not_sliceable() {
  // Character copying is protected so a Monster cannot be sliced into a
  // Character.
  CHECK(!std::is_copy_constructible<Character>::value);
  CHECK(!std::is_copy_assignable<Character>::value);
}

// ------------------------------------------------------------------- library
/** @brief The registry holds 16 sorted spells with sane metadata; unknown ids
 * return null. */
static void test_library_basics() {
  CHECK(std::string(spellcasting_library_version()).size() > 0);
  const std::vector<std::string> ids = all_spell_ids();
  CHECK_EQ(ids.size(), static_cast<std::size_t>(16));
  std::vector<std::string> sorted = ids;
  std::sort(sorted.begin(), sorted.end());
  CHECK(sorted == ids);
  for (const auto &id : ids) {
    const Spell *s = find_spell(id);
    CHECK(s != nullptr);
    if (s) {
      CHECK_EQ(s->id(), id);
      CHECK(!s->name().empty());
      CHECK(!s->summary().empty());
      CHECK(s->min_actions() >= 1 && s->max_actions() >= s->min_actions());
      CHECK(s->rank() == 0 || s->rank() == 1);
    }
  }
  CHECK(find_spell("fireball") == nullptr);
  CHECK_EQ(find_spell(spell_id::kLight)->rank(), 0);
  CHECK_EQ(find_spell(spell_id::kHeal)->rank(), 1);
  CHECK(find_spell(spell_id::kForceBarrage)->resolution() ==
        Resolution::AutomaticDamage);
  CHECK(find_spell(spell_id::kDaze)->resolution() == Resolution::SavingThrow);
  CHECK(find_spell(spell_id::kHeal)->resolution() == Resolution::Healing);
  CHECK(find_spell(spell_id::kMysticArmor)->resolution() == Resolution::Buff);
}

/** @brief A bad spell id or wrong rank in a config throws
 * `std::invalid_argument`. */
static void test_config_validation() {
  SpellcasterConfig bad = kyra_config();
  bad.cantrips.push_back("fireball");
  CHECK_THROWS(make_spellcaster(bad));

  bad = kyra_config();
  bad.cantrips.push_back(spell_id::kHeal); // rank 1 listed as a cantrip
  CHECK_THROWS(make_spellcaster(bad));

  bad = kyra_config();
  bad.prepared.push_back(spell_id::kLight); // cantrip listed as prepared
  CHECK_THROWS(make_spellcaster(bad));

  {
    auto ok = make_spellcaster(kyra_config());
    CHECK(ok != nullptr);
  }
}

// ----------------------------------------------------------------- casting
/** @brief Missile count equals actions, damage matches HP lost, and slots run
 * out. */
static void test_force_barrage() {
  dice().seed(11);
  auto ezren = make_spellcaster(ezren_config());
  Dummy caster("Ezren", 20);
  Dummy target("Target", 500);

  // 1 action = 1 missile of 1d4+1.
  CastResult one = ezren->cast(spell_id::kForceBarrage, 1, caster, {&target});
  CHECK(one.ok);
  CHECK_EQ(one.outcomes.size(), static_cast<std::size_t>(1));
  CHECK(one.outcomes[0].damage_dealt >= 2 && one.outcomes[0].damage_dealt <= 5);
  CHECK(one.source == SlotSource::Prepared);

  // 3 actions = 3 missiles, all at one target.
  const int before = target.hp();
  CastResult three = ezren->cast(spell_id::kForceBarrage, 3, caster, {&target});
  CHECK(three.ok);
  CHECK_EQ(three.outcomes.size(), static_cast<std::size_t>(3));
  int total = 0;
  for (const auto &o : three.outcomes)
    total += o.damage_dealt;
  CHECK_EQ(before - target.hp(), total);
  CHECK(total >= 6 && total <= 15);

  // Both prepared slots are gone now.
  CHECK_EQ(ezren->slots_remaining(spell_id::kForceBarrage), 0);
  CastResult none = ezren->cast(spell_id::kForceBarrage, 1, caster, {&target});
  CHECK(!none.ok);
  CHECK(!none.error.empty());
}

/** @brief One target per missile works; a wrong target count changes nothing.
 */
static void test_force_barrage_split_targets() {
  dice().seed(12);
  auto ezren = make_spellcaster(ezren_config());
  Dummy caster("Ezren", 20), a("A", 500), b("B", 500), c("C", 500);
  CastResult r = ezren->cast(spell_id::kForceBarrage, 3, caster, {&a, &b, &c});
  CHECK(r.ok);
  CHECK_EQ(r.outcomes.size(), static_cast<std::size_t>(3));
  CHECK(a.hp() < 500 && b.hp() < 500 && c.hp() < 500);

  // Two targets for three missiles is not allowed, and changes nothing.
  const int slots = ezren->slots_remaining(spell_id::kForceBarrage);
  CastResult wrong = ezren->cast(spell_id::kForceBarrage, 3, caster, {&a, &b});
  CHECK(!wrong.ok);
  CHECK_EQ(ezren->slots_remaining(spell_id::kForceBarrage), slots);
}

/** @brief Spell damage goes through `take_damage` on a derived character. */
static void test_resistance_applies_to_spells() {
  dice().seed(13);
  auto ezren = make_spellcaster(ezren_config());
  Dummy caster("Ezren", 20);
  Skeleton skeleton;
  // Force is not resisted by the skeleton, but Telekinetic Projectile
  // (bludgeoning) and Force Bolt must still go through take_damage.
  CastResult r =
      ezren->cast(spell_id::kTelekineticProjectile, 2, caster, {&skeleton});
  CHECK(r.ok);
  CHECK(skeleton.hp() < 100);
}

/** @brief Cantrips can be cast any number of times. */
static void test_cantrips_unlimited() {
  dice().seed(14);
  auto kyra = make_spellcaster(kyra_config());
  Dummy caster("Kyra", 20);
  SaveBot target("Target", -50); // always fails
  for (int i = 0; i < 25; ++i) {
    CastResult r = kyra->cast(spell_id::kDaze, 2, caster, {&target});
    CHECK(r.ok);
  }
  CHECK_EQ(kyra->slots_remaining(spell_id::kDaze), -1);
}

/** @brief Healing is capped at max HP, dice ranges by actions, and font slots
 * are spent. */
static void test_heal() {
  dice().seed(15);
  auto kyra = make_spellcaster(kyra_config());
  Dummy caster("Kyra", 20);
  Dummy hurt("Hurt", 30);
  hurt.set_hp(29);

  // Healing never exceeds max HP.
  CastResult r = kyra->cast(spell_id::kHeal, 2, caster, {&hurt});
  CHECK(r.ok);
  CHECK_EQ(hurt.hp(), 30);
  CHECK_EQ(r.outcomes[0].healing_done, 1);
  CHECK(r.source == SlotSource::Font);
  CHECK_EQ(kyra->slots_remaining(spell_id::kHeal), 3);

  // 2 actions heals more than 1 action, on average. Check ranges directly.
  Dummy big("Big", 1000);
  big.set_hp(1);
  CastResult one = kyra->cast(spell_id::kHeal, 1, caster, {&big});
  CHECK(one.ok);
  CHECK(big.hp() - 1 >= 1 && big.hp() - 1 <= 8); // 1d8
  big.set_hp(1);
  CastResult two = kyra->cast(spell_id::kHeal, 2, caster, {&big});
  CHECK(two.ok);
  CHECK(big.hp() - 1 >= 9 && big.hp() - 1 <= 16); // 1d8+8
  CHECK_EQ(kyra->slots_remaining(spell_id::kHeal), 1);
}

/** @brief The 3-action form affects every listed target, living and undead. */
static void test_heal_three_actions_hits_everyone() {
  dice().seed(16);
  auto kyra = make_spellcaster(kyra_config());
  Dummy caster("Kyra", 20);
  Dummy ally("Ally", 1000);
  ally.set_hp(1);
  SaveBot undead("Undead", -50, true); // fails its save
  undead.set_hp(200);                  // stays in range for the damage check
  CastResult r = kyra->cast(spell_id::kHeal, 3, caster, {&ally, &undead});
  CHECK(r.ok);
  CHECK_EQ(r.outcomes.size(), static_cast<std::size_t>(2));
  CHECK(ally.hp() > 1);
  CHECK(undead.hp() < 200); // harmed, not healed
}

/** @brief Heal damages undead instead of healing them. */
static void test_heal_harms_undead_and_respects_immunity() {
  dice().seed(17);
  auto kyra = make_spellcaster(kyra_config());
  Dummy caster("Kyra", 20);
  Zombie zombie;
  CastResult r = kyra->cast(spell_id::kHeal, 2, caster, {&zombie});
  CHECK(r.ok);
  CHECK(zombie.hp() < 100);
  CHECK_EQ(r.outcomes[0].healing_done, 0);
}

/** @brief Orders are validated, applied on a failed save, and resisted on a
 * success; failed casts cost nothing. */
static void test_command() {
  dice().seed(18);
  auto kyra = make_spellcaster(kyra_config());
  Dummy caster("Kyra", 20);
  SaveBot weak("Weak", -50);
  SaveBot strong("Strong", 60);

  CastResult bad_option =
      kyra->cast(spell_id::kCommand, 2, caster, {&weak}, "dance");
  CHECK(!bad_option.ok);
  CastResult no_option = kyra->cast(spell_id::kCommand, 2, caster, {&weak});
  CHECK(!no_option.ok);
  CHECK_EQ(kyra->slots_remaining(spell_id::kCommand),
           2); // failed casts cost nothing

  CastResult r = kyra->cast(spell_id::kCommand, 2, caster, {&weak}, "flee");
  CHECK(r.ok);
  CHECK(weak.has_condition(Condition::Commanded));
  CHECK_EQ(weak.condition_detail(Condition::Commanded), std::string("flee"));
  CHECK_EQ(kyra->slots_remaining(spell_id::kCommand), 1);

  CastResult resisted =
      kyra->cast(spell_id::kCommand, 2, caster, {&strong}, "drop");
  CHECK(resisted.ok);
  CHECK(!strong.has_condition(Condition::Commanded));
  CHECK_EQ(kyra->slots_remaining(spell_id::kCommand), 0);
}

/** @brief Immune targets take nothing; critical failures stun; strong saves
 * reduce damage. */
static void test_daze() {
  dice().seed(19);
  auto kyra = make_spellcaster(kyra_config());
  Dummy caster("Kyra", 20);

  // Skeleton is immune to mental damage: no HP lost, no stun.
  Skeleton skeleton;
  CastResult r = kyra->cast(spell_id::kDaze, 2, caster, {&skeleton});
  CHECK(r.ok);
  CHECK_EQ(skeleton.hp(), 100);
  CHECK(!skeleton.has_condition(Condition::Stunned));

  // A character that always fails (critical failure) takes damage and is
  // stunned.
  SaveBot weak("Weak", -50);
  int stunned = 0;
  for (int i = 0; i < 10; ++i) {
    weak.set_hp(200);
    weak.remove_condition(Condition::Stunned);
    CastResult w = kyra->cast(spell_id::kDaze, 2, caster, {&weak});
    CHECK(w.ok);
    CHECK(weak.hp() < 200);
    if (weak.has_condition(Condition::Stunned))
      ++stunned;
  }
  CHECK(stunned > 0);

  // A character that always succeeds on its Will save takes nothing (or at most
  // half).
  SaveBot strong("Strong", 60);
  for (int i = 0; i < 10; ++i) {
    CastResult s = kyra->cast(spell_id::kDaze, 2, caster, {&strong});
    CHECK(s.ok);
  }
  CHECK(strong.hp() >=
        200 - 10 * 3); // success is half damage at most (1d6 -> max 3)
}

/** @brief Reported damage always equals the HP actually lost. */
static void test_basic_save_consistency() {
  // Over many casts, the damage recorded in the outcome must match what the
  // target actually lost, and failures must do at least as much as successes.
  dice().seed(20);
  auto ezren = make_spellcaster(ezren_config());
  Dummy caster("Ezren", 20);
  for (int i = 0; i < 100; ++i) {
    Dummy target("T", 1000);
    CastResult r =
        ezren->cast(spell_id::kTelekineticProjectile, 2, caster, {&target});
    CHECK(r.ok);
    CHECK_EQ(1000 - target.hp(), r.outcomes[0].damage_dealt);
  }
}

/** @brief One or two targets only; a third is rejected. */
static void test_breathe_fire() {
  dice().seed(21);
  SpellcasterConfig cfg = kyra_config();
  cfg.prepared = {spell_id::kBreatheFire};
  // Rank-1 spell prepared by a "cleric" is fine for the contract: the library
  // does not enforce class lists, only ranks.
  auto caster_book = make_spellcaster(cfg);
  Dummy caster("Caster", 20);
  SaveBot a("A", -50), b("B", -50), c("C", -50);

  CastResult too_many =
      caster_book->cast(spell_id::kBreatheFire, 2, caster, {&a, &b, &c});
  CHECK(!too_many.ok);
  CastResult r = caster_book->cast(spell_id::kBreatheFire, 2, caster, {&a, &b});
  CHECK(r.ok);
  CHECK(a.hp() < 200 && b.hp() < 200);
  CHECK_EQ(c.hp(), 200);
}

/** @brief Buffs change AC and weapon dice, and Runic Weapon expires after 10
 * rounds. */
static void test_mystic_armor_and_runic_weapon() {
  dice().seed(22);
  SpellcasterConfig cfg = ezren_config();
  cfg.prepared = {spell_id::kMysticArmor, spell_id::kRunicWeapon};
  auto ezren = make_spellcaster(cfg);
  Dummy caster("Ezren", 20);
  Dummy fighter("Fighter", 20);

  CHECK_EQ(caster.armor_class(), 15);
  CastResult armor = ezren->cast(spell_id::kMysticArmor, 2, caster, {});
  CHECK(armor.ok);
  CHECK_EQ(caster.armor_class(), 16);

  CHECK(!fighter.weapon_dice_doubled());
  CastResult rune = ezren->cast(spell_id::kRunicWeapon, 2, caster, {&fighter});
  CHECK(rune.ok);
  CHECK(fighter.weapon_dice_doubled());
  CHECK_EQ(fighter.modifier(Stat::Attack), 1);

  // Runic Weapon lasts 10 rounds.
  for (int i = 0; i < 10; ++i)
    fighter.end_turn();
  CHECK(!fighter.weapon_dice_doubled());
  CHECK_EQ(fighter.modifier(Stat::Attack), 0);
}

/** @brief A hit deals damage and starts persistent bleed. */
static void test_gouging_claw() {
  dice().seed(23);
  SpellcasterConfig cfg = ezren_config();
  cfg.cantrips = {spell_id::kGougingClaw};
  auto caster_book = make_spellcaster(cfg);
  Dummy caster("Caster", 20);
  SaveBot target("Target", -50);
  target.set_base_ac(-50); // an attack roll cannot miss
  CastResult r =
      caster_book->cast(spell_id::kGougingClaw, 2, caster, {&target});
  CHECK(r.ok);
  CHECK(target.hp() < 200);
  CHECK(target.has_persistent_damage());
}

/** @brief Only undead are valid targets for Vitality Lash. */
static void test_vitality_lash() {
  dice().seed(24);
  SpellcasterConfig cfg = kyra_config();
  cfg.cantrips = {spell_id::kVitalityLash};
  auto kyra = make_spellcaster(cfg);
  Dummy caster("Kyra", 20);
  Dummy living("Living", 100);
  SaveBot undead("Undead", -50, true);

  // Not undead: rejected, nothing spent.
  CastResult no = kyra->cast(spell_id::kVitalityLash, 2, caster, {&living});
  CHECK(!no.ok || living.hp() == 100);
  CHECK_EQ(living.hp(), 100);

  CastResult yes = kyra->cast(spell_id::kVitalityLash, 2, caster, {&undead});
  CHECK(yes.ok);
  CHECK(undead.hp() < 200);
}

/** @brief Light, Detect Magic and Stabilize cast successfully. */
static void test_utility_spells() {
  auto ezren = make_spellcaster(ezren_config());
  Dummy caster("Ezren", 20);
  CastResult light = ezren->cast(spell_id::kLight, 2, caster, {});
  CHECK(light.ok);
  CHECK(!light.text().empty());
  CastResult detect = ezren->cast(spell_id::kDetectMagic, 2, caster, {});
  CHECK(detect.ok);

  auto kyra = make_spellcaster(kyra_config());
  Dummy dying("Dying", 10);
  dying.set_hp(0);
  CastResult stab = kyra->cast(spell_id::kStabilize, 2, caster, {&dying});
  CHECK(stab.ok);
}

/** @brief Prepared slots are spent before font slots. */
static void test_slot_source_order() {
  dice().seed(25);
  // A caster with both a prepared Heal and font Heal slots spends prepared
  // first.
  SpellcasterConfig cfg = kyra_config();
  cfg.prepared = {spell_id::kHeal};
  auto kyra = make_spellcaster(cfg);
  Dummy caster("Kyra", 20), target("T", 100);
  CHECK_EQ(kyra->slots_remaining(spell_id::kHeal), 5);
  CHECK(kyra->cast(spell_id::kHeal, 2, caster, {&target}).source ==
        SlotSource::Prepared);
  CHECK(kyra->cast(spell_id::kHeal, 2, caster, {&target}).source ==
        SlotSource::Font);
  CHECK_EQ(kyra->slots_remaining(spell_id::kHeal), 3);
}

/** @brief Focus points and the arcane bond are each usable once and gated
 * correctly. */
static void test_focus_and_arcane_bond() {
  dice().seed(26);
  auto ezren = make_spellcaster(ezren_config());
  Dummy caster("Ezren", 20), target("Target", 500);

  CHECK_EQ(ezren->focus_points(), 1);
  CHECK(ezren->arcane_bond_available());
  CHECK_EQ(ezren->slots_remaining(spell_id::kForceBolt), 1);

  CastResult bolt = ezren->cast(spell_id::kForceBolt, 1, caster, {&target});
  CHECK(bolt.ok);
  CHECK(bolt.source == SlotSource::Focus);
  CHECK_EQ(ezren->focus_points(), 0);

  CastResult again = ezren->cast(spell_id::kForceBolt, 1, caster, {&target});
  CHECK(!again.ok);

  // Arcane Bond recasts a spell that was prepared today, once.
  ezren->cast(spell_id::kForceBarrage, 2, caster, {&target});
  ezren->cast(spell_id::kForceBarrage, 2, caster, {&target});
  CHECK_EQ(ezren->slots_remaining(spell_id::kForceBarrage), 0);
  CastResult bonded = ezren->cast_with_arcane_bond(spell_id::kForceBarrage, 2,
                                                   caster, {&target});
  CHECK(bonded.ok);
  CHECK(bonded.source == SlotSource::ArcaneBond);
  CHECK(!ezren->arcane_bond_available());
  CastResult twice = ezren->cast_with_arcane_bond(spell_id::kForceBarrage, 2,
                                                  caster, {&target});
  CHECK(!twice.ok);

  // Not prepared today: bond refused (and, since it failed, still unspent).
  auto ezren2 = make_spellcaster(ezren_config());
  CastResult unprepared =
      ezren2->cast_with_arcane_bond(spell_id::kHeal, 2, caster, {&target});
  CHECK(!unprepared.ok);
  CHECK(ezren2->arcane_bond_available());

  // A caster without a bond cannot use it.
  auto kyra = make_spellcaster(kyra_config());
  CastResult nobond = kyra->cast_with_arcane_bond(spell_id::kCommand, 2, caster,
                                                  {&target}, "drop");
  CHECK(!nobond.ok);
}

/** @brief Scrolls need no slot, and `rest()` restores everything. */
static void test_scroll_and_rest() {
  dice().seed(27);
  auto kyra = make_spellcaster(kyra_config());
  Dummy caster("Kyra", 20), target("T", 500);
  // Scrolls do not need a slot, even for a spell the caster never prepared.
  CastResult scroll =
      kyra->cast_from_scroll(spell_id::kForceBarrage, 2, caster, {&target});
  CHECK(scroll.ok);
  CHECK(scroll.source == SlotSource::Scroll);
  CHECK_EQ(kyra->slots_remaining(spell_id::kForceBarrage), 0);

  Dummy hurt("Hurt", 100);
  hurt.set_hp(1);
  for (int i = 0; i < 4; ++i)
    kyra->cast(spell_id::kHeal, 2, caster, {&hurt});
  CHECK_EQ(kyra->slots_remaining(spell_id::kHeal), 0);
  kyra->rest();
  CHECK_EQ(kyra->slots_remaining(spell_id::kHeal), 4);
  CHECK_EQ(kyra->slots_remaining(spell_id::kCommand), 2);
}

/** @brief Refused casts spend no slot, roll no dice, and harm nobody. */
static void test_failed_casts_change_nothing() {
  dice().seed(28);
  auto ezren = make_spellcaster(ezren_config());
  Dummy caster("Ezren", 20), target("T", 500);
  const std::uint64_t rolls_before = dice().rolls_made();
  const int slots = ezren->slots_remaining(spell_id::kForceBarrage);

  CHECK(!ezren->cast(spell_id::kForceBarrage, 0, caster, {&target})
             .ok); // 0 actions
  CHECK(!ezren->cast(spell_id::kForceBarrage, 4, caster, {&target})
             .ok);                                                // 4 actions
  CHECK(!ezren->cast(spell_id::kForceBarrage, 2, caster, {}).ok); // no target
  CHECK(!ezren->cast("fireball", 2, caster, {&target}).ok); // unknown spell
  CHECK(!ezren->cast(spell_id::kHeal, 2, caster, {&target}).ok); // not known

  CHECK_EQ(ezren->slots_remaining(spell_id::kForceBarrage), slots);
  CHECK_EQ(dice().rolls_made(), rolls_before); // nothing rolled
  CHECK_EQ(target.hp(), 500);
}

/** @brief `options()` and `can_cast()` report what is available. */
static void test_options_and_can_cast() {
  auto ezren = make_spellcaster(ezren_config());
  const std::vector<CastOption> opts = ezren->options();
  CHECK(!opts.empty());
  bool found_barrage = false, found_light = false;
  for (const auto &o : opts) {
    if (o.spell_id == spell_id::kForceBarrage) {
      found_barrage = true;
      CHECK_EQ(o.uses_left, 2);
    }
    if (o.spell_id == spell_id::kLight) {
      found_light = true;
      CHECK_EQ(o.uses_left, -1);
    }
  }
  CHECK(found_barrage);
  CHECK(found_light);
  CHECK(ezren->can_cast(spell_id::kForceBarrage, 2));
  CHECK(!ezren->can_cast(spell_id::kForceBarrage, 0));
  CHECK(!ezren->can_cast(spell_id::kHeal, 2));
  CHECK(!ezren->can_cast("fireball", 2));
}

/** @brief The same seed reproduces the same fight, text and all. */
static void test_determinism_through_spells() {
  // The same seed must give the same fight. This is what makes bugs
  // reproducible.
  auto play = [](std::uint64_t seed) {
    dice().seed(seed);
    auto ezren = make_spellcaster(ezren_config());
    Dummy caster("Ezren", 20), target("T", 500);
    std::string log;
    for (int i = 0; i < 3; ++i) {
      log += ezren->cast(spell_id::kForceBarrage, 3, caster, {&target}).text() +
             "\n";
      ezren->rest();
    }
    return log;
  };
  CHECK(play(777) == play(777));
  CHECK(play(777) != play(778));
}

/** @brief Run every test and return 0 only if all checks pass. */
int main() {
  std::cout << "Otari spellcasting contract tests (library "
            << spellcasting_library_version() << ")\n";
  run("dice: determinism", test_dice_determinism);
  run("dice: range", test_dice_range);
  run("dice: expressions", test_dice_expressions);
  run("dice: flat check and history", test_dice_flat_check_and_history);
  run("dice: global instance", test_global_dice_is_shared);
  run("character: hp", test_character_hp);
  run("character: resistance and weakness", test_resistance_and_weakness);
  run("character: typed bonus stacking", test_typed_bonus_stacking);
  run("character: timed effects and conditions",
      test_timed_effects_and_conditions);
  run("character: persistent damage", test_persistent_damage);
  run("character: not sliceable", test_character_not_sliceable);
  run("library: basics", test_library_basics);
  run("library: config validation", test_config_validation);
  run("spell: force barrage", test_force_barrage);
  run("spell: force barrage split targets", test_force_barrage_split_targets);
  run("spell: damage goes through take_damage",
      test_resistance_applies_to_spells);
  run("spell: cantrips unlimited", test_cantrips_unlimited);
  run("spell: heal", test_heal);
  run("spell: heal 3 actions", test_heal_three_actions_hits_everyone);
  run("spell: heal vs undead", test_heal_harms_undead_and_respects_immunity);
  run("spell: command", test_command);
  run("spell: daze", test_daze);
  run("spell: damage bookkeeping", test_basic_save_consistency);
  run("spell: breathe fire", test_breathe_fire);
  run("spell: mystic armor and runic weapon",
      test_mystic_armor_and_runic_weapon);
  run("spell: gouging claw", test_gouging_claw);
  run("spell: vitality lash", test_vitality_lash);
  run("spell: utility", test_utility_spells);
  run("caster: slot source order", test_slot_source_order);
  run("caster: focus and arcane bond", test_focus_and_arcane_bond);
  run("caster: scroll and rest", test_scroll_and_rest);
  run("caster: failed casts change nothing", test_failed_casts_change_nothing);
  run("caster: options and can_cast", test_options_and_can_cast);
  run("caster: determinism through spells", test_determinism_through_spells);

  std::cout << "\n" << g_checks << " checks, " << g_failures << " failures\n";
  return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
