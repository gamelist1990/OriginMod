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

---

## hackerdetector.json

他プレイヤーの異常な移動/回転を検知して通知する機能（`hackerdetect`）の設定です。

### 設定項目

- `messages`: 通知先
  - `client`: 自分のクライアントにのみ表示（推奨）
  - `server`: サーバーチャットに送信（周囲に見える可能性があります）
- `speedCheck` (true/false): 速度チェック
- `derpCheck` (true/false): 回転（pitch/yaw）の不正値チェック
- `flyCheck` (true/false): 飛行っぽい挙動チェック（誤検知しやすいので既定はfalse）
- `strictness` (0-5): 厳しさ（大きいほど厳しめ）
- `cooldownMs`: 通知のクールダウン（スパム防止）
- `minFailsToReport`: 連続で何回失敗したら通知するか

### 使用方法

1. このファイルをModのconfigディレクトリ（通常は `plugins/OriginMod/config/`）にコピーしてください
2. `-config reload hackerdetector.json` を実行するか、Modを再起動してください