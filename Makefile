PROG = proj

# Sources live in purpose-grouped subdirs; .PATH lets bmake find them by name.
.PATH: ${.CURDIR}/drivers ${.CURDIR}/entropy ${.CURDIR}/video ${.CURDIR}/game ${.CURDIR}/analysis

SRCS = main.c timer.c utils.c kbc.c mouse.c rtc.c sha256.c pool.c entropy.c deck.c video.c sprite.c text.c button.c blackjack.c render.c game.c mode.c analysis.c

# Let cross-directory #include "header.h" resolve.
CPPFLAGS += -I${.CURDIR}/drivers -I${.CURDIR}/entropy -I${.CURDIR}/video -I${.CURDIR}/game -I${.CURDIR}/analysis

CFLAGS += -pedantic

# MINIX's clang 3.6 segfaults at -O2 on this code (compiler bug, multiple files,
# different passes). -O0 is the only config that builds; perf floor here is the
# framebuffer copy, not the optimizer.
CFLAGS += -O0

DPADD += ${LIBLCF}
LDADD += -llcf

.include <minix.lcom.mk>
