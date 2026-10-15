/**
 * @file character.h
 * @brief Shared base class for every hero and monster.
 *
 * Header-only; nothing to link. Part 1 derives Player (and Fighter, Cleric,
 * Rogue, Wizard) from Character. Part 3 derives Monster (and the six monster
 * types) from Character. The spellcasting library only ever sees a Character,
 * so a spell can target a hero or a monster without knowing which.
 *
 * @par What Character does for you
 * - Hit points, armor class, Perception, and the three saves.
 * - Typed bonuses and penalties that follow the real stacking rule: only the
 *   best bonus and the worst penalty of each type apply.
 * - A small set of conditions (off-guard, stuck, stunned, sickened, commanded).
 * - Persistent damage, with the flat check to end it.
 * - A virtual take_damage() with resistance, weakness and immunity hooks.
 *
 * @par What it deliberately does not do
 * Attacks, skills, initiative, actions, inventory and spell slots. Those belong
 * to your parts.
 *
 * @ingroup otari_shared
 */
#ifndef CHARACTER_H
#define CHARACTER_H

#include <algorithm>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

#include "otari/dice.h"

namespace otari {
/**
 * @brief The kinds of damage in the game.
 *
 * Resistance, weakness, and immunity are all keyed on this type.
 * @ingroup otari_shared
 */
enum class DamageType {
  Bludgeoning,
  Piercing,
  Slashing,
  Acid,
  Cold,
  Electricity,
  Fire,
  Sonic,
  Force,
  Mental,
  Poison,
  Vitality,
  Bleed,
  Untyped
};

/** @brief Lowercase display name of a DamageType value, for game output.
 * @ingroup otari_shared */
inline const char *to_string(DamageType t) {
  switch (t) {
  case DamageType::Bludgeoning:
    return "bludgeoning";
  case DamageType::Piercing:
    return "piercing";
  case DamageType::Slashing:
    return "slashing";
  case DamageType::Acid:
    return "acid";
  case DamageType::Cold:
    return "cold";
  case DamageType::Electricity:
    return "electricity";
  case DamageType::Fire:
    return "fire";
  case DamageType::Sonic:
    return "sonic";
  case DamageType::Force:
    return "force";
  case DamageType::Mental:
    return "mental";
  case DamageType::Poison:
    return "poison";
  case DamageType::Vitality:
    return "vitality";
  case DamageType::Bleed:
    return "bleed";
  case DamageType::Untyped:
    return "untyped";
  }
  return "?";
}

/**
 * @brief The three saving throws.
 * @ingroup otari_shared
 */
enum class SaveType { Fortitude, Reflex, Will };

/** @brief Lowercase display name of a SaveType value, for game output. @ingroup
 * otari_shared */
inline const char *to_string(SaveType s) {
  switch (s) {
  case SaveType::Fortitude:
    return "Fortitude";
  case SaveType::Reflex:
    return "Reflex";
  case SaveType::Will:
    return "Will";
  }
  return "?";
}

/**
 * @brief The four degrees of success. Order matters: higher is better.
 *
 * Meeting the DC is a success, beating it by 10 is a critical success, missing
 * it by 10 is a critical failure. A natural 20 or 1 shifts the result one step.
 * @ingroup otari_shared
 */
enum class Degree {
  CriticalFailure = 0,
  Failure = 1,
  Success = 2,
  CriticalSuccess = 3
};

/** @brief Lowercase display name of a Degree value, for game output. @ingroup
 * otari_shared */
inline const char *to_string(Degree d) {
  switch (d) {
  case Degree::CriticalFailure:
    return "critical failure";
  case Degree::Failure:
    return "failure";
  case Degree::Success:
    return "success";
  case Degree::CriticalSuccess:
    return "critical success";
  }
  return "?";
}

/**
 * @brief Statistics that bonuses and penalties can change.
 * @see Character::modifier
 * @ingroup otari_shared
 */
enum class Stat {
  ArmorClass,
  Attack,
  Damage,
  Fortitude,
  Reflex,
  Will,
  Perception,
  Athletics,
  Skills, // Skills = every skill check except Athletics
  kCount  ///< Sentinel: the number of stats. Not a real statistic.
};

/**
 * @brief The type of a bonus or penalty.
 *
 * Within one type only the best bonus and the worst penalty count. Different
 * types add together.
 * @ingroup otari_shared
 */
enum class BonusType { Status, Circumstance, Item };

/**
 * @brief Conditions a character can have.
 * @see Character::add_condition
 * @ingroup otari_shared
 */
enum class Condition {
  OffGuard, ///< -2 circumstance penalty to AC.
  Stuck,    ///< Held in place (spider web); cleared by a successful Escape.
  Stunned,  ///< Value = actions lost; no reactions until its next turn.
  Sickened, ///< Value = status penalty to AC, attacks, saves, Perception,
            ///< skills.
  Commanded ///< Value = actions that must obey; detail = the order.
};

/** @brief Lowercase display name of a Condition value, for game output.
 * @ingroup otari_shared */
inline const char *to_string(Condition c) {
  switch (c) {
  case Condition::OffGuard:
    return "off-guard";
  case Condition::Stuck:
    return "stuck";
  case Condition::Stunned:
    return "stunned";
  case Condition::Sickened:
    return "sickened";
  case Condition::Commanded:
    return "commanded";
  }
  return "?";
}

/**
 * @brief A named bonus or penalty bundle, such as "Mystic Armor" (+1 item to
 * AC).
 * @see Character::add_effect
 * @ingroup otari_shared
 */
struct StatusEffect {
  std::string
      name; ///< Unique name; adding the same name replaces the old effect.
  BonusType type =
      BonusType::Status;    ///< Stacking type for every modifier in @ref mods.
  std::map<Stat, int> mods; ///< Stat to bonus (positive) or penalty (negative).
  bool doubles_weapon_dice =
      false;            ///< Runic Weapon: weapon damage uses two dice.
  int rounds_left = -1; ///< -1 lasts until rest; otherwise Character::end_turn
                        ///< counts it down.
};

/**
 * @brief The stored details of one active condition.
 * @ingroup otari_shared
 */
struct ConditionState {
  int value = 1;        ///< Strength of the condition (e.g. Stunned 2).
  std::string detail;   ///< Extra text, e.g. the order for Commanded.
  int rounds_left = -1; ///< -1 means until removed.
};

/**
 * @brief One source of persistent damage, such as a bleed or a burn.
 * @see Character::add_persistent_damage
 * @ingroup otari_shared
 */
struct PersistentDamage {
  DamageType type = DamageType::Untyped; ///< Damage type of each tick.
  std::string expression; ///< Rolled again each time it is taken, e.g. "1d4".
  bool helped = false;    ///< Flat check DC drops from 15 to 10.
};

// ------------------------------------------------------------------ Character

/**
 * @brief Base class for every hero and monster.
 *
 * Derive from it and override the virtual hooks (take_damage(), immune(),
 * resistance(), weakness(), is_undead(), describe()). Always hold characters by
 * pointer or reference, never by value: copying through a Character would slice
 * off the derived class, so the copy operations are protected.
 *
 * @code
 * class Skeleton : public otari::Character {
 * public:
 *     Skeleton() : Character("Skeleton", 4, 16, 2, 5, 3, 2) {}
 *     bool is_undead() const override { return true; }
 *     int resistance(otari::DamageType t) const override {
 *         return t == otari::DamageType::Slashing ? 5 : 0;
 *     }
 * };
 * @endcode
 *
 * @ingroup otari_shared
 */
class Character {
public:
  /**
   * @brief Construct a character at full health.
   * @param name       Display name.
   * @param max_hp     Maximum (and starting) hit points.
   * @param base_ac    Armor class before bonuses and penalties.
   * @param perception Perception modifier before bonuses and penalties.
   * @param fortitude  Fortitude save modifier before bonuses and penalties.
   * @param reflex     Reflex save modifier before bonuses and penalties.
   * @param will       Will save modifier before bonuses and penalties.
   */
  Character(std::string name, int max_hp, int base_ac, int perception,
            int fortitude, int reflex, int will)
      : m_name(std::move(name)), m_max_hp(max_hp), m_hp(max_hp),
        m_base_ac(base_ac), m_perception(perception), m_fortitude(fortitude),
        m_reflex(reflex), m_will(will) {}

