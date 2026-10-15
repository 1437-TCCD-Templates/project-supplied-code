/**
 * @file spellcasting.h
 * @brief The Part 4 contract: spells and spell slots.
 *
 * This is the interface between spellcasting and the rest of the engine.
 * - Teams of 3 link the precompiled library (`libotari_spells`) and never see
 *   its source.
 * - The team of 4 writes its own Part 4 against this header: implement the
 *   Spell and SpellCaster interfaces and the three free functions at the bottom
 *   (make_spellcaster(), find_spell(), all_spell_ids()). Parts 2 and 3 are
 *   written identically either way.
 *
 * @par Scope
 * Level 1 iteration, no positioning: the 16 spells in @ref otari::spell_id.
 * There is no map, so "within 30 feet" and "in a 15-foot cone" are replaced by
 * target lists the caller chooses. See Spell::cast for each spell's target
 * rule.
 *
 * @par Rules every implementation follows
 * - Every roll goes through otari::dice().
 * - Every damage point goes through Character::take_damage(), so the target's
 *   resistances and weaknesses apply.
 * - A failed cast (`ok == false`) changes nothing: no slot is spent and no
 *   dice are rolled that affect the game.
 *
 * @ingroup otari_spells_api
 */
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "otari/character.h"
#include "otari/dice.h"

/**
 * @defgroup otari_spells_api Spellcasting contract
 * @brief The Part 4 interface: Spell, SpellCaster and the free functions.
 */

namespace otari {

/**
 * @brief String ids of the 16 Level 1 spells.
 *
 * Pass these to SpellcasterConfig, SpellCaster::cast() and find_spell().
 * @ingroup otari_spells_api
 */
namespace spell_id {
inline constexpr const char *kBreatheFire =
    "breathe_fire"; ///< Breathe Fire: 2d6 fire, basic Reflex save, 1 or 2
                    ///< targets.
inline constexpr const char *kCommand =
    "command"; ///< Command: a one-word order, Will save.
inline constexpr const char *kDaze =
    "daze"; ///< Daze: 1d6 mental, basic Will save; a critical failure stuns.
inline constexpr const char *kDetectMagic =
    "detect_magic"; ///< Detect Magic: utility.
inline constexpr const char *kFigment =
    "figment"; ///< Figment: utility illusion.
inline constexpr const char *kForceBarrage =
    "force_barrage"; ///< Force Barrage: 1 to 3 auto-hit force missiles.
inline constexpr const char *kForceBolt =
    "force_bolt"; ///< Force Bolt: Ezren's focus spell, auto-hit force damage.
inline constexpr const char *kGougingClaw =
    "gouging_claw"; ///< Gouging Claw: spell attack, slashing plus bleed.
inline constexpr const char *kHeal =
    "heal"; ///< Heal: heals the living, harms undead.
inline constexpr const char *kLight = "light";     ///< Light: utility.
inline constexpr const char *kMessage = "message"; ///< Message: utility.
inline constexpr const char *kMysticArmor =
    "mystic_armor"; ///< Mystic Armor: +1 item bonus to AC, self only.
inline constexpr const char *kRunicWeapon =
    "runic_weapon"; ///< Runic Weapon: +1 attack and doubled weapon dice for 10
                    ///< rounds.
inline constexpr const char *kStabilize =
    "stabilize"; ///< Stabilize: utility first aid.
inline constexpr const char *kTelekineticProjectile =
    "telekinetic_projectile"; ///< Telekinetic Projectile: 2d6 bludgeoning.
inline constexpr const char *kVitalityLash =
    "vitality_lash"; ///< Vitality Lash: 2d6 vitality to one undead character.
} // namespace spell_id

/**
 * @brief How a spell is resolved. Mirrors the six Part 4 derived classes.
 * @ingroup otari_spells_api
 */
enum class Resolution {
  AutomaticDamage, ///< No roll; the damage simply happens (Force Barrage).
  SavingThrow,     ///< The target rolls a save against the spell DC.
  AttackRoll,      ///< The caster rolls a spell attack against AC.
  Healing,         ///< Restores HP (and harms undead).
  Buff,            ///< Adds a bonus to a character.
  Utility          ///< Everything else: light, detection, messages, first aid.
};

/**
 * @brief Where the power for a cast came from.
 * @ingroup otari_spells_api
 */
enum class SlotSource {
  Cantrip,  ///< Unlimited.
  Prepared, ///< One of the caster's prepared 1st-rank slots.
  Font,     ///< Kyra's Divine Font heal slots.
  Focus,    ///< Ezren's school spell (Force Bolt), costs a focus point.
  Scroll,   ///< SpellCaster::cast_from_scroll: no slot; the caller destroys the
            ///< scroll item.
  ArcaneBond ///< Ezren's once-per-day recast.
};

/**
 * @brief What the caller supplies for one cast.
 *
 * SpellCaster::cast() builds this for you. You only fill one in yourself if you
 * call Spell::cast() directly (for example in a test).
 * @ingroup otari_spells_api
 */
struct CastContext {
  Character *caster = nullptr; ///< Who is casting.
  std::vector<Character *>
      targets;          ///< Targets; see each spell's rule in Spell::cast.
  int actions = 2;      ///< How many actions the caster spends.
  int spell_attack = 0; ///< The caster's spell attack modifier (e.g. +7).
  int spell_dc = 10;    ///< The caster's spell DC (e.g. 17).
  std::string option;   ///< Spell-specific choice, e.g. Command's order.
};

/**
 * @brief What happened to one target (or one missile, for Force Barrage).
 * @ingroup otari_spells_api
 */
struct TargetOutcome {
  Character *target = nullptr; ///< The character affected.
  std::string target_name;     ///< Its name, for display.
  bool has_degree = false;     ///< `false` for automatic effects (no roll).
  Degree degree =
      Degree::Success;   ///< Degree of success, when @ref has_degree is true.
  int natural_roll = 0;  ///< The d20, when a roll was made.
  int roll_total = 0;    ///< d20 plus modifier, when a roll was made.
  int damage_rolled = 0; ///< Damage before the save and before resistance.
  int damage_dealt = 0;  ///< HP actually lost.
  int healing_done = 0;  ///< HP actually restored.
  std::string note; ///< e.g. "immune", "stunned 1", "no effect: not undead".
};

/**
 * @brief The result of one cast attempt.
 *
 * Check @ref ok first. When it is `false`, nothing happened and @ref error says
 * why. When it is `true`, @ref lines holds ready-to-print narration and
 * @ref outcomes holds the per-target details.
 * @ingroup otari_spells_api
 */
struct CastResult {
  bool ok = false;      ///< `false`: nothing happened (see @ref error).
  std::string error;    ///< Why the cast was refused, when @ref ok is false.
  std::string spell_id; ///< Which spell was cast.
  int actions = 0;      ///< Actions spent.
  SlotSource source = SlotSource::Cantrip; ///< What paid for the cast.
  std::vector<TargetOutcome>
      outcomes; ///< One entry per target (or per missile).
  std::vector<std::string>
      lines; ///< Ready-to-print narration, one entry per line.

