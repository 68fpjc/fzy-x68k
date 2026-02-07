#include <condrv.h>
#include <mbctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <x68k/dos.h>
#include <x68k/iocs.h>

#include "tty.h"
#include "ttykey.h"

#define X68K_COLOR_NORMAL 33
#define X68K_COLOR_HIGHLIGHT 36

/**
 * @brief tty_getchar() が呼び出された時点のキーコードグループ 7 の状態
 */
static int keybit7;

/**
 * @brief tty_getchar() が呼び出された時点のシフトキーの状態
 */
static int sftsns;

/**
 * @brief キーコードに対応する TTY_KEY の配列 1
 *
 * SHIFT / CTRL / OPT.1 / OPT.2 のいずれも押されていない場合
 */
static const TTY_KEY tty_key_map_nonshift[256] = {
    [0x07] = TTY_KEY_DEL,	//
    [0x08] = TTY_KEY_BACKSPACE, //
    [0x09] = TTY_KEY_TAB,	//
    [0x0d] = TTY_KEY_ENTER,	//
    [0x1b] = TTY_KEY_ESC	//
};

/**
 * @brief キーコードに対応する TTY_KEY の配列 2
 *
 * SHIFT / CTRL / OPT.1 / OPT.2 のいずれかが押されている場合
 */
static const TTY_KEY tty_key_map[256] = {
    [0x01] = TTY_KEY_CTRL_A, //
    [0x02] = TTY_KEY_CTRL_B, //
    [0x03] = TTY_KEY_CTRL_C, //
    [0x04] = TTY_KEY_CTRL_D, //
    [0x05] = TTY_KEY_CTRL_E, //
    [0x06] = TTY_KEY_CTRL_F, //
    [0x08] = TTY_KEY_CTRL_H, //
    [0x09] = TTY_KEY_CTRL_I, //
    [0x0a] = TTY_KEY_CTRL_J, //
    [0x0b] = TTY_KEY_CTRL_K, //
    [0x0d] = TTY_KEY_CTRL_M, //
    [0x0e] = TTY_KEY_CTRL_N, //
    [0x10] = TTY_KEY_CTRL_P, //
    [0x15] = TTY_KEY_CTRL_U, //
    [0x16] = TTY_KEY_CTRL_V, //
    [0x17] = TTY_KEY_CTRL_W, //
    [0x1a] = TTY_KEY_CTRL_Z, //
    [0x1b] = TTY_KEY_ESC     //
};

#if TTY_KEY_NORMAL != 0
#error TTY_KEY_NORMAL must be 0
#endif

/**
 * @brief シフトキーの状態を取得する (SHIFT / CTRL / OPT.1 / OPT.2)
 * @param sftsns シフトキーの状態
 * @return いずれかが押されている場合は非ゼロ値、押されていない場合は 0
 */
static int tty_sftsns(const int sftsns) {
	return sftsns & 0x000f;
}

/**
 * @brief カーソルキーの状態を取得する (↑)
 * @param keybit7 キーコードグループ 7 の状態
 * @return 押されている場合は非ゼロ値、押されていない場合は 0
 */
static int tty_sns_arrow_up(const int keybit7) {
	return keybit7 & 0x0010;
}

/**
 * @brief カーソルキーの状態を取得する (↓)
 * @param keybit7 キーコードグループ 7 の状態
 * @return 押されている場合は非ゼロ値、押されていない場合は 0
 */
static int tty_sns_arrow_down(const int keybit7) {
	return keybit7 & 0x0040;
}

/**
 * @brief カーソルキーの状態を取得する (←)
 * @param keybit7 キーコードグループ 7 の状態
 * @return 押されている場合は非ゼロ値、押されていない場合は 0
 */
static int tty_sns_arrow_left(const int keybit7) {
	return keybit7 & 0x0008;
}

/**
 * @brief カーソルキーの状態を取得する (→)
 * @param keybit7 キーコードグループ 7 の状態
 * @return 押されている場合は非ゼロ値、押されていない場合は 0
 */
static int tty_sns_arrow_right(const int keybit7) {
	return keybit7 & 0x0020;
}

/**
 * @brief ROLL UP キーの状態を取得する
 * @param keybit7 キーコードグループ 7 の状態
 * @return 押されている場合は非ゼロ値、押されていない場合は 0
 */
