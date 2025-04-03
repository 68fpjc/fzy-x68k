#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config.h"
#include "cp932.h"
#include "match.h"
#include "tty_interface.h"

#define BATCH_SIZE 10

static size_t prev_cursor(tty_interface_t *state) {
	size_t ret = 0;
	{
		size_t tmp = 0;
		while (tmp < state->cursor) {
			ret = tmp;
			tmp += is_cp932_lead_byte(state->search[tmp]) ? 2 : 1;
		}
	}
	return ret;
}

static void clear(tty_interface_t *state) {
	tty_t *tty = state->tty;
	tty_carriagereturn(tty);
	tty_clearend(tty);
	tty_flush(tty);
}

static void draw_match(tty_interface_t *state, const char *choice, int selected) {
	tty_t *tty = state->tty;
	options_t *options = state->options;
	char *search = state->last_search;

	size_t n = cp932_strlen(search);
	size_t positions[n + 1];
	for (size_t i = 0; i < n + 1; i++)
		positions[i] = (size_t)-1;

	score_t score = match_positions(search, choice, &positions[0]);

	if (options->show_scores) {
		if (score == SCORE_MIN) {
			tty_fputs(tty, "(     ) ");
		} else {
			tty_printf(tty, "(%5.2f) ", score);
		}
	}

	if (selected)
#ifdef TTY_SELECTION_UNDERLINE
		tty_setunderline(tty);
#else
		tty_setinvert(tty);
#endif

	tty_setnowrap(tty);
	for (size_t i = 0; choice[i] != '\0';) {
		int highlighted = 0;
		for (size_t j = 0; j < n; j++) {
			if (positions[j] == i) {
				highlighted = 1;
				break;
			}
		}
		if (highlighted) {
			tty_setfg(tty, TTY_COLOR_HIGHLIGHT);
		} else {
			tty_setfg(tty, TTY_COLOR_NORMAL);
		}
		tty_putc(tty, choice[i]);
		if (is_cp932_lead_byte(choice[i]) && choice[i + 1]) {
			tty_putc(tty, choice[i + 1]);
			i += 2;
		} else {
			i++;
		}
	}
	tty_setwrap(tty);
	tty_setnormal(tty);
}

static void draw_prompt(tty_interface_t *state) {
	tty_t *tty = state->tty;
	options_t *options = state->options;
	tty_fputs(tty, options->prompt);
}

static void draw_search_full(tty_interface_t *state) {
	tty_t *tty = state->tty;
	tty_setcursor(tty, state->search_home);
	tty_fputs(tty, state->search);
	tty_clearline(tty);
}

static void draw_search_cursor(tty_interface_t *state) {
	tty_t *tty = state->tty;
	tty_setcursor(tty, state->search_home);
	for (size_t i = 0; i < state->cursor; i++)
		tty_putc(tty, state->search[i]);
}

static void draw_results(tty_interface_t *state) {
	tty_t *tty = state->tty;
	choices_t *choices = state->choices;
	options_t *options = state->options;

	unsigned int num_lines = options->num_lines;
	size_t start = 0;
	size_t current_selection = choices->selection;
	if (current_selection + options->scrolloff >= num_lines) {
		start = current_selection + options->scrolloff - num_lines + 1;
		size_t available = choices_available(choices);
		if (start + num_lines >= available && available > 0) {
			start = available - num_lines;
		}
	}
	for (size_t i = start; i < start + num_lines; i++) {
		tty_carriagereturn(tty);
		tty_linefeed(tty);
		tty_clearline(tty);
		const char *choice = choices_get(choices, i);
		if (choice) {
			draw_match(state, choice, i == choices->selection);
		}
	}
}

static void draw(tty_interface_t *state) {
	if (state->redraw_search_full) {
		draw_search_full(state);
		state->redraw_search_full = 0;
	}
	if (state->redraw_search_cursor) {
		draw_search_cursor(state);
		state->redraw_search_cursor = 0;
	}
	if (state->redraw_results) {
		tty_cursor_t cursor = tty_getcursor(state->tty);
		draw_results(state);
		tty_setcursor(state->tty, cursor);
		state->redraw_results = 0;
	}
	tty_flush(state->tty);
}

static void update_search(tty_interface_t *state) {
	choices_search_start(state->choices, state->search); // 遅延検索を使う
	strcpy(state->last_search, state->search);
}

