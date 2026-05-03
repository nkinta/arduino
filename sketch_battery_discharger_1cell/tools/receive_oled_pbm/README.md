# receive_oled_pbm

OLED の `PBM(P4)` ダンプをシリアルで受信し、`PNG` に変換してタイムスタンプ付きで保存するツールです。

## 必要環境

- Python 3.9 以降
- シリアルポートにアクセスできること

## セットアップ

```bash
pip install -r tools/receive_oled_pbm/requirements.txt
```

## 使い方

```bash
python tools/receive_oled_pbm/receive_oled_pbm.py COM6
```

例:

```bash
python tools/receive_oled_pbm/receive_oled_pbm.py COM6 --output-dir captures --prefix battery
```

## オプション

- `--baud`
  シリアル通信速度を指定します。デフォルトは `115200` です。

- `--output-dir`
  保存先ディレクトリを指定します。デフォルトは `captures` です。

- `--prefix`
  保存ファイル名の接頭辞を指定します。デフォルトは `oled_capture` です。

- `--timeout`
  シリアル受信タイムアウト秒を指定します。デフォルトは `10` 秒です。

- `--keep-pbm`
  変換前の `pbm` ファイルも一緒に保存します。

## 保存ファイル

保存名は次の形式です。

```text
<prefix>_YYYYMMDD_HHMMSS.png
```

例:

```text
oled_capture_20260503_153045.png
```

## 実行手順

1. このスクリプトを起動します。
2. Arduino 側で `BatteryController` 実行中に `L+R` を同時押しします。
3. 受信した OLED 画像が `PNG` で保存されます。
