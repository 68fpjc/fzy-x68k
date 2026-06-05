#ifndef CHOICES_H
#define CHOICES_H CHOICES_H

#include <stdio.h>

#include "match.h"
#include "options.h"

struct scored_result {
	score_t score;
	const char *str;
};

typedef struct {
	char *buffer;
	size_t buffer_size;

	size_t capacity;
	size_t size;

	const char **strings;
	struct scored_result *results;

	size_t available;
	size_t selection;

	unsigned int worker_count;

	// Function to match a string
	score_t (*match)(const char *needle, const char *haystack);

	// State for delayed search
	char *last_search;
	size_t processed_count;
	int search_in_progress;
} choices_t;

void choices_init(choices_t *c, options_t *options);
void choices_fread(choices_t *c, FILE *file);
void choices_destroy(choices_t *c);
void choices_add(choices_t *c, const char *choice);
size_t choices_available(choices_t *c);
void choices_search(choices_t *c, const char *search);
const char *choices_get(choices_t *c, size_t n);
score_t choices_getscore(choices_t *c, size_t n);
void choices_prev(choices_t *c);
void choices_next(choices_t *c);

/*
 * Start a delayed search.
 */
void choices_search_start(choices_t *c, const char *search);
/*
 * Perform a step of the delayed search.
 * Returns 1 if the search is complete, 0 otherwise.
 */
int choices_search_step(choices_t *c, size_t batch_size);
/*
 * Check if the search is complete.
 * Returns 1 if the search is complete, 0 otherwise.
 */
int choices_is_search_complete(choices_t *c);

#endif
