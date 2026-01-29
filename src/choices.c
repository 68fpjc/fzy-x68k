#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "choices.h"
#include "chop.h"
#include "match.h"
#include "options.h"

/* Initial size of buffer for storing input in memory */
#define INITIAL_BUFFER_CAPACITY 4096

/* Initial size of choices array */
#define INITIAL_CHOICE_CAPACITY 128

static int cmpchoice(const void *_idx1, const void *_idx2) {
	const struct scored_result *a = _idx1;
	const struct scored_result *b = _idx2;

	if (a->score == b->score) {
		/* To ensure a stable sort, we must also sort by the string
		 * pointers. We can do this since we know all the strings are
		 * from a contiguous memory segment (buffer in choices_t).
		 */
		if (a->str < b->str) {
			return -1;
		} else {
			return 1;
		}
	} else if (a->score < b->score) {
		return 1;
	} else {
		return -1;
	}
}

static void *safe_realloc(void *buffer, size_t size) {
	buffer = realloc(buffer, size);
	if (!buffer) {
		fprintf(stderr, "Error: Can't allocate memory (%zu bytes)\n", size);
		abort();
	}

	return buffer;
}

void choices_fread(choices_t *c, FILE *file) {
	/* Save current position for parsing later */
	size_t buffer_start = c->buffer_size;

	/* Resize buffer to at least one byte more capacity than our current
	 * size. This uses a power of two of INITIAL_BUFFER_CAPACITY.
	 * This must work even when c->buffer is NULL and c->buffer_size is 0
	 */
	size_t capacity = INITIAL_BUFFER_CAPACITY;
	while (capacity <= c->buffer_size)
		capacity *= 2;
	c->buffer = safe_realloc(c->buffer, capacity);

	/* Continue reading until we get a "short" read, indicating EOF */
	while ((c->buffer_size += fread(c->buffer + c->buffer_size, 1, capacity - c->buffer_size,
					file)) == capacity) {
		capacity *= 2;
		c->buffer = safe_realloc(c->buffer, capacity);
	}
	c->buffer = safe_realloc(c->buffer, c->buffer_size + 1);
	c->buffer[c->buffer_size++] = '\0';

	/* Truncate buffer to used size, (maybe) freeing some memory for
	 * future allocations.
	 */

	/* Tokenize input and add to choices */
	char *line = c->buffer + buffer_start;
	do {
		char *nl = strchr(line, '\n');
		if (nl)
			*nl++ = '\0';

		/* Skip empty lines */
		if (*line)
			choices_add(c, line);

		line = nl;
	} while (line);
}

static void buffer_output(int c, chop_output_context *ctx) {
	if (ctx->ptr) {
		*ctx->ptr++ = (char)c;
	}
}

void choices_finish_fread(choices_t *c, const size_t maxwidth, const int show_scores) {
	c->buffer_con = malloc(c->buffer_size);
	if (!c->buffer_con) {
		fprintf(stderr, "Error: Can't allocate memory\n");
		abort();
	}
	c->strings_con = malloc(c->size * sizeof(char *));
	if (!c->strings_con) {
		fprintf(stderr, "Error: Can't allocate memory\n");
		abort();
	}
	{
		chop_output_context ctx = {buffer_output, c->buffer_con};
		int col = !show_scores ? 0 : 8;
		for (size_t i = 0; i < c->size; i++) {
			c->strings_con[i] = ctx.ptr;
			chop(c->strings[i], col, maxwidth, &ctx);
			*ctx.ptr++ = '\0';
		}
	}
}

static void choices_resize(choices_t *c, size_t new_capacity) {
	c->strings = safe_realloc(c->strings, new_capacity * sizeof(const char *));
	c->capacity = new_capacity;
}

static void choices_reset_search(choices_t *c) {
	free(c->results);
	c->selection = c->available = 0;
	c->results = NULL;
	c->search_in_progress = 0;
	c->processed_count = 0;
}

void choices_init(choices_t *c, options_t *options) {
	(void)options;
	c->strings = NULL;
	c->results = NULL;

	c->buffer_size = 0;
	c->buffer = NULL;

	c->capacity = c->size = 0;
	choices_resize(c, INITIAL_CHOICE_CAPACITY);

	choices_reset_search(c);

	c->match = match;
#ifdef NO_CALC_SCORE_OPTION
	if (!options->calc_score) {
		c->match = match_stub;
	}
#endif

	/* Initialize delayed search */
	c->last_search = NULL;
	c->processed_count = 0;
	c->search_in_progress = 0;
}

