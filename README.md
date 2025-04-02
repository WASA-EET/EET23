# このリポジトリについて

WASA（23代～）のパイロット＋追走者用スマホアプリ。鳥人間コンテスト2024で使用

# Features
* C++
* HTTP通信でマイコンからデータを取得（データ形式はJSON）
* 機速、高度、ペラ回転数、飛行距離（琵琶湖のみ）を表示
* 地図表示（現在地、軌跡付き）。現状は琵琶湖、富士川滑空場、大利根飛行場に対応
* 内部ストレージにログをcsvとして保存（要書き込み権限）
* GitHub Actionでpush時にapkを生成

# 操作方法
画面を1秒以上長押しすると、その時触れていた指の数に応じて次の処理が行われます
* 1本→飛行経路の軌跡を削除
* 2本→ログ記録開始（または終了）。同時に現在の姿勢角を基準角とする
* 3本→マップ変更

# Libraries
* [ＤＸライブラリ（描画系）](https://dxlib.xsrv.jp/index.html)
* [Json for Modern C++（JSON管理用）](https://github.com/nlohmann/json)
* [cpp-httplib（HTTP通信用）](https://github.com/yhirose/cpp-httplib)

# Screenshot
※ パラメータは適当です
![Screenshot](/screenshot.bmp)

# 作成者について
* 水本幸希（23代電装班長）
  * [GitHub](https://github.com/21km43)
  * [Twitter](https://twitter.com/21km43)

# Bugs
* 琵琶湖プラットホーム周辺では電波が混雑しているため、ボート上の追走者はマイコンと通信できない模様。3km過ぎたあたりから通信できるようになった。
  * LoRaなどの使用を推奨
* 姿勢角の値が時間経過でドリフトする