  /** @brief Virtual destructor, so deleting through a Character pointer is
   * safe. */
  virtual ~Character() = default;

  /** @name Identity and hit points
   *  @{ */

  /** @brief The display name. */
  const std::string &name() const { return m_name; }
  /** @brief Current hit points (never below 0). */
  int hp() const { return m_hp; }
  /** @brief Maximum hit points. */
  int max_hp() const { return m_max_hp; }
  /** @brief `true` when hit points are 0. */
  bool is_unconscious() const { return m_hp <= 0; }

  /**
   * @brief Restore hit points, up to the maximum.
   *
   * Healing a character at 0 HP lifts it above 0, which ends unconscious
   * automatically.
   * @param amount Points to restore; zero or negative restores nothing.
   * @return The amount actually restored.
   */
  int heal(int amount) {
    if (amount <= 0)
      return 0;
    const int gained = std::min(amount, m_max_hp - m_hp);
    m_hp += gained;
    return gained;
  }

  /**
   * @brief Change the maximum HP (level-up, rest). Current HP is capped to the
   * new maximum.
   * @param value The new maximum.
   */
  void set_max_hp(int value) {
    m_max_hp = value;
    m_hp = std::min(m_hp, m_max_hp);
  }
  /**
   * @brief Set current HP directly, clamped to the range 0 to max HP.
   * @param value The new current HP.
   */
  void set_hp(int value) { m_hp = std::max(0, std::min(value, m_max_hp)); }
  /** @} */

