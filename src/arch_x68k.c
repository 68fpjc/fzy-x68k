#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <x68k/dos.h>
#include <x68k/iocs.h>

#include "cp932.h"
#include "tty.h"
#include "ttykey.h"

#define X68K_COLOR_NORMAL 33
#define X68K_COLOR_HIGHLIGHT 36

int _dos_kflushonly() {
	int ret;
	__asm__ volatile(
	    // MODE = -1 の技は ED.X が使っている
	    "move.w	#-1, %%sp@-\n"
	    ".short	0xff0c\n"
	    "addq.l	#2, %%sp\n"
	    : "=d"(ret) // Output operand to capture d0
	);
	return ret;
}

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
	(void)tty_filename;
	tty_getwinsz(tty);
	tty_setnormal(tty);
}

void tty_alloc(tty_t *tty, unsigned int num_lines) {
	for (unsigned int i = 0; i < num_lines; i++) {
		tty_linefeed(tty);
	}
	for (unsigned int i = 0; i < num_lines; i++) {
		tty_backline(tty);
	}
}

void tty_getwinsz(tty_t *tty) {
	// TODO 本当はカーソルを右下へ移動して DOS _CONCTRL (MD: 3) を使いたい
	int v = _iocs_b_consol(-1, -1, -1, -1);
	tty->maxwidth = (v >> 16) + 1;
	tty->maxheight = (v & 0xffff) + 1;
}

short tty_getchar(tty_t *tty) {
	(void)tty;
	short ret = _dos_k_keyinp();
	if (is_cp932_lead_byte(ret)) {
		ret = ret << 8 | _dos_k_keyinp();
	}
	sftsns = _dos_k_sftsns();
	return ret;
}

short tty_getchar_nonblock(tty_t *tty) {
	short ret = 0;
	if (_dos_k_keysns()) {
		ret = tty_getchar(tty);
	}
	return ret;
}

static void tty_close_stdin() {
	static int initialized = 0;
	if (!initialized) {
		fclose(stdin); // これをしないと DOS _KFLUSH が効かない？
		initialized = 1;
	}
}

void tty_flush_keys(void) {
	tty_close_stdin();
	_dos_kflushonly();
}

tty_cursor_t tty_getcursor(tty_t *tty) {
	tty_flush(tty);
	// TODO 本当は DOS _CONCTRL (MD: 3) を使いたい
	return _iocs_b_locate(-1, -1);
}

void tty_setcursor(tty_t *tty, tty_cursor_t cursor) {
	tty_printf(tty, "\x1b[%d;%dH", (cursor & 0xffff) + 1, (cursor >> 16) + 1);
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
	(void)tty;
}

void tty_setnormal(tty_t *tty) {
	tty->fgcolor = X68K_COLOR_NORMAL;
	tty->sgr = ~tty->fgcolor;
	tty->invert = 0;
	tty_setfg_internal(tty, tty->fgcolor);
}

void tty_setnowrap(tty_t *tty) {
	// not supported
	(void)tty;
}

void tty_setwrap(tty_t *tty) {
	// not supported
	(void)tty;
}

void tty_carriagereturn(tty_t *tty) {
	tty_putc(tty, '\r');
}

void tty_linefeed(tty_t *tty) {
	tty_putc(tty, '\n');
}

void tty_backline(tty_t *tty) {
	tty_printf(tty, "\x1b[A");
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
	char buf[64];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	tty_fputs(tty, buf);
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

void tty_putw(tty_t *tty, const short wc) {
	char ch_high = wc >> 8;
	if (ch_high) {
		tty_putc(tty, ch_high);
	}
	tty_putc(tty, wc & 0xff);
}

void tty_flush(tty_t *tty) {
	(void)tty;
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
