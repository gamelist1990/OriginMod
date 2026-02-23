# OriginMod 設定ファイル

このフォルダにはOriginModの設定ファイルのサンプルが含まれています。

## messages.json

KillGGとAutoGG機能のメッセージをカスタマイズできます。

### 設定項目

#### killgg セクション
- `enabled`: KillGG機能の有効/無効 (true/false)
- `cooldown`: メッセージ送信のクールダウン時間（ミリ秒）
- `messages`: キル時に送信されるメッセージのリスト
  - `{}` は対戦相手の名前に置き換えられます

#### autogg セクション
- `enabled`: AutoGG機能の有効/無効 (true/false)
- `minDelay`: ゲーム終了後の最小遅延時間（ミリ秒）
- `maxDelay`: ゲーム終了後の最大遅延時間（ミリ秒）
- `messages`: ゲーム終了時に送信されるメッセージのリスト

### 使用方法

1. このファイルをModのconfigディレクトリ（通常は `plugins/OriginMod/config/`）にコピーしてください
2. 設定を変更してゲーム内で `/config reload` コマンドを実行するか、Modを再起動してください
3. 設定が正しく読み込まれない場合、Modのログを確認してください

### 注意事項

- JSONの構文エラーがあると設定ファイルが読み込まれません
- メッセージが空の場合、その機能は無効化されます
- クールダウン時間は正の整数で指定してください