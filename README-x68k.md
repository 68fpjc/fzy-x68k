# fzy for X68000

## これはなに？

ファジーファインダー [fzy](https://github.com/jhawthorn/fzy) を X68000 + Human68k に移植したものです。

## X68000 版について

```
Usage: fzy [OPTION]...
 -l, --lines=LINES        Specify how many lines of results to show (default 10)
 -p, --prompt=PROMPT      Input prompt (default '> ')
 -q, --query=QUERY        Use QUERY as the initial search string
 -e, --show-matches=QUERY Output the sorted matches of QUERY
 -t, --tty=TTY            Specify file to use as TTY device (default /dev/tty)
 -s, --show-scores        Show the scores of each match
 -j, --workers NUM        Use NUM workers for searching. (default is # of CPUs)
 -H, --highlight          Highlight matching characters
 -n, --no-highlight       Do not highlight matching characters (default)
 -h, --help     Display this help and exit
 -v, --version  Output version information and exit
```

- コマンドラインオプション `-h` と `-H` を追加しました :
  - オリジナルの fzy は一致した文字をハイライト表示しますが、 `-h` で抑制します
  - ハイライト表示は非常に重いため、 X68000 版のデフォルトは `-h` です
- 下記コマンドラインオプションは機能しません。指定しても無視されます :
  - `-t`
  - `-j`

## issues

気が向いたら直します。

- 非力な X68000 でそれなりに動くようにしたものの、重いときは重い
- 長いテキストを渡すと画面が崩れる
- condrv のバックログが汚れる
- Shift_JIS 対応が不完全な気がする
- X68000 の 2 バイト半角には非対応
- その他、いろいろ

## ソースコードからのビルド

X68000 ではビルドできません。 [elf2x68k](https://github.com/yunkya2/elf2x68k) が必要です。 Makefile のあるディレクトリで `make` してください。

## 参考文献

- [ぷにぐらま～ずまにゅある](https://github.com/jhawthorn/fzy)

## 連絡先

https://github.com/68fpjc/fzy-x68k
