#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <x68k/dos.h>
#include <x68k/iocs.h>

#include "tty.h"

#include "../config.h"

#define X68K_COLOR_NORMAL 33
#define X68K_COLOR_HIGHLIGHT 36

void tty_reset(tty_t *tty) {
	tty_fputs(tty, "\x1b[0m");
}

void tty_close(tty_t *tty) {
	tty_reset(tty);
	tty_flush(tty);
}

void tty_init(tty_t *tty, const char *tty_filename) {
	tty_getwinsz(tty);
	tty_setnormal(tty);
}

void tty_getwinsz(tty_t *tty) {
	int v = _iocs_b_consol(-1, -1, -1, -1);
	tty->maxwidth = (v >> 16) + 1;
	tty->maxheight = (v & 0xffff) + 1;
}

char tty_getchar(tty_t *tty) {
	static int initialized = 0;
	if (!initialized) {
		fclose(stdin);
		initialized = 1;
	}
	return _dos_inkey();
}

int tty_input_ready(tty_t *tty, long int timeout, int return_on_signal) {
	// fd_set readfs;
	// FD_ZERO(&readfs);
	// FD_SET(tty->fdin, &readfs);

	// struct timespec ts = {timeout / 1000, (timeout % 1000) * 1000000};

	// sigset_t mask;
	// sigemptyset(&mask);
	// if (!return_on_signal)
	// 	sigaddset(&mask, SIGWINCH);

	// int err = pselect(tty->fdin + 1, &readfs, NULL, NULL, timeout < 0 ? NULL : &ts,
	// 		  return_on_signal ? NULL : &mask);

	// if (err < 0) {
	// 	if (errno == EINTR) {
	// 		return 0;
	// 	} else {
	// 		perror("select");
	// 		exit(EXIT_FAILURE);
	// 	}
	// } else {
	// 	return FD_ISSET(tty->fdin, &readfs);
	// }
	return 1;
}

static void tty_sgr(tty_t *tty, int code) {
	if (tty->sgr != code) {
		tty_printf(tty, "\x1b[%im", code);
		tty->sgr = code;
	}
}

static void tty_setfg_internal(tty_t *tty, int fg) {
	tty->fgcolor = fg;
	tty_sgr(tty, fg + (tty->invert ? 10 : 0));
}

void tty_setfg(tty_t *tty, int fg) {
	tty_setfg_internal(tty, fg == TTY_COLOR_NORMAL ? X68K_COLOR_NORMAL : X68K_COLOR_HIGHLIGHT);
}

void tty_setinvert(tty_t *tty) {
	tty->invert = 1;
	tty_setfg_internal(tty, tty->fgcolor);
}

void tty_setunderline(tty_t *tty) {
	// not supported
}

void tty_setnormal(tty_t *tty) {
	tty->fgcolor = X68K_COLOR_NORMAL;
	tty->sgr = ~tty->fgcolor;
	tty->invert = 0;
	tty_setfg_internal(tty, tty->fgcolor);
}

void tty_setnowrap(tty_t *tty) {
	// not supported
}

void tty_setwrap(tty_t *tty) {
	// not supported
}

void tty_newline(tty_t *tty) {
	tty_fputs(tty, "\x1b[K\n");
}

void tty_clearline(tty_t *tty) {
	tty_fputs(tty, "\x1b[K");
}

void tty_setcol(tty_t *tty, int col) {
	tty_putc(tty, '\r');
	if (col > 0) {
		tty_printf(tty, "\x1b[%iC", col + 1);
	}
}

void tty_moveup(tty_t *tty, int i) {
	tty_printf(tty, "\x1b[%iA", i);
}

void tty_printf(tty_t *tty, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	{
		char *p;
		vasprintf(&p, fmt, args);
		tty_fputs(tty, p);
		free(p);
	}
	va_end(args);
}

void tty_fputs(tty_t *tty, const char *s) {
	for (const char *p = s; *p; ++p) {
		tty_putc(tty, *p);
	}
}

static char ttybuf[1024];
static size_t ttybufpos = 0;

void tty_putc(tty_t *tty, const char c) {
	// fputc(c, tty->fout);
	ttybuf[ttybufpos++] = c;
	if (ttybufpos == sizeof(ttybuf) - 1) {
		tty_flush(tty);
	}
}

void tty_flush(tty_t *tty) {
	if (ttybufpos) {
		ttybuf[ttybufpos] = 0;
		_dos_c_print(ttybuf);
		ttybufpos = 0;
	}
}

size_t tty_getwidth(tty_t *tty) {
	return tty->maxwidth;
}

size_t tty_getheight(tty_t *tty) {
	return tty->maxheight;
}