static int tty_sns_rollup(const int keybit7) {
	return keybit7 & 0x0001;
}

/**
 * @brief ROLL DOWN キーの状態を取得する
 * @param keybit7 キーコードグループ 7 の状態
 * @return 押されている場合は非ゼロ値、押されていない場合は 0
 */
static int tty_sns_rolldown(const int keybit7) {
	return keybit7 & 0x0002;
}

/**
 * @brief X68000 のキー入力から TTY_KEY を取得する
 * @param ch キーコード
 * @param sftsns シフトキーの状態
 * @param keybit7 キーコードグループ 7 の状態
 * @return TTY_KEY
 */
static TTY_KEY tty_to_tty_key_internal(const short ch, const int sftsns, const int keybit7) {
	TTY_KEY ret;
	if (tty_sftsns(sftsns)) {
		ret = (ch >= 0 && ch < 256) ? tty_key_map[ch] : TTY_KEY_NORMAL;
	} else {
		ret = (ch >= 0 && ch < 256) ? tty_key_map_nonshift[ch] : TTY_KEY_NORMAL;
		if (ret == TTY_KEY_NORMAL) {
			if (tty_sns_arrow_up(keybit7)) {
				ret = TTY_KEY_UP;
			} else if (tty_sns_arrow_down(keybit7)) {
				ret = TTY_KEY_DOWN;
			} else if (tty_sns_arrow_left(keybit7)) {
				ret = TTY_KEY_LEFT;
			} else if (tty_sns_arrow_right(keybit7)) {
				ret = TTY_KEY_RIGHT;
			} else if (tty_sns_arrow_up(keybit7)) {
				ret = TTY_KEY_UP;
			} else if (tty_sns_arrow_down(keybit7)) {
				ret = TTY_KEY_DOWN;
			} else if (tty_sns_rollup(keybit7)) {
				ret = TTY_KEY_PAGEDOWN; // PAGE UP と PAGE DOWN を入れ替える
			} else if (tty_sns_rolldown(keybit7)) {
				ret = TTY_KEY_PAGEUP; // PAGE UP と PAGE DOWN を入れ替える
			} else {
				ret = TTY_KEY_NORMAL;
			}
		}
	}
	return ret;
}

TTY_KEY tty_to_tty_key(const short ch) {
	return tty_to_tty_key_internal(ch, sftsns, keybit7);
}

void tty_reset(tty_t *tty) {
	tty_fputs(tty, "\x1b[0m");
}

static int condrv_level = -1; // condrv_xoff() の戻り値

/**
 * @brief condrv(em).sys のバッファリング処理を復旧する
 */
static void xon_condrv(void) {
	if (condrv_level >= 0) {
		condrv_xon();
		condrv_level = -1;
	}
}

/**
 * @brief エラーによるアボート時の処理
 */
__attribute__((noreturn)) static void errjvc(void) {
	xon_condrv();
	_dos_exit2(1);
}

/**
 * @brief condrv(em).sys のバッファリング処理を停止する
 *
 * DOS _INTVCS により、エラーによるアボート時にはバッファリング処理を復旧する (ただし完全ではない)
 */
static void xoff_condrv(void) {
	union {
		void (*func_ptr)(void);
		void *obj_ptr;
	} safe_cast;
	safe_cast.func_ptr = errjvc;
	_dos_intvcs(0xfff2, safe_cast.obj_ptr);
	condrv_level = condrv_xoff();
}

void tty_close(tty_t *tty) {
	tty_reset(tty);
	tty_flush(tty);
	xon_condrv();
}

void tty_init(tty_t *tty, const char *tty_filename) {
	(void)tty_filename;
	tty_getwinsz(tty);
	tty_setnormal(tty);
	xoff_condrv();
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
	if (ismbblead(ret)) {
		ret = ret << 8 | _dos_k_keyinp();
	}
	keybit7 = _dos_k_keybit(7);
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

void tty_flush_keys(void) {
	while (_dos_k_keysns()) {
		_dos_k_keyinp();
	}
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
	char ch_high = _MBGETH(wc);
	if (ch_high) {
		tty_putc(tty, ch_high);
	}
	tty_putc(tty, _MBGETL(wc));
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
