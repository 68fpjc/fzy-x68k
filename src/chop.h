/**
 * @brief 出力コンテキスト
 */
typedef struct chop_output_context {
  void (*output_func)(int, struct chop_output_context*);  // 出力関数ポインタ
  char* ptr;  // バッファポインタ（呼び出し側で管理）
} chop_output_context;

/**
 * @brief 文字列を指定された幅でクリップして出力する
 * @param s 文字列
 * @param col 表示開始桁位置 (0 ～)
 * @param width 幅
 * @param ctx 出力コンテキスト
 * @return 実際に出力した幅
 *
 * @example
 * // 標準出力への出力
 * void stdout_output(int c, chop_output_context* ctx) {
 *   (void)ctx;
 *   putchar(c);
 * }
 * chop_output_context ctx = {stdout_output, NULL};
 * int width = chop("Hello", 0, 10, &ctx);
 *
 * @example
 * // バッファへの出力
 * void buffer_output(int c, chop_output_context* ctx) {
 *   if (ctx->ptr) {
 *     *ctx->ptr++ = (char)c;
 *   }
 * }
 * char buffer[256];
 * chop_output_context ctx = {buffer_output, buffer};
 * int width = chop("Hello", 0, 10, &ctx);
 * // ctx.ptr はバッファ内の次の書き込み位置
 */
int chop(const char* s, const int col, const int width,
         chop_output_context* ctx);