  /**
   * @brief Everything a game display needs, joined with newlines.
   * @return The narration, or `"Cannot cast: <error>"` when @ref ok is false.
   */
  std::string text() const {
    std::string out;
    if (!ok)
      return "Cannot cast: " + error;
    for (std::size_t i = 0; i < lines.size(); ++i) {
      if (i)
        out += "\n";
      out += lines[i];
    }
    return out;
  }
};

/**
 * @brief One spell.
 *
 * The library implements this as a base class with six derived classes (one per
 * Resolution). You only ever hold a `const Spell*` obtained from find_spell().
 * @ingroup otari_spells_api
 */
class Spell {
public:
  virtual ~Spell() = default;

  /** @brief The spell's id, one of the @ref otari::spell_id constants. */
  virtual const std::string &id() const = 0;
  /** @brief Display name, e.g. "Force Barrage". */
  virtual const std::string &name() const = 0;
  /** @brief 0 for a cantrip, 1 for a 1st-rank spell. */
  virtual int rank() const = 0;
  /** @brief Fewest actions the spell can be cast with. */
  virtual int min_actions() const = 0;
  /** @brief Most actions the spell can be cast with. */
  virtual int max_actions() const = 0;
  /** @brief How the spell is resolved. */
  virtual Resolution resolution() const = 0;
  /** @brief Short rules text for menus. */
  virtual std::string summary() const = 0;

  /**
   * @brief Resolve the spell.
   *
   * Validates first and returns `ok == false`, changing nothing, if the
   * actions, targets, or option are wrong.
   *
   * @par Target rules
   * | Spell | Targets |
   * |---|---|
   * | force_barrage | 1 target (all missiles), or one target per missile (equal
   * to actions) | | force_bolt, daze, command, gouging_claw,
   * telekinetic_projectile, vitality_lash, stabilize | exactly 1 | |
   * breathe_fire | 1 or 2 (the caster chooses; the engine has no cone) | | heal
   * | 1 at 1 or 2 actions; 1 or more at 3 actions (the 3-action form affects
   * everyone listed, living and undead) | | mystic_armor | none (affects the
   * caster) | | runic_weapon | exactly 1 (the weapon's wielder) | | message | 0
   * or 1 | | detect_magic, light, figment | none |
   *
   * @param context Caster, targets, actions and numbers for this cast.
   * @return What happened. Check CastResult::ok first.
   */
  virtual CastResult cast(const CastContext &context) const = 0;
};

/**
 * @brief What a caster has prepared. All spell ids must exist.
 * @see make_spellcaster
 * @ingroup otari_spells_api
 */
struct SpellcasterConfig {
  std::string owner;                 ///< For log lines, e.g. "Kyra".
  int spell_attack = 0;              ///< +7 for both casters at Level 1.
  int spell_dc = 10;                 ///< 17 for both casters at Level 1.
  std::vector<std::string> cantrips; ///< Rank 0, unlimited casts.
  std::vector<std::string>
      prepared; ///< Rank 1; one entry per slot, duplicates allowed.
  std::map<std::string, int>
      font_slots; ///< Extra 1st-rank slots, e.g. `{"heal", 4}`.
  std::vector<std::string>
      school_spells;        ///< Cast with focus points, e.g. `{"force_bolt"}`.
  int focus_points = 0;     ///< 1 for Ezren.
  bool arcane_bond = false; ///< `true` for Ezren.
};

/**
 * @brief One line in a cast menu.
 * @see SpellCaster::options
 * @ingroup otari_spells_api
 */
struct CastOption {
  std::string spell_id; ///< Which spell.
  int min_actions = 0;  ///< Fewest actions it can be cast with.
  int max_actions = 0;  ///< Most actions it can be cast with.
  SlotSource source = SlotSource::Cantrip; ///< What would pay for the cast.
  int uses_left = -1; ///< Casts remaining; -1 means unlimited.
};

/**
 * @brief Tracks slots for one caster and performs casts.
 *
 * Hold one per caster, by `std::unique_ptr` (see make_spellcaster()).
 * @ingroup otari_spells_api
 */
class SpellCaster {
public:
  virtual ~SpellCaster() = default;

