# このリポジトリについて

WASA（23代～）のパイロット用スマホアプリ

# Features
* Arduino IDEと同じ感覚で開発できるよう、全てC++でプログラムを記述
* 通信はHTTPを利用し、データ形式はJSON
* ログはサーバーにアップロード

といった感じになっています。ちなみに、地図はネットにつなげているわけではなく、大利根、富士川、琵琶湖だけの画像をアプリに内蔵して描画しています。

# Libraries
* [ＤＸライブラリ（描画系）](https://dxlib.xsrv.jp/index.html)
* [Json for Modern C++（JSON管理用）](https://github.com/nlohmann/json)
* [cpp-httplib（HTTP通信用）](https://github.com/yhirose/cpp-httplib)

# 原案者
* [水本幸希（23代電装班長）](https://github.com/21km43)

# Contributer
* （24代電装班員）
* （24代電装班員）

# Bugs
* マイコンと接続時、アプリが稀に落ちる（Jsonか通信の問題？）
  * 一定時間毎に自動でアプリを起動するマクロタスクを常駐させることで対応中

# Any questions
このリポジトリから連絡いただいても大丈夫ですが、原案者の[Twitter](https://twitter.com/21km43)にDMを送ってくれるとすぐに対応できると思います。
