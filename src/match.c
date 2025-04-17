#include <ctype.h>
#include <float.h>
#include <math.h>
#include <mbctype.h>
#include <mbstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "bonus.h"
#include "match.h"

#include "../config.h"

unsigned char *strcasechr(const unsigned char *s, int c) {
	if (!_MBIS16(c)) {
		int upper = toupper(c);
		int mbc = mbbtombc(c);
		int mbc_upper = mbbtombc(upper);
		int nch;
		while ((nch = mbsnextc(s)) != 0) {
			if (nch == c || nch == upper || nch == mbc || nch == mbc_upper) {
				return (unsigned char *)s;
			}
			s = mbsinc((unsigned char *)s);
		}
		return NULL;
	} else {
		return mbschr(s, c);
	}
}

static int has_match_internal(const unsigned char *needle, const unsigned char *haystack) {
	int nch;
	while ((nch = mbsnextc(needle))) {
		needle = mbsinc((unsigned char *)needle);
		if (!(haystack = strcasechr(haystack, nch))) {
			return 0;
		}
		haystack = mbsinc((unsigned char *)haystack);
	}
	return 1;
}

int has_match(const char *needle, const char *haystack) {
	return has_match_internal((const unsigned char *)needle, (const unsigned char *)haystack);
}

#define max(a, b) (((a) > (b)) ? (a) : (b))

#ifdef DEBUG_VERBOSE
/* print one of the internal matrices */
void mat_print(score_t *mat, char name, const char *needle, const char *haystack) {
	int n = strlen(needle);
	int m = strlen(haystack);
	int i, j;
	fprintf(stderr, "%c   ", name);
	for (j = 0; j < m; j++) {
		fprintf(stderr, "     %c", haystack[j]);
	}
	fprintf(stderr, "\n");
	for (i = 0; i < n; i++) {
		fprintf(stderr, " %c |", needle[i]);
		for (j = 0; j < m; j++) {
			score_t val = mat[i * m + j];
			if (val == SCORE_MIN) {
				fprintf(stderr, "    -\u221E");
			} else {
				fprintf(stderr, " %.3f", val);
			}
		}
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "\n\n");
}
#endif

static void precompute_bonus(const char *haystack, score_t *match_bonus) {
	/* Which positions are beginning of words */
	unsigned char *tmp_haystack = (unsigned char *)haystack;
	size_t m = mbslen(tmp_haystack);
	int last_ch = '/';
	for (size_t i = 0; i < m; i++) {
		int ch = mbsnextc(tmp_haystack);
		match_bonus[i] = !_MBIS16(ch) ? COMPUTE_BONUS(last_ch, ch) : 0;
		last_ch = ch;
		tmp_haystack = mbsinc(tmp_haystack);
	}
}

score_t match_positions(const char *needle, const char *haystack, size_t *positions) {
	if (!*needle)
		return SCORE_MIN;

	size_t n = mbslen((const unsigned char *)needle);
	size_t m = mbslen((const unsigned char *)haystack);

	if (n <= 0 || m <= 0) {
		return SCORE_MIN;
	}

	if (n == m) {
		/* Since this method can only be called with a haystack which
		 * matches needle. If the lengths of the strings are equal the
		 * strings themselves must also be equal (ignoring case).
		 */
		if (positions)
			for (size_t i = 0; i < n; i++)
				positions[i] = i;
		return SCORE_MAX;
	}

	if (m > 1024) {
		/*
		 * Unreasonably large candidate: return no score
		 * If it is a valid match it will still be returned, it will
		 * just be ranked below any reasonably sized candidates
		 */
		return SCORE_MIN;
	}

	score_t match_bonus[m];
	score_t(*D)[m] = malloc(sizeof(score_t) * n * m);
	score_t(*M)[m] = malloc(sizeof(score_t) * n * m);
	if (!D || !M) {
		if (D)
			free(D);
		if (M)
			free(M);
		return SCORE_MIN;
	}

	/*
	 * D[][] Stores the best score for this position ending with a match.
	 * M[][] Stores the best possible score at this position.
	 */
	precompute_bonus(haystack, match_bonus);

	{
		unsigned char *tmp_needle = (unsigned char *)needle;

		for (size_t i = 0; i < n; i++) {
			score_t prev_score = SCORE_MIN;
			score_t gap_score = i == n - 1 ? SCORE_GAP_TRAILING : SCORE_GAP_INNER;

			int ch_needle = mbsnextc(tmp_needle);
			unsigned char *tmp_haystack = (unsigned char *)haystack;

			for (size_t j = 0; j < m; j++) {
				int ch_haystack = mbsnextc(tmp_haystack);
				int matched = 0;

				if (!_MBIS16(ch_needle)) {
					if (!_MBIS16(ch_haystack) &&
					    tolower(ch_haystack) == ch_needle) {
						matched = 1;
					}
				} else {
					if (ch_haystack == ch_needle) {
						matched = 1;
					}
				}
				if (matched) {
					score_t score = SCORE_MIN;
					if (!i) {
						score = (j * SCORE_GAP_LEADING) + match_bonus[j];
					} else if (j) { /* i > 0 && j > 0*/
						score =
						    max(M[i - 1][j - 1] + match_bonus[j],

							/* consecutive match, doesn't stack
							   with match_bonus */
							D[i - 1][j - 1] + SCORE_MATCH_CONSECUTIVE);
					}
					D[i][j] = score;
					M[i][j] = prev_score = max(score, prev_score + gap_score);
				} else {
					D[i][j] = SCORE_MIN;
					M[i][j] = prev_score = prev_score + gap_score;
				}
				tmp_haystack = mbsinc(tmp_haystack);
			}
			tmp_needle = mbsinc(tmp_needle);
		}
	}

#ifdef DEBUG_VERBOSE
	fprintf(stderr, "\"%s\" =~ \"%s\"\n", needle, haystack);
	mat_print(&D[0][0], 'D', needle, haystack);
	mat_print(&M[0][0], 'M', needle, haystack);
	fprintf(stderr, "\n");
#endif

	score_t result = M[n - 1][m - 1];

	/* backtrace to find the positions of optimal matching */
	if (positions) {
		int match_required = 0;
		for (int i = n - 1, j = m - 1; i >= 0; i--) {
			for (; j >= 0; j--) {
				/*
				 * There may be multiple paths which result in
				 * the optimal weight.
				 *
				 * For simplicity, we will pick the first one
				 * we encounter, the latest in the candidate
				 * string.
				 */
				if (D[i][j] != SCORE_MIN &&
				    (match_required || D[i][j] == M[i][j])) {
					/* If this score was determined using
					 * SCORE_MATCH_CONSECUTIVE, the
					 * previous character MUST be a match
					 */
					match_required =
					    i && j &&
					    M[i][j] == D[i - 1][j - 1] + SCORE_MATCH_CONSECUTIVE;
					positions[i] = j--;
					break;
				}
			}
		}
	}

	free(D);
	free(M);

	return result;
}

score_t match(const char *needle, const char *haystack) {
	return match_positions(needle, haystack, NULL);
}

#ifdef NO_CALC_SCORE_OPTION
score_t match_stub(const char *needle, const char *haystack) {
	(void)needle;
	(void)haystack;
	return SCORE_MIN;
}
#endif
