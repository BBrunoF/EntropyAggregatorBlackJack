#include <stddef.h>   /* NULL */

#include "mode.h"

/* ── Shadow-dealer dialogue (font is caps-only: no . , ' allowed) ─────────── */
static const char *LINES_INTRO[]         = {
    "WELCOME    SIT    PLAY", "PLACE YOUR BET" };
static const char *LINES_PLAYER_WIN[]    = {
    "LUCKY    FOR NOW", "DONT GET COMFORTABLE", "ENJOY IT WHILE IT LASTS" };
static const char *LINES_BUST[]          = {
    "TOO GREEDY", "GREED KILLS" };
static const char *LINES_DEALER_WIN[]    = {
    "THE HOUSE ALWAYS WINS", "MINE NOW", "YOU SHOULD LEAVE" };
static const char *LINES_PUSH[]          = {
    "A DRAW    HOW DULL" };
static const char *LINES_GAMEOVER_WON[]  = {
    "YOU ESCAPE PURGATORY    FOR NOW", "THE GATES OPEN    LEAVE WHILE YOU CAN" };
static const char *LINES_GAMEOVER_LOST[] = {
    "WELCOME TO PURGATORY", "YOUR SOUL STAYS HERE", "THERE IS NO LEAVING" };
static const char *LINES_ALLIN[]         = {
    "ALL IN    BOLD", "EVERYTHING    HOW DESPERATE", "YOUR SOUL ON THE TABLE" };
static const char *LINES_LOW[]           = {
    "DOWN TO SCRAPS", "ALMOST EMPTY", "THE END CREEPS CLOSER" };

/* Field order: name, finite_house, horror, then the {lines, count} pools in the
 * order declared in mode.h (intro, player_win, bust, dealer_win, push, over_won,
 * over_lost, allin, low). */
const GameMode MODE_PURGATORY = {
    "PURGATORY", true, true,
    { LINES_INTRO,         2 },
    { LINES_PLAYER_WIN,    3 },
    { LINES_BUST,          2 },
    { LINES_DEALER_WIN,    3 },
    { LINES_PUSH,          1 },
    { LINES_GAMEOVER_WON,  2 },
    { LINES_GAMEOVER_LOST, 3 },
    { LINES_ALLIN,         3 },
    { LINES_LOW,           3 },
};

const GameMode MODE_NORMAL = {
    "NORMAL", false, false,
    { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },
    { 0, 0 }, { 0, 0 },
};

const char *mode_line(LinePool pool) {
    static unsigned seq = 0;
    if (pool.lines == NULL || pool.count <= 0)
        return "";
    return pool.lines[seq++ % (unsigned) pool.count];
}