  /** @name Defenses
   *  Active bonuses and penalties are already included in these values.
   *  @{ */

  /** @brief Armor class including all active modifiers. */
  int armor_class() const { return m_base_ac + modifier(Stat::ArmorClass); }
  /** @brief Perception modifier including all active modifiers. */
  int perception_modifier() const {
    return m_perception + modifier(Stat::Perception);
  }
  /**
   * @brief Saving throw modifier including all active modifiers.
   * @param type Which save.
   */
  int save_modifier(SaveType type) const {
    switch (type) {
    case SaveType::Fortitude:
      return m_fortitude + modifier(Stat::Fortitude);
    case SaveType::Reflex:
      return m_reflex + modifier(Stat::Reflex);
    case SaveType::Will:
      return m_will + modifier(Stat::Will);
    }
    return 0;
  }
  /** @brief Armor class before any bonuses or penalties. */
  int base_ac() const { return m_base_ac; }
  /** @brief Change the base armor class (e.g. when armor is equipped). */
  void set_base_ac(int value) { m_base_ac = value; }
  /** @} */

  /** @name Damage
   *  Override in Monster or Character as the project guide's rules require.
   *  @{ */

  /**
   * @brief Apply damage to this character.
   *
   * The default applies, in order: immunity, then resistance, then weakness.
   * A derived class may override either the three hooks below or this whole
   * function. Every spell and attack that hurts a character must call this so
   * the target's own rules apply.
   *
   * @param amount Damage before any adjustment.
   * @param type   The damage type.
   * @return The HP actually lost.
   */
  virtual int take_damage(int amount, DamageType type) {
    if (amount <= 0)
      return 0;
    if (immune(type))
      return 0;
    amount -= resistance(type);
    if (amount < 0)
      amount = 0;
    if (amount > 0)
      amount += weakness(type);
    const int lost = std::min(amount, m_hp);
    m_hp -= lost;
    return lost;
  }

  /** @brief Hook: `true` if this character takes no damage of the given type.
   * Default: never. */
  virtual bool immune(DamageType) const { return false; }
  /** @brief Hook: damage subtracted from each hit of the given type (never
   * below 0). Default: 0. */
  virtual int resistance(DamageType) const { return 0; }
  /** @brief Hook: extra damage added to each damaging hit of the given type.
   * Default: 0. */
  virtual int weakness(DamageType) const { return 0; }

  /**
   * @brief Hook: `true` for undead.
   *
   * The Heal spell damages undead instead of healing them, and Vitality Lash
   * only affects undead. Default: `false`.
   */
  virtual bool is_undead() const { return false; }
  /** @} */

  /**
   * @brief A one-line status string for the game's display.
   *
   * Shows name, HP, AC, unconscious state, conditions and effects. Override to
   * add more.
   */
  virtual std::string describe() const {
    std::ostringstream out;
    out << m_name << "  HP " << m_hp << "/" << m_max_hp << "  AC "
        << armor_class();
    if (is_unconscious())
      out << "  [unconscious]";
    for (const auto &entry : m_conditions) {
      out << "  [" << to_string(entry.first);
      if (entry.second.value > 1)
        out << " " << entry.second.value;
      out << "]";
    }
    for (const auto &effect : m_effects)
      out << "  [" << effect.name << "]";
    return out.str();
  }

