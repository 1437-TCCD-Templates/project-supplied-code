/**
 * @file dice.h
 * @brief Seedable dice for the Menace Under Otari engine.
 *
 * Header-only. Every random number in the engine must come from here so that a
 * run can be replayed from its seed.
 *
 * @code
 * #include "otari/dice.h"
 * otari::dice().seed(12345);
 * int hit{otari::dice().d20()};
 * int dmg{otari::dice().roll_expression("1d8+4")};
 * @endcode
 *
 * @par Determinism
 * The engine is `std::mt19937_64`, whose output sequence is fixed by the C++
 * standard. The bounded roll does its own rejection sampling, so the same seed
 * gives the same rolls on every compiler and OS.
 * `std::uniform_int_distribution` is not portable, which is why it is not used.
 *
 * @ingroup otari_shared.
 */
#ifndef DICE_H
#define DICE_H

#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * @defgroup otari_shared Shared engine classes
 * @brief Headers every team uses: dice and the Character base class.
 */
namespace otari {

/**
 * @brief A parsed dice expression such as `"2d6+3"`.
 *
 * Holds @ref count dice of @ref sides sides plus a constant @ref bonus.
 * A plain number like `"4"` has `count == 0` and `bonus == 4`.
 *
 * @see Dice::parse
 * @ingroup otari_shared
 */
struct DiceExpression {
  int count{0}; ///< Number of dice (0 for a plain number)
  int sides{0}; ///< Sides per die (0 for a plain number)
  int bonus{0}; ///< Constant added to the total; may be negative

  /** @brief Lowest possible total (every die shows 1). */
  int minimum() const { return count + bonus; };
  /** @brief Highest possible total (every die shows its maximum). */
  int maximum() const { return count * sides + bonus; };
};

/**
 * @brief One entry in the optional roll history, useful for the debugging log.
 * @see Dice::set_history_enabled, Dice::history
 * @ingroup otari_shared
 */
struct RollRecord {
  std::string expression; ///< What was rolled, e.g., `"d20"` or `"2d6+3"`.
  std::vector<int> date;  ///< Each die as rolled, in order.
  int total{0};           ///< Sum of the dice plus any bonus
};

/**
 * @brief A seedable dice roller with a fixed, portable random sequence.
 *
 * Do not create your own `Dice` for gameplay rolls. Use the shared instance
 * returned by @ref otari::dice() so a single seed controls the whole game.
 *
 * @ingroup otari_shared
 */
class Dice {
public:
  /**
   * @brief Construct and seed the roller
   * @param initial_seed Starting seed (default 1)
   */
  explicit Dice(std::uint64_t initial_seed = 1) { seed(initial_seed); }

  /**
   * @brief Restart the random sequence.
   *
   * Also resets @ref rolls_made to zero and clears the roll history.
   * @param value The new seed. The same seed always produces the same rolls.
   */
  void seed(std::uint64_t value) {
    m_seed = value;
    m_engine.seed(value);
    m_rolls_made = 0;
    m_history.clear();
  }

  /** @brief The seed most recently passed to @ref seed. */
  std::uint64_t seed_value() const { return m_seed; }
  /** @brief Number of dice rolled since the last @ref seed call. */
  std::uint64_t rolls_made() const { return m_rolls_made; };

  /**
   * @brief Roll one die.
   * @param sides Number of sides (at least 1)
   * @return A value from 1 to @p sides
   * @throws std::invalid_argument if @p sides is less than 1
   */
  int roll(int sides) {
    const int value{raw_roll(sides)};
    record_if_enabled("d" + std::to_string(sides), {value}, value);
    return value;
  }

  /**
   * @brief Roll several identical dice and sum them.
   * @param count Number of dice (0 or more).
   * @param sides Sides per die (at least 1).
   * @return The sum of the dice.
   * @throws std::invalid_argument if @p count is negative or @p sides is less
   * than 1.
   */
  int roll(int count, int sides) {
    if (count < 0)
      throw std::invalid_argument("Dice::roll: negative count");
    std::vector<int> each;
    int total{0};
    for (int i{0}; i < count; ++i) {
      const int value{raw_roll(sides)};
      each.push_back(value);
      total += value;
    }
    record_if_enabled(std::to_string(count) + "d" + std::to_string(sides), each,
                      total);

    return total;
  }

  /** @brief Roll one twenty-side die. @return A value from 1 to 20. */
  int d20() { return roll(20); }

  /**
   * @brief A flat check: roll a bare d20 with no modifiers
   * @param dc Difficulty class.
   * @return `true` if the roll is @p dc or higher.
   */
  bool flat_check(int dc) { return d20() >= dc; }

