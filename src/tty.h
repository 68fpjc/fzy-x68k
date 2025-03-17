#ifndef TTY_H
#define TTY_H TTY_H

#include "ttykey.h"

typedef struct {
	int fdin;
	FILE *fout;
	int fgcolor;
	int invert;
	int sgr;
	size_t maxwidth;
	size_t maxheight;
} tty_t;

TTY_KEY tty_to_tty_key(const short);
void tty_reset(tty_t *tty);
void tty_close(tty_t *tty);
void tty_init(tty_t *tty, const char *tty_filename);
void tty_getwinsz(tty_t *tty);
short tty_getchar(tty_t *tty);
int tty_input_ready(tty_t *tty, long int timeout, int return_on_signal);

void tty_setfg(tty_t *tty, int fg);
void tty_setinvert(tty_t *tty);
void tty_setunderline(tty_t *tty);
void tty_setnormal(tty_t *tty);
void tty_setnowrap(tty_t *tty);
void tty_setwrap(tty_t *tty);

#define TTY_COLOR_BLACK 0
#define TTY_COLOR_RED 1
#define TTY_COLOR_GREEN 2
#define TTY_COLOR_YELLOW 3
#define TTY_COLOR_BLUE 4
#define TTY_COLOR_MAGENTA 5
#define TTY_COLOR_CYAN 6
#define TTY_COLOR_WHITE 7
#define TTY_COLOR_NORMAL 9

/*
 * tty_carriagereturn
 * Move the cursor to the beginning of the current line.
 */
void tty_carriagereturn(tty_t *);

/*
 * tty_linefeed
 * Move the cursor to the beginning of the next line.
 */
void tty_linefeed(tty_t *);

/* tty_clearline
 * Clear to the end of the current line without advancing the cursor.
 */
void tty_clearline(tty_t *tty);

/*
 * tty_clearend
 * Clear to the end of the screen from the current cursor position.
 */
void tty_clearend(tty_t *);

void tty_moveup(tty_t *tty, int i);

void tty_printf(tty_t *tty, const char *fmt, ...);
void tty_fputs(tty_t *tty, const char *s);
void tty_putc(tty_t *tty, const char c);
void tty_flush(tty_t *tty);

size_t tty_getwidth(tty_t *tty);
size_t tty_getheight(tty_t *tty);

#endif