  /** @name Bonuses and penalties
   *  @{ */

  /**
   * @brief Total of the active bonuses and penalties for a statistic.
   *
   * Within each BonusType only the best bonus and the worst penalty count.
   * Off-guard and sickened are included automatically.
   * @param stat The statistic.
   * @return The combined modifier (may be negative).
   */
  int modifier(Stat stat) const {
    int best[3] = {0, 0, 0};
    int worst[3] = {0, 0, 0};
    auto note = [&](BonusType type, int value) {
      const int i = static_cast<int>(type);
      if (value > 0)
        best[i] = std::max(best[i], value);
      else if (value < 0)
        worst[i] = std::min(worst[i], value);
    };
    for (const auto &effect : m_effects) {
      const auto it = effect.mods.find(stat);
      if (it != effect.mods.end())
        note(effect.type, it->second);
    }
    if (stat == Stat::ArmorClass && has_condition(Condition::OffGuard)) {
      note(BonusType::Circumstance, -2);
    }
    if (stat != Stat::Damage && has_condition(Condition::Sickened)) {
      note(BonusType::Status, -condition_value(Condition::Sickened));
    }
    return best[0] + best[1] + best[2] + worst[0] + worst[1] + worst[2];
  }

  /**
   * @brief Add a bonus or penalty bundle. Replaces any effect with the same
   * name.
   * @param effect The effect to add.
   */
  void add_effect(StatusEffect effect) {
    remove_effect(effect.name);
    m_effects.push_back(std::move(effect));
  }
  /** @brief `true` if an effect with this name is active. */
  bool has_effect(const std::string &name) const {
    return std::any_of(m_effects.begin(), m_effects.end(),
                       [&](const StatusEffect &e) { return e.name == name; });
  }
  /** @brief Remove the effect with this name, if present. */
  void remove_effect(const std::string &name) {
    m_effects.erase(
        std::remove_if(m_effects.begin(), m_effects.end(),
                       [&](const StatusEffect &e) { return e.name == name; }),
        m_effects.end());
  }
  /** @brief All active effects, for display. */
  const std::vector<StatusEffect> &effects() const { return m_effects; }

  /** @brief `true` while a Runic Weapon (or similar) is active: weapon damage
   * uses two dice. */
  bool weapon_dice_doubled() const {
    return std::any_of(
        m_effects.begin(), m_effects.end(),
        [](const StatusEffect &e) { return e.doubles_weapon_dice; });
  }

  /** @} */

  /** @name Conditions
   *  @{ */

  /**
   * @brief Apply a condition. If it is already present the larger value is
   * kept.
   * @param c      The condition.
   * @param value  Strength (e.g. 2 for Stunned 2).
   * @param detail Extra text, such as the order for Commanded.
   * @param rounds Turns until it expires, or -1 to last until removed.
   */
  void add_condition(Condition c, int value = 1, std::string detail = "",
                     int rounds = -1) {
    ConditionState &state = m_conditions[c];
    state.value = std::max(state.value, value);
    state.detail = std::move(detail);
    state.rounds_left = rounds;
  }
  /** @brief `true` if the condition is active. */
  bool has_condition(Condition c) const { return m_conditions.count(c) != 0; }
  /** @brief The condition's value, or 0 if it is not active. */
  int condition_value(Condition c) const {
    const auto it = m_conditions.find(c);
    return it == m_conditions.end() ? 0 : it->second.value;
  }
  /** @brief The condition's detail text, or an empty string if it is not
   * active. */
  std::string condition_detail(Condition c) const {
    const auto it = m_conditions.find(c);
    return it == m_conditions.end() ? std::string() : it->second.detail;
  }
  /** @brief Remove the condition, if present. */
  void remove_condition(Condition c) { m_conditions.erase(c); }
  /** @} */

  /** @name Persistent damage
   *  Gouging Claw's bleed, the cinder rat's fire.
   *  @{ */