  /**
   * @brief Parse and roll a dice expression
   *
   * Accepted forms: `"NdM"`, `"dM"`, `"NdM+K"`, `"NdM-K"`, or a plain integer.
   * Spaces are ignored.
   * @param text The expression, e.g., `"2d6+3"`.
   * @return The total.
   * @throws std::invalid_argument if @p text is not a valid expression.
   */
  int roll_expression(const std::string &text) {
    const DiceExpression expr = parse(text);
    std::vector<int> each;

    int total{expr.bonus};

    for (int i{0}; i < expr.count; ++i) {
      const int value{raw_roll(expr.sides)};
      each.push_back(value);
      total += value;
    }
    record_if_enabled(text, each, total);
    return total;
  }

  /**
   * @brief Parse a dice expression without rolling it.
   * @param text The expression; see @ref roll_expression for the accepted
   * forms.
   * @return The parsed pieces.
   * @throws std::invalid_argument on bad input
   */
  static DiceExpression parse(const std::string &text) {
    std::string s;
    for (char c : text) {
      if (c != ' ' && c != '\t')
        s.push_back(c);
    }

    if (s.empty())
      throw std::invalid_argument("empty dice expression");

    DiceExpression expr;
    std::size_t i{0};

    auto read_number{[&](bool &found) {
      int n{0};
      found = false;
      while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        n = n * 10 + (s[i] - '0');
        found = true;
        ++i;
      }
      return n;
    }};

    bool sign_negative{false};
    if (s[i] == '-') {
      sign_negative = true;
      ++i;
    } else if (s[i] == '+') {
      ++i;
    }

    bool found_first{false};

    const int first{read_number(found_first)};

    if (i < s.size() && (s[i] == 'd' || s[i] == 'D')) {
      if (sign_negative)
        throw std::invalid_argument("bad dice expression: " + text);
      ++i;
      bool found_sides{false};
      const int sides{read_number(found_sides)};

      if (!found_sides || sides < 1)
        throw std::invalid_argument("bad dice expression: " + text);

      expr.count = found_first ? first : 1;
      expr.sides = sides;

      if (i < s.size()) {
        const char op = s[i++];
        if (op != '+' && op != '-')
          throw std::invalid_argument("bad dice expression " + text);
        bool found_bonus{false};

        const int bonus{read_number(found_bonus)};

        if (!found_bonus)
          throw std::invalid_argument("bad dice expression: " + text);

        expr.bonus = (op == '-') ? -bonus : bonus;
      }
    } else {
      if (!found_first)
        throw std::invalid_argument("bad dice expression: " + text);
      expr.bonus = sign_negative ? -first : first;
    }
    if (i != s.size())
      throw std::invalid_argument("bad dice expression: " + text);
    return expr;
  }

  /** @brief Turn roll history on or off (off by default). Useful for a
   * debugging log. */
  void set_history_enabled(bool on) { m_history_enabled = on; }
  /** @brief Every roll recorded since history was enabled or last cleared. */
  const std::vector<RollRecord> &history() const { return m_history; }
  /** @brief Discard the recorded history. */
  void clear_history() { m_history.clear(); }

private:
  /**
   * @brief One unbiased die roll using rejection sampling.
   *
   * Values from the engine at or above the largest multiple of @p sides are
   * discarded so every face is equally likely.
   */
  int raw_roll(int sides) {
    if (sides < 1)
      throw std::invalid_argument("Dice: a die needs at least 1 side");
    const std::uint64_t s{static_cast<std::uint64_t>(sides)};
    const std::uint64_t max{std::numeric_limits<std::uint64_t>::max()};
    const std::uint64_t limit{(max / s) * s}; // reject the biased tail
    std::uint64_t r;

    do {
      r = m_engine();
    } while (r >= limit);
    ++m_rolls_made;
    return static_cast<int>(r % s) + 1;
  }

  /** @brief Append to the history when it is enabled */
  void record_if_enabled(const std::string &expression, std::vector<int> each,
                         int total) {
    if (!m_history_enabled)
      return;
    m_history.push_back(RollRecord{expression, std::move(each), total});
  }

  std::mt19937_64 m_engine;
  std::uint64_t m_seed{1};
  std::uint64_t m_rolls_made{0};
  bool m_history_enabled{false};
  std::vector<RollRecord> m_history;
};

/**
 * @brief The one shared dice roller for the whole program.
 *
 * An inline function with a function-local static gives a single instance
 * across every translation unit and the spellcasting library. Seed it once at
 * startup, and never create a second Dice for gameplay rolls.
 *
 * @return A reference to the shared Dice.
 * @ingroup otari_shared
 */
inline Dice &dice() {
  static Dice instance;
  return instance;
}

} // namespace otari
#endif
