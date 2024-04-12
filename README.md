# このリポジトリについて

2023年（23代、24代）WASAのパイロット用スマホアプリです
主な特徴として、

* Arduino IDEと同じ感覚で開発できるよう、全てC++でプログラムを記述。Javaのプログラムは一切なし
* 通信はHTTPを利用し、データはJSONとしてやりとりしている
* とにかく複雑な構造を避けている（軌跡の部分だけ少し複雑だけど...）

といった感じになっています。ちなみに、地図はネットにつなげているわけではなく、大利根、富士川、琵琶湖だけの画像をアプリに内蔵して描画しています。

# 作成者
* [23代電装班長　水本幸希](https://github.com/21km43)
* （24代電装班員）
* （24代電装班員）

# 使用したライブラリ
* [ＤＸライブラリ（描画系）](https://dxlib.xsrv.jp/index.html)
* [Json for Modern C++（JSON管理用）](https://github.com/nlohmann/json)
* [cpp-httplib（HTTP通信用）](https://github.com/yhirose/cpp-httplib)

# なにかあれば
このリポジトリから連絡いただいても大丈夫ですが、班長の[Twitter](https://twitter.com/21km43)にDMを送ってくれるとすぐに対応できると思います。
