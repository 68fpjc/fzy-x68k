#include "cp932.h"
#include <stdlib.h>
#include <string.h>

int is_cp932_lead_byte(const char c) {
	unsigned char uc = (unsigned char)c;
	return (uc >= 0x81 && uc <= 0x9F) || (uc >= 0xE0 && uc <= 0xFC);
}

int is_print_cp932(const short wc) {
	char wc_high = wc >> 8;
	return wc_high ? is_cp932_lead_byte(wc_high) : wc >= 0x20;
}

size_t cp932_strlen(const char *s) {
	size_t len = 0;
	while (*s) {
		if (is_cp932_lead_byte(*s) && *(s + 1)) {
			s += 2;  // 全角文字は2バイト進む
		} else {
			s++;    // 半角文字は1バイト進む
		}
		len++;      // どちらの場合も文字数は1つ増やす
	}
	return len;
}
