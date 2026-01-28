#include <stdio.h>

#include "chop.h"

/**
 * @brief 半角文字の処理
 * @param c 文字
 * @param ctx 出力コンテキスト
 * @return 出力した幅
 */
static int process_hankaku(const unsigned char c, chop_output_context* ctx) {
  int ret = 0;
  if (c < 0x20 || c == 0x7F) {
    // Control characters are not printed
  } else {
    // Print ASCII character
    ctx->output_func(c, ctx);
    ret = 1;
  }
  return ret;
}

/**
 * @brief 全角文字の処理
 * @param c1 1 バイト目
 * @param c2 2 バイト目
 * @param ctx 出力コンテキスト
 * @return 出力した幅
 */
static int process_zenkaku(const unsigned char c1, const unsigned char c2,
                           chop_output_context* ctx) {
  // For simplicity, just print the two bytes representing the Zenkaku character
  ctx->output_func(c1, ctx);
  ctx->output_func(c2, ctx);
  return 2;  // Zenkaku characters occupy width of 2
}

/**
 * @brief 2 バイト半角文字の処理
 * @param c1 1 バイト目
 * @param c2 2 バイト目
 * @param ctx 出力コンテキスト
 * @return 出力した幅
 */
static int process_two_bytes_hankaku(const unsigned char c1,
                                     const unsigned char c2,
                                     chop_output_context* ctx) {
  ctx->output_func(c1, ctx);
  ctx->output_func(c2, ctx);
  return 1;  // Treat as hankaku width of 1
}

int chop(const char* s, const int col, const int width,
         chop_output_context* ctx) {
  int current_width = 0;
  int effective_width = width - col;
  unsigned char c;
  const char* s_tmp = s;

  while ((c = (unsigned char)*s_tmp++) != '\0') {
    if (current_width >= effective_width) {
      break;
    }
    if (c == '\t') {
      int spaces = 8 - (current_width % 8);
      if (current_width + spaces > width) {
        break;  // Not enough space for tab expansion
      }
      ctx->output_func(c, ctx);
      current_width += spaces;
    } else if ((c < 0x80) || ((c >= 0xA0) && (c <= 0xDF))) {
      current_width += process_hankaku(c, ctx);
    } else {
      unsigned char c2 = (unsigned char)*s_tmp++;
      if (!c2) {
        break;  // Prevent reading beyond the string
      }
      if ((c == 0x80) || (c >= 0xf0)) {
        current_width += process_two_bytes_hankaku(c, c2, ctx);
      } else {
        if (current_width + 2 > width) {
          break;  // Not enough space for a zenkaku character
        }
        current_width += process_zenkaku(c, c2, ctx);
      }
    }
  }
  //   {
  //     ctx->output_func('\n', ctx);
  //     for (int i = 0; i < effective_width; i++) {
  //       ctx->output_func("1234567890"[i % 10], ctx);
  //     }
  //     ctx->output_func('\n', ctx);
  //   }

  return current_width;
}
