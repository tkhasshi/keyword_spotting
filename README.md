# Keyword_spotting
音声認識エンジンのJuliusと音声合成ツールのOpenJTalkを用いた、聴覚支援システムです。

list.txtで音素を設定しておくと、音声認識文の中でその音素を含む単語を抜き出して表示します。


音声ファイルの音声認識を行う場合は
sh file_run.sh
を実行して、音声ファイル(***.wav)を入力してください。

マイク入力の音声認識を行う場合は
sh run.sh
を実行してください。

OpenJTalkは、jtalkdllをお借りしています。
下記のページでダウンロードできます。

https://github.com/rosmarinus/jtalkdll

Juliusで使用している音響モデルと言語モデルは、以下のサイトからダウンロードをお願いします。
このシステムで用いているのは、ディクテーションキット(dictation-kit)のモデル(model)です。

https://julius.osdn.jp/index.php?q=dictation-kit.html

開発環境

Mac Book Pro(macOS Catalina バージョン 10.15.3)
