# fzy for X68000

## これはなに？

ファジーファインダー [fzy](https://github.com/jhawthorn/fzy) を X68000 + Human68k に移植したものです。

## issues

気が向いたら直します。

- 非力な X68000 でそれなりに動くようにしたものの、重いときは重い
- 画面描画が汚い
- 長いテキストを渡すと画面が崩れる
- Shift_JIS 対応が不完全な気がする
- X68000 の 2 バイト半角には非対応
- その他、いろいろ

## ソースコードからのビルド

X68000 ではビルドできません。 [elf2x68k](https://github.com/yunkya2/elf2x68k) が必要です。 Makefile のあるディレクトリで `make` してください。

## 参考文献

- [ぷにぐらま～ずまにゅある](https://github.com/jhawthorn/fzy)

## 連絡先

https://github.com/68fpjc/fzy-x68k
