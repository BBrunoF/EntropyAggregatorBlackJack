# EntropyAggregatorBlackJack

A provably-fair BlackJack game for MINIX 3, built from scratch in C11.

The central design challenge was making card shuffles **verifiably random** — not just unpredictable, but mathematically provable to a player after the fact. To solve this, the game continuously harvests hardware entropy from real I/O events (timer jitter, keystrokes, mouse movement, RTC reads), folds them into a live SHA-256 pool, and uses a commit/reveal protocol so neither the game nor any observer can bias the outcome once a hand starts.

> **Authorship note:** The entropy system (`entropy/`) — pool design, commit/reveal protocol, Lehmer decoder, SHA-256 implementation, and statistical validation — is my work. The rest of the codebase (game logic, video engine, drivers) was done by teammates.

---

## The Entropy System

This is the piece I'm most proud of. The shuffle is provably fair by construction, not by trust.

### How it works — end to end

```
Hardware events → SHA-256 pool → commit hash → player input → reveal seed → Lehmer decode → deck
```

**1. Continuous entropy accumulation** (`entropy/pool.c`, `entropy/entropy.c`)

Every hardware interrupt contributes to a running SHA-256 context — the *pool*. Each contribution mixes in:
- a source tag (`TIMER`, `KBD`, `MOUSE`, `RTC`)
- the current PIT counter value (16-bit hardware timer, read live) — this is the timing jitter
- the raw event byte (scancode, mouse packet byte, RTC register value)

The PIT counter is the key: it samples the i8254 timer at the exact cycle the interrupt fires, capturing sub-millisecond jitter that is unpredictable even to the OS. The pool never resets; it accumulates state across the entire session.

**2. Commit/reveal protocol** (`entropy/entropy.c`)

Before a hand is dealt, `entropy_commit()` extracts the current pool state into a 32-byte seed and immediately outputs `SHA256(seed)` as a *commitment*. This commitment is shown to the player (or logged). SHA-256 preimage resistance means nobody — including the game — can work backwards from the commitment to know the deck order.

After the player makes their first bet/action (adding more entropy), `entropy_shuffle_committed()` decodes the *already-fixed* seed into the deck. The seed cannot change after commitment, so the shuffle cannot be influenced by the game. After the hand, `entropy_reveal()` exposes the seed: anyone can verify that `SHA256(seed) == commitment` and replay `deck_shuffle_from_digest(seed)` to confirm they got exactly the hand they were dealt.

**3. Factoradic (Lehmer) deck decode** (`entropy/deck.c`)

Given a 32-byte SHA-256 digest, the deck is derived by treating it as a 256-bit big integer and performing successive divmod against 1, 2, …, 52 — producing the *factoradic digits* (Lehmer code) of a permutation. These digits are then decoded into a card ordering by repeatedly selecting and removing cards from an ordered set.

This covers all 52! ≈ 2²²⁶ permutations with no rejection sampling and no modular bias (256 bits >> 226 bits needed), and it is deterministic: same digest → same deck, always.

```c
// Simplified sketch from deck.c
for (int i = 1; i <= 52; i++)
    lehmer[i - 1] = bignum_divmod_small(N, i);   // N % i, then N /= i

for (int k = 51; k >= 0; k--) {
    out[51 - k] = remaining[lehmer[k]];           // pick & remove
}
```

### Statistical validation

I ran the shuffle over **1,012,720 generated decks** (~12.15 million dealt cards) to verify there is no bias. Results (see `analysis/RESULTS.md`):

| Check | Expected | Observed | Verdict |
|---|---|---|---|
| Rank frequency (each rank) | 7.69% | 7.68 – 7.71% | Flat, ≤ 1.7σ |
| High-card rate per position | 30.77% | 30.67 – 30.85% | No positional skew |
| P(next high \| cur high) | 29.41% | 29.40% | Matches without-replacement math |
| P(next high \| cur low) | 31.37% | 31.36% | Matches without-replacement math |

The adjacency correlation is *not* a bug — it is the exact statistical signature of a correct uniform shuffle of a finite deck (15/51 = 29.41%, 16/51 = 31.37%).

To reproduce:
```sh
lcom_run proj analyze
```

---

## The Game

**BlackJack with two modes:**

- **Normal** — standard rules: hit, stand, double down, chip betting.
- **Purgatory** — a horror-themed variant. The dealer's arm is replaced by a demon claw that drops chips; there is a financial-risk mechanic where losing hands escalate consequences.

The full sprite engine, button system, and render loop are in `video/`. The game state machine and rule logic live in `game/`.

---

## Environment / Prerequisites

> **This runs on MINIX 3 only**, in the FEUP LCOM virtual machine.  
> It will not compile or run on Linux, macOS, or Windows.

| Requirement | Detail |
|---|---|
| OS | MINIX 3 (FEUP LCOM VM image) |
| Compiler | Clang 3.6 (bundled with MINIX image) |
| Build system | `bmake` + `minix.lcom.mk` |
| Framework | LCOM Control Framework (`liblcf`, `lcom/lcf.h`) |
| Display | VBE mode `0x105` (1024×768, 32 bpp) |

The LCOM framework and MINIX VM image are distributed by FEUP and are not included here.

---

## Build

Inside the MINIX VM, with the repo mounted at `~/shared`:

```sh
cd ~/shared/EntropyAggregatorBlackJack
make
```

> **Compiler note:** Clang 3.6 on MINIX segfaults at `-O2` on several files (a known compiler bug). The Makefile forces `-O0`. Do not override this flag.

> **VirtualBox shared folder note:** `make clean` fails with `EINVAL` on vboxsf. To clean, delete `*.o`, `*.d`, and `proj` from the **host** side, then rebuild with `make`.

---

## Run

```sh
# Start the game
lcom_run proj

# Run entropy uniformity analysis (headless)
lcom_run proj analyze

# Run SHA-256 unit tests
make test_sha256
./test_sha256
```

---

## Project Structure

```
EntropyAggregatorBlackJack/
├── main.c              # Entry point; interrupt loop, entropy feed, frame dispatch
├── drivers/            # timer, KBC (keyboard), mouse, RTC device drivers
├── entropy/
│   ├── pool.c/h        # Running SHA-256 entropy pool
│   ├── entropy.c/h     # Feed functions, commit/reveal API
│   ├── deck.c/h        # Factoradic (Lehmer) shuffle decoder
│   └── sha256.c/h      # SHA-256 implementation (no external deps)
├── game/               # BlackJack rules, state machine, Normal/Purgatory modes
├── video/              # Sprite engine, text renderer, button UI, render loop
├── analysis/           # Headless shuffle uniformity validator
│   └── RESULTS.md      # Statistical results (1M+ decks)
├── tests/              # SHA-256 unit tests
└── report/             # Academic report (PDF) and gameplay video
```

---

## Academic Context

| Field | Value |
|---|---|
| Course | LCOM — Laboratory of Computers |
| Institution | FEUP — Faculdade de Engenharia da Universidade do Porto |
| Year | 2024/2025 |
| Author | Bruno Freitas |

---

## Known Limitations

- **MINIX 3 only.** Tightly coupled to MINIX IPC (`sys_outb`, `driver_receive`, `sef_startup`), the LCOM framework, and i386 I/O port hardware. Not portable.
- **Clang 3.6 / `-O0` only.** Higher optimisation levels trigger a compiler segfault.
- **VBE 0x105 only.** The video engine targets 1024×768 32 bpp; other resolutions are unsupported.
- **No network / multi-player.** Single-player only; the commit/reveal protocol is designed for local auditability, not online play.
