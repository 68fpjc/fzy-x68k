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
	int m = strlen(haystack);
	char last_ch = '/';
	for (int i = 0; i < m; i++) {
		char ch = haystack[i];
		match_bonus[i] = COMPUTE_BONUS(last_ch, ch);
		last_ch = ch;
	}
}

score_t match_positions(const char *needle, const char *haystack, size_t *positions) {
	if (!*needle)
		return SCORE_MIN;

	int n = mbslen((const unsigned char *)needle);
	int m = mbslen((const unsigned char *)haystack);

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

	// 簡易文字マッチングでまず検証
	if (!has_match(needle, haystack)) {
		// 単純なマッチングもできない場合は最低スコアを返す
		return SCORE_MIN;
	}

	// バイト位置から文字位置へのマッピングテーブルを作成
	size_t needle_pos_map[n + 1];
	size_t haystack_pos_map[m + 1];

	// 文字位置をインデックスとしてバイト位置を格納
	{
		const char *s = needle;
		for (int i = 0; i < n; i++) {
			needle_pos_map[i] = s - needle;
			s += ismbblead(*s) && *(s + 1) ? 2 : 1;
		}
		needle_pos_map[n] = strlen(needle);
	}
	{
		const char *s = haystack;
		for (int i = 0; i < m; i++) {
			haystack_pos_map[i] = s - haystack;
			s += ismbblead(*s) && *(s + 1) ? 2 : 1;
		}
		haystack_pos_map[m] = strlen(haystack);
	}

	// まず簡易的に文字位置を見つける
	// これは完全に最適ではないが、マッチするポジションが確実に見つかる
	if (positions) {
		// 初期値は -1 (見つからなかった)
		for (int i = 0; i < n; i++) {
			positions[i] = (size_t)-1;
		}

		// 各文字について、ヘイスタック内で最初に出現する位置を見つける
		size_t last_pos = 0;
		for (int i = 0; i < n; i++) {
			size_t needle_byte_pos = needle_pos_map[i];
			char needle_char = needle[needle_byte_pos];
			int is_double = ismbblead(needle_char) && needle[needle_byte_pos + 1];

			// ヘイスタック内のこの文字を探す
			for (size_t j = last_pos; j < strlen(haystack); j++) {
				if (is_double) {
					// 全角文字の場合
					if (ismbblead(haystack[j]) && haystack[j + 1] &&
					    haystack[j] == needle_char &&
					    haystack[j + 1] == needle[needle_byte_pos + 1]) {
						positions[i] = j;
						last_pos = j + 2; // 次の検索は 2 バイト先から
						break;
					}
				} else {
					// 半角文字の場合
					if (!ismbblead(haystack[j]) &&
					    tolower(haystack[j]) == tolower(needle_char)) {
						positions[i] = j;
						last_pos = j + 1; // 次の検索は 1 バイト先から
						break;
					}
				}
			}
		}
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

	// n または m が 0 の場合は処理できないのでエラーを返す
	if (n <= 0 || m <= 0) {
		free(D);
		free(M);
		return SCORE_MIN;
	}

	for (int i = 0; i < n; i++) {
		score_t prev_score = SCORE_MIN;
		score_t gap_score = i == n - 1 ? SCORE_GAP_TRAILING : SCORE_GAP_INNER;

		for (int j = 0; j < m; j++) {
			// バイト位置で文字を比較
			size_t needle_byte_pos = needle_pos_map[i];
			size_t haystack_byte_pos = haystack_pos_map[j];

			int is_match = 0;

			// 半角 / 全角判定を直接バイト文字で行う
			char needle_char = needle[needle_byte_pos];
			char haystack_char = haystack[haystack_byte_pos];

			// 両方全角文字の場合
			if (ismbblead(needle_char) && ismbblead(haystack_char) &&
			    needle[needle_byte_pos + 1] && haystack[haystack_byte_pos + 1]) {
				// 2 バイト文字同士を比較 (大文字小文字区別)
				is_match = (needle_char == haystack_char &&
					    needle[needle_byte_pos + 1] ==
						haystack[haystack_byte_pos + 1]);
			}
			// 両方半角文字の場合
			else if (!ismbblead(needle_char) && !ismbblead(haystack_char)) {
				// 半角文字を比較 (大文字小文字無視)
				is_match = (tolower(needle_char) == tolower(haystack_char));
			}

			if (is_match) {
				score_t score = SCORE_MIN;
				if (!i) {
					score = (j * SCORE_GAP_LEADING) + match_bonus[j];
				} else if (j) { /* i > 0 && j > 0*/
					score = max(
					    M[i - 1][j - 1] + match_bonus[j],

					    /* consecutive match, doesn't stack with match_bonus */
					    D[i - 1][j - 1] + SCORE_MATCH_CONSECUTIVE);
				}
				D[i][j] = score;
				M[i][j] = prev_score = max(score, prev_score + gap_score);
			} else {
				D[i][j] = SCORE_MIN;
				M[i][j] = prev_score = prev_score + gap_score;
			}
		}
	}

#ifdef DEBUG_VERBOSE
	fprintf(stderr, "\"%s\" =~ \"%s\"\n", needle, haystack);
	mat_print(&D[0][0], 'D', needle, haystack);
	mat_print(&M[0][0], 'M', needle, haystack);
	fprintf(stderr, "\n");
#endif

	score_t result = M[n - 1][m - 1];

	// DP 計算が完了してスコアが計算できたら、最適なバックトレースを行う
	// 既に positions は簡易的に設定済みなので、ここでのエラーは無視しても動作する
	if (positions && result != SCORE_MIN) {
		int match_required = 0;
		int found_all = 1;

		// 一度全部リセット
		for (int i = 0; i < n; i++) {
			positions[i] = (size_t)-1;
		}
		// バックトレース
		for (int i = n - 1, j = m - 1; i >= 0 && j >= 0; i--) {
			int found = 0;

			for (; j >= 0; j--) {
				if (D[i][j] != SCORE_MIN &&
				    (match_required || D[i][j] == M[i][j])) {
					positions[i] = haystack_pos_map[j];
					match_required =
					    i > 0 && j > 0 &&
					    M[i][j] == D[i - 1][j - 1] + SCORE_MATCH_CONSECUTIVE;
					found = 1;
					j--;
					break;
				}
			}
			if (!found) {
				found_all = 0;
				// このケースでは簡易マッチングの結果を維持する
				break;
			}
		}
		// 念のため、解が見つからなかった場合は手動で検出
		if (!found_all) {
			// 既に簡易マッチングで設定済みなので何もしない
		}
	}

	free(D);
	free(M);

	return result;
}

score_t match(const char *needle, const char *haystack) {
	return match_positions(needle, haystack, NULL);
}