static int update_state(tty_interface_t *state) {
	int ret = strcmp(state->last_search, state->search);
	if (ret) {
		update_search(state);
	}
	if (!choices_is_search_complete(state->choices)) {
		if (!choices_search_step(state->choices, BATCH_SIZE)) {
			state->redraw_results = 1; // 検索が完了した場合のみ結果を再描画
			return 1;
		}
	}
	return ret;
}

static void action_emit(tty_interface_t *state) {
	/* Reset the tty as close as possible to the previous state */
	clear(state);

	/* ttyout should be flushed before outputting on stdout */
	tty_close(state->tty);

	const char *selection = choices_get(state->choices, state->choices->selection);
	if (selection) {
		/* output the selected result */
		printf("%s\n", selection);
	} else {
		/* No match, output the query instead */
		printf("%s\n", state->search);
	}

	state->exit = EXIT_SUCCESS;
}

static void action_del_char(tty_interface_t *state) {
	size_t length = strlen(state->search);
	if (state->cursor == 0) {
		return;
	}
	size_t original_cursor = state->cursor;
	state->cursor = prev_cursor(state);
	memmove(&state->search[state->cursor], &state->search[original_cursor],
		length - original_cursor + 1);
	state->redraw_search_full = 1;
	state->redraw_search_cursor = 1;
}

static void action_del_word(tty_interface_t *state) {
	size_t original_cursor = state->cursor;
	size_t cursor = state->cursor;

	while (cursor > 0 &&
	       (state->search[cursor - 1] == ' ' || state->search[cursor - 1] == '\t')) {
		cursor = is_cp932_lead_byte(state->search[cursor - 2]) && cursor >= 2 ? cursor - 2
										      : cursor - 1;
	}
	while (cursor > 0 &&
	       (state->search[cursor - 1] != ' ' && state->search[cursor - 1] != '\t')) {
		cursor = is_cp932_lead_byte(state->search[cursor - 2]) && cursor >= 2 ? cursor - 2
										      : cursor - 1;
	}
	memmove(&state->search[cursor], &state->search[original_cursor],
		strlen(state->search) - original_cursor + 1);
	state->cursor = cursor;
	state->redraw_search_full = 1;
	state->redraw_search_cursor = 1;
}

static void action_del_all(tty_interface_t *state) {
	memmove(state->search, &state->search[state->cursor],
		strlen(state->search) - state->cursor + 1);
	state->cursor = 0;
	state->redraw_search_full = 1;
	state->redraw_search_cursor = 1;
}

static void action_prev(tty_interface_t *state) {
	choices_prev(state->choices);
	state->redraw_results = 1;
}

static void action_next(tty_interface_t *state) {
	choices_next(state->choices);
	state->redraw_results = 1;
}

static void action_left(tty_interface_t *state) {
	size_t cursor = prev_cursor(state);
	if (cursor != state->cursor) {
		state->cursor = cursor;
		state->redraw_search_cursor = 1;
	}
}

static void action_right(tty_interface_t *state) {
	if (state->cursor < strlen(state->search)) {
		state->cursor += is_cp932_lead_byte(state->search[state->cursor]) ? 2 : 1;
		state->redraw_search_cursor = 1;
	}
}

static void action_beginning(tty_interface_t *state) {
	if (state->cursor) {
		state->cursor = 0;
		state->redraw_search_cursor = 1;
	}
}

static void action_end(tty_interface_t *state) {
	size_t cursor = strlen(state->search);
	if (cursor != state->cursor) {
		state->cursor = cursor;
		state->redraw_search_cursor = 1;
	}
}

static void action_pageup(tty_interface_t *state) {
	for (size_t i = 0; i < state->options->num_lines && state->choices->selection > 0; i++)
		choices_prev(state->choices);
	state->redraw_results = 1;
}

static void action_pagedown(tty_interface_t *state) {
	for (size_t i = 0; i < state->options->num_lines &&
			   state->choices->selection < state->choices->available - 1;
	     i++)
		choices_next(state->choices);
	state->redraw_results = 1;
}

static void action_autocomplete(tty_interface_t *state) {
	const char *current_selection = choices_get(state->choices, state->choices->selection);
	if (current_selection) {
		strncpy(state->search, choices_get(state->choices, state->choices->selection),
			SEARCH_SIZE_MAX);
		state->cursor = strlen(state->search);
		state->redraw_search_full = 1;
		state->redraw_search_cursor = 1;
	}
}