  /** @brief The configuration this caster was built from. */
  virtual const SpellcasterConfig &config() const = 0;

  /** @brief Everything the caster can cast right now, for building a menu. */
  virtual std::vector<CastOption> options() const = 0;

  /**
   * @brief Cheap check that does not roll or change anything.
   * @param spell_id Which spell.
   * @param actions  Actions the caster would spend.
   * @return `true` if a slot or cantrip is available and the action count is
   * legal.
   * @note Targets are not checked.
   */
  virtual bool can_cast(const std::string &spell_id, int actions) const = 0;

  /**
   * @brief Uses left for a spell.
   *
   * Counts unused prepared slots plus font slots (plus focus points for a
   * school spell).
   * @param spell_id Which spell.
   * @return The count; -1 for cantrips (unlimited); 0 if unknown or
   * unavailable.
   */
  virtual int slots_remaining(const std::string &spell_id) const = 0;
  /** @brief Focus points left. */
  virtual int focus_points() const = 0;
  /** @brief `true` if the caster has an Arcane Bond that has not been used
   * today. */
  virtual bool arcane_bond_available() const = 0;

  /**
   * @brief Cast using the normal slot rules.
   *
   * Slots are spent only if the cast succeeds. When several sources could
   * pay, the preference is prepared, then font, then focus.
   *
   * @param spell_id Which spell.
   * @param actions  Actions to spend.
   * @param caster   The character casting.
   * @param targets  Targets; see Spell::cast for each spell's rule.
   * @param option   Spell-specific choice, e.g. Command's order.
   * @return What happened. Check CastResult::ok first.
   */
  virtual CastResult cast(const std::string &spell_id, int actions,
                          Character &caster,
                          const std::vector<Character *> &targets,
                          const std::string &option = "") = 0;

  /**
   * @brief Cast from a scroll: no slot is needed or spent.
   * @note The caller removes the scroll from the inventory when `ok == true`.
   * @param spell_id,actions,caster,targets,option As for cast().
   */
  virtual CastResult cast_from_scroll(const std::string &spell_id, int actions,
                                      Character &caster,
                                      const std::vector<Character *> &targets,
                                      const std::string &option = "") = 0;

  /**
   * @brief Arcane Bond: once per day, recast a spell that was prepared today
   * even if its slot is spent.
   *
   * No slot is used. Fails if the caster has no bond, it was already used, or
   * the spell was not prepared.
   * @param spell_id,actions,caster,targets,option As for cast().
   */
  virtual CastResult
  cast_with_arcane_bond(const std::string &spell_id, int actions,
                        Character &caster,
                        const std::vector<Character *> &targets,
                        const std::string &option = "") = 0;

  /**
   * @brief Restore every slot, focus point, and the bond.
   * @note The Level 1 engine has no rest; this is for tests and for a later
   * iteration.
   */
  virtual void rest() = 0;
};

/**
 * @brief Build a SpellCaster.
 * @param config What the caster has prepared.
 * @return A new caster that owns its own slot bookkeeping.
 * @throws std::invalid_argument if a spell id is unknown, a cantrip is not
 *         rank 0, or a prepared, font or school spell is not rank 1.
 * @ingroup otari_spells_api
 */
std::unique_ptr<SpellCaster> make_spellcaster(const SpellcasterConfig &config);

/**
 * @brief Look up a spell by id.
 * @param spell_id One of the @ref otari::spell_id constants.
 * @return The spell, or `nullptr` if the id is unknown. The pointer stays valid
 *         for the life of the program.
 * @ingroup otari_spells_api
 */
const Spell *find_spell(const std::string &spell_id);

/**
 * @brief All 16 spell ids, in alphabetical order.
 * @ingroup otari_spells_api
 */
std::vector<std::string> all_spell_ids();

/**
 * @brief The library version string, e.g. `"1.0.0"`.
 * @ingroup otari_spells_api
 */
const char *spellcasting_library_version();

} // namespace otari