  /**
   * @brief Start persistent damage.
   *
   * Only the larger of two instances of the same type is kept.
   * @param type       Damage type.
   * @param expression Dice expression rolled each tick, e.g. `"1d4"`.
   * @throws std::invalid_argument if @p expression is invalid.
   */
  void add_persistent_damage(DamageType type, const std::string &expression) {
    const int incoming_max = Dice::parse(expression).maximum();
    for (auto &existing : m_persistent) {
      if (existing.type == type) {
        // Only the larger of two same-type instances is kept.
        if (incoming_max > Dice::parse(existing.expression).maximum()) {
          existing.expression = expression;
        }
        return;
      }
    }
    m_persistent.push_back(PersistentDamage{type, expression, false});
  }
  /**
   * @brief Record that someone helped (patting out flames): the end-check DC
   * drops to 10.
   * @param type Which persistent damage was helped.
   */
  void assist_persistent_damage(DamageType type) {
    for (auto &p : m_persistent) {
      if (p.type == type)
        p.helped = true;
    }
  }
  /** @brief `true` if any persistent damage is active. */
  bool has_persistent_damage() const { return !m_persistent.empty(); }
  /** @brief All active persistent damage, for display. */
  const std::vector<PersistentDamage> &persistent_damage() const {
    return m_persistent;
  }

  /**
   * @brief Resolve persistent damage. Call at the end of this character's turn.
   *
   * Takes each persistent damage once (through take_damage()), then makes the
   * flat check (DC 15, or 10 if helped) to end it.
   * @return Total HP lost.
   */
  int tick_persistent_damage() {
    int total = 0;
    for (std::size_t i = 0; i < m_persistent.size();) {
      const PersistentDamage p = m_persistent[i];
      total += take_damage(dice().roll_expression(p.expression), p.type);
      if (dice().flat_check(p.helped ? 10 : 15)) {
        m_persistent.erase(m_persistent.begin() +
                           static_cast<std::ptrdiff_t>(i));
      } else {
        ++i;
      }
    }
    return total;
  }

  /** @} */

  /**
   * @brief Count down timed effects and conditions. Call at the end of this
   * character's turn.
   *
   * Anything whose `rounds_left` reaches 0 is removed.
   */
  void end_turn() {
    for (auto &effect : m_effects) {
      if (effect.rounds_left > 0)
        --effect.rounds_left;
    }
    m_effects.erase(std::remove_if(m_effects.begin(), m_effects.end(),
                                   [](const StatusEffect &e) {
                                     return e.rounds_left == 0;
                                   }),
                    m_effects.end());
    for (auto it = m_conditions.begin(); it != m_conditions.end();) {
      if (it->second.rounds_left > 0)
        --it->second.rounds_left;
      if (it->second.rounds_left == 0)
        it = m_conditions.erase(it);
      else
        ++it;
    }
  }

  /**
   * @brief Clear every effect that lasts "until you rest".
   *
   * The Level 1 engine has no rest; this exists for tests and for a later
   * iteration.
   */
  void clear_until_rest_effects() {
    m_effects.erase(
        std::remove_if(m_effects.begin(), m_effects.end(),
                       [](const StatusEffect &e) { return e.rounds_left < 0; }),
        m_effects.end());
  }

protected:
  /**
   * @name Copying
   * Derived classes can copy freely; copying through a Character value is
   * blocked to prevent slicing. Store characters by pointer or reference.
   * @{ */
  /** @brief Copy constructor, protected so a Character cannot be copied by
   * value. */
  Character(const Character &) = default;
  /** @brief Copy assignment, protected so a Character cannot be assigned by
   * value. */
  Character &operator=(const Character &) = default;
  /** @} */

private:
  std::string m_name;                  ///< Display name.
  int m_max_hp;                        ///< Maximum hit points.
  int m_hp;                            ///< Current hit points.
  int m_base_ac;                       ///< Armor class before modifiers.
  int m_perception;                    ///< Perception before modifiers.
  int m_fortitude;                     ///< Fortitude save before modifiers.
  int m_reflex;                        ///< Reflex save before modifiers.
  int m_will;                          ///< Will save before modifiers.
  std::vector<StatusEffect> m_effects; ///< Active bonus and penalty bundles.
  std::map<Condition, ConditionState> m_conditions; ///< Active conditions.
  std::vector<PersistentDamage> m_persistent; ///< Active persistent damage.
};

} // namespace otari
#endif