void choices_destroy(choices_t *c) {
	free(c->buffer);
	c->buffer = NULL;
	c->buffer_size = 0;

	free(c->strings);
	c->strings = NULL;
	c->capacity = c->size = 0;

	free(c->results);
	c->results = NULL;
	c->available = c->selection = 0;

	// Cleanup for delayed search
	if (c->last_search) {
		free(c->last_search);
		c->last_search = NULL;
	}
	c->processed_count = 0;
	c->search_in_progress = 0;

	free(c->buffer_con);
	c->buffer_con = NULL;
	free(c->strings_con);
	c->strings_con = NULL;
}

void choices_add(choices_t *c, const char *choice) {
	/* Previous search is now invalid */
	choices_reset_search(c);

	if (c->size == c->capacity) {
		choices_resize(c, c->capacity * 2);
	}
	c->strings[c->size++] = choice;
}

size_t choices_available(choices_t *c) {
	return c->available;
}

#define BATCH_SIZE 512

struct result_list {
	struct scored_result *list;
	size_t size;
};

void choices_search(choices_t *c, const char *search) {
	choices_reset_search(c);

	c->results = malloc(c->size * sizeof(struct scored_result));
	if (!c->results) {
		fprintf(stderr, "Error: Can't allocate memory\n");
		abort();
	}

	c->available = 0;
	for (size_t i = 0; i < c->size; i++) {
		if (has_match(search, c->strings[i])) {
			c->results[c->available].str = c->strings[i];
			c->results[c->available].str_con = c->strings_con[i];
			c->results[c->available].score = c->match(search, c->strings[i]);
			c->available++;
		}
	}

	qsort(c->results, c->available, sizeof(struct scored_result), cmpchoice);
}

void choices_search_start(choices_t *c, const char *search) {
	choices_reset_search(c);

	// Save the last search string
	if (c->last_search) {
		free(c->last_search);
	}
	c->last_search = strdup(search);
	if (!c->last_search) {
		fprintf(stderr, "Error: Can't allocate memory for search string\n");
		abort();
	}

	c->results = malloc(c->size * sizeof(struct scored_result));
	if (!c->results) {
		fprintf(stderr, "Error: Can't allocate memory\n");
		abort();
	}

	c->processed_count = 0;
	c->available = 0;
	c->search_in_progress = 1;
}

int choices_search_step(choices_t *c, size_t batch_size) {
	if (!c->search_in_progress || !c->last_search) {
		return 0;
	}

	size_t end = c->processed_count + batch_size;
	if (end > c->size) {
		end = c->size;
	}

	// Process items in batches
	for (size_t i = c->processed_count; i < end; i++) {
		if (has_match(c->last_search, c->strings[i])) {
			c->results[c->available].str = c->strings[i];
			c->results[c->available].str_con = c->strings_con[i];
			c->results[c->available].score = c->match(c->last_search, c->strings[i]);
			c->available++;
		}
	}

	c->processed_count = end;

	// Once all items are processed, sort the results
	if (c->processed_count >= c->size) {
		qsort(c->results, c->available, sizeof(struct scored_result), cmpchoice);
		c->search_in_progress = 0;
		return 0; // Search complete
	}

	return 1; // Processing is still ongoing
}

int choices_is_search_complete(choices_t *c) {
	return !c->search_in_progress;
}

const char *choices_get(choices_t *c, size_t n) {
	if (n < c->available) {
		return c->results[n].str;
	} else {
		return NULL;
	}
}

const char *choices_get_con(choices_t *c, size_t n) {
	if (n < c->available) {
		return c->results[n].str_con;
	} else {
		return NULL;
	}
}

score_t choices_getscore(choices_t *c, size_t n) {
	return c->results[n].score;
}

void choices_prev(choices_t *c) {
	if (c->available)
		c->selection = (c->selection + c->available - 1) % c->available;
}

void choices_next(choices_t *c) {
	if (c->available)
		c->selection = (c->selection + 1) % c->available;
}