static void action_exit(tty_interface_t *state) {
	clear(state);
	tty_close(state->tty);

	state->exit = EXIT_FAILURE;
}

static void append_search(tty_interface_t *state, const short wc) {
	char *search = state->search;
	size_t search_size = strlen(search);
	char wc_high = wc >> 8;
	char wc_low = wc & 0xFF;
	size_t ch_size = wc_high ? 2 : 1;
	if (search_size + ch_size <= SEARCH_SIZE_MAX) {
		char *p = search + state->cursor;
		memmove(p + ch_size, p, search_size - state->cursor + ch_size);
		if (wc_high) {
			*p++ = wc_high;
		}
		*p = wc_low;
		state->cursor += ch_size;
		state->redraw_search_full = 1;
		state->redraw_search_cursor = 1;
	}
}

void tty_interface_init(tty_interface_t *state, tty_t *tty, choices_t *choices,
			options_t *options) {
	state->tty = tty;
	state->choices = choices;
	state->options = options;

	strcpy(state->search, "");
	strcpy(state->last_search, "x"); // Force the search to start

	state->exit = -1;
	state->redraw_search_full = 0;
	state->redraw_search_cursor = 0;

	if (options->init_search)
		strncpy(state->search, options->init_search, SEARCH_SIZE_MAX);

	state->cursor = strlen(state->search);
}

typedef struct {
	const TTY_KEY key;
	void (*action)(tty_interface_t *);
} keybinding_t;

static const keybinding_t keybindings[] = {
    {TTY_KEY_ESC, action_exit},		   /* ESC */
    {TTY_KEY_CTRL_H, action_del_char},	   /* Backspace (C-H) */
    {TTY_KEY_CTRL_W, action_del_word},	   /* C-W */
    {TTY_KEY_CTRL_U, action_del_all},	   /* C-U */
    {TTY_KEY_CTRL_I, action_autocomplete}, /* TAB (C-I ) */
    {TTY_KEY_CTRL_C, action_exit},	   /* C-C */
    {TTY_KEY_CTRL_D, action_exit},	   /* C-D */
    {TTY_KEY_CTRL_M, action_emit},	   /* CR */
    {TTY_KEY_CTRL_P, action_prev},	   /* C-P */
    {TTY_KEY_CTRL_N, action_next},	   /* C-N */
    {TTY_KEY_CTRL_K, action_prev},	   /* C-K */
    {TTY_KEY_CTRL_J, action_next},	   /* C-J */
    {TTY_KEY_CTRL_A, action_beginning},	   /* C-A */
    {TTY_KEY_CTRL_E, action_end},	   /* C-E */
    {TTY_KEY_LEFT, action_left},	   /* Left */
    {TTY_KEY_RIGHT, action_right},	   /* Right */
    {TTY_KEY_HOME, action_beginning},	   /* Home */
    {TTY_KEY_PAGEUP, action_pageup},	   /* PageUp */
    {TTY_KEY_PAGEDOWN, action_pagedown},   /* PageDown */
    {'\0', NULL}			   /* End of keybindings */
};

static void handle_input(tty_interface_t *state, const short wc) {
	{
		/* Figure out if we have completed a keybinding */
		int found_keybinding = -1;
		{
			TTY_KEY ttykey = tty_to_tty_key(wc);
			for (int i = 0; keybindings[i].action; i++) {
				if (keybindings[i].key == ttykey) {
					found_keybinding = i;
					break;
				}
			}
		}
		/* If we have an unambiguous keybinding, run it.  */
		if (found_keybinding != -1) {
			keybindings[found_keybinding].action(state);
			return;
		}
	}
	/* No matching keybinding, add to search */
	if (is_print_cp932(wc)) {
		tty_putw(state->tty, wc);
		tty_flush(state->tty);
		append_search(state, wc);
	}
}

int tty_interface_run(tty_interface_t *state) {
	tty_alloc(state->tty, state->options->num_lines);
	draw_prompt(state);
	state->search_home = tty_getcursor(state->tty);

	while (1) {
		short wc = tty_getchar_nonblock(state->tty);
		if (wc) {
			handle_input(state, wc);
			if (state->exit >= 0) {
				break;
			}
		}
		if (!update_state(state) && wc) {
			tty_flush_keys();
		}
		draw(state);
	}
	return state->exit;
}
