#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <x68k/dos.h>
#include <x68k/iocs.h>

#include "tty.h"
#include "ttykey.h"

#define X68K_COLOR_NORMAL 33
#define X68K_COLOR_HIGHLIGHT 36

// tty_getchar() が呼び出された時点のシフトキーの状態
static int sftsns;

static int tty_sns_ctrl() {
	return sftsns & 0x0002;
}

TTY_KEY tty_to_tty_key(const short ch) {
	TTY_KEY ret = TTY_KEY_NORMAL;
	switch (ch) {
		case 0x1b:
			ret = TTY_KEY_ESC;
			break;
		case 0x08:
			ret = TTY_KEY_CTRL_H;
			break;
		case 0x17:
			ret = tty_sns_ctrl() ? TTY_KEY_CTRL_W : TTY_KEY_PAGEUP;
			break;
		case 0x15:
			ret = TTY_KEY_CTRL_U;
			break;
		case 0x09:
			ret = TTY_KEY_CTRL_I;
			break;
		case 0x03:
			ret = TTY_KEY_CTRL_C;
			break;
		case 0x04:
			ret = tty_sns_ctrl() ? TTY_KEY_CTRL_D : TTY_KEY_RIGHT;
			break;
		case 0x0d:
			ret = TTY_KEY_CTRL_M;
			break;
		case 0x10:
			ret = TTY_KEY_CTRL_P;
			break;
		case 0x0e:
			ret = TTY_KEY_CTRL_N;
			break;
		case 0x0b:
			ret = TTY_KEY_CTRL_K;
			break;
		case 0x0a:
			ret = TTY_KEY_CTRL_J;
			break;
		case 0x01:
			ret = TTY_KEY_CTRL_A;
			break;
		case 0x05:
			ret = tty_sns_ctrl() ? TTY_KEY_CTRL_E : TTY_KEY_PAGEDOWN;
			break;
		case 0x13:
			ret = TTY_KEY_LEFT;
			break;
		case 0x45:
			ret = TTY_KEY_HOME;
			break;
		case 0x06:
			ret = TTY_KEY_DOWN;
			break;
		default:
			break;
	}
	return ret;
}

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

short tty_getchar(tty_t *tty) {
	short ret = _dos_k_keyinp();
	if (is_cp932_lead_byte(ret)) {
		ret = ret << 8 | _dos_k_keyinp();
	}
	sftsns = _dos_k_sftsns();
	return ret;
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

void tty_carriagereturn(tty_t *tty) {
	tty_putc(tty, '\r');
}

void tty_linefeed(tty_t *tty) {
	tty_putc(tty, '\n');
}

void tty_clearline(tty_t *tty) {
	tty_fputs(tty, "\x1b[K");
}

void tty_clearend(tty_t *tty) {
	tty_fputs(tty, "\x1b[J");
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
