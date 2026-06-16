# Deck Shuffle — Uniformity Analysis

**What this tests:** whether the provably-fair shuffle (`SHA-256(seed) → factoradic/
Lehmer decode`, see `entropy/deck.c`) produces a *uniform* 52-card permutation,
with no bias in the cards a BlackJack hand actually consumes.

**How:** run the program headless with `lcom_run proj analyze` (see
`analysis/analysis.c`). It generates decks continuously from live entropy and,
for the **first 12 cards of each deck**, accumulates:

- rank frequency (each rank should converge to 1/13 = 7.69%),
- per-position high-card rate (T/J/Q/K, 16/52 = 30.77%, should be flat across
  positions — this is where an early-deal bias would show),
- adjacency: P(next card high | current high/low).

The harness samples the same live entropy the game feeds the pool — timer,
keyboard, and RTC (mouse aside, since it runs headless). Shuffle uniformity is a
property of the decode and is independent of the source mix, so these figures
characterize the `deck.c` decode regardless of which devices contribute.

> Note: counting all 52 positions is meaningless — every deck contains each rank
> exactly 4 times, so the full-deck histogram is uniform *by construction*. Bias
> can only appear **per position**, which is what this samples.

## Result (1,012,720 decks ≈ 12.15M dealt cards)

```
rank freq over dealt cards (expect 7.69% each):
  A     934562  7.69%   2 935228  7.70%   3 935892  7.70%   4 935449  7.70%
  5     934990  7.69%   6 933624  7.68%   7 936136  7.70%   8 933433  7.68%
  9     934916  7.69%   T 934387  7.69%   J 934115  7.69%   Q 936438  7.71%
  K     933470  7.68%

high-card (T/J/Q/K) rate per position (expect 30.77%):
  pos 0: 30.77   pos 3: 30.76   pos 6: 30.78   pos 9:  30.77
  pos 1: 30.67   pos 4: 30.80   pos 7: 30.82   pos 10: 30.75
  pos 2: 30.72   pos 5: 30.73   pos 8: 30.72   pos 11: 30.85

dealt-region correlation:
  P(high)                 = 30.76%
  P(next high | cur high) = 29.40%
  P(next high | cur low)  = 31.36%
```

## Interpretation

1. **Rank frequency is flat.** All ranks land in 7.68–7.71% (expected 7.69%).
   Observed counts vary by ~±1.7σ at most (σ ≈ 929 on ~934,818 expected): the
   high is Q at +1620 (1.74σ), the low is 8 at −1385 (1.49σ). No rank is favored.

2. **No positional bias.** Every position's high-card rate sits within ~2σ of
   30.77% (σ ≈ 0.046%). In particular `pos 0`/`pos 1` — the first cards dealt —
   are **not** elevated (30.77% / 30.67%). There is no early-deal skew toward
   high cards.

3. **The adjacency "correlation" is the without-replacement signature, not bias.**
   A high card is followed by a high card *less* often (29.40%) than a low card is
   (31.36%) — a small **negative** correlation. This is exactly the finite-deck
   math for a uniform shuffle:
   - after a high card: 15 highs of 51 left → **15/51 = 29.41%** (observed 29.40%)
   - after a low card:  16 highs of 51 left → **16/51 = 31.37%** (observed 31.36%)

   Both match theory to within ~1σ. A correct uniform shuffle is *required* to
   produce these numbers.

## Conclusion

Over ~1.01M decks (12.15M cards) the shuffle is statistically indistinguishable
from a true uniform permutation: flat rank distribution, no positional bias, and
adjacency correlations that match the exact without-replacement baseline. The
entropy aggregator + Lehmer decode produces fair deals.
