# ReeL ZMK

Raytac MDBT50Q-CX-40 が USB 接続の Central になり、左右の Seeed XIAO nRF52840 が BLE で接続します。左にはエンコーダー、右には PMW3610、両側には beekeeb の Sharp LS011B7DH03（nice_view）があります。PC に左右をペアリングする必要はありません。

[RMK main の既定配列](https://github.com/cotrin8672/rmk-config-ReeL/blob/45bbb9d688d6e555129441cf5bf2af7871a0a07f/src/keymap.rs)を [config/reel.keymap](config/reel.keymap) に移植しました。4レイヤーとも RMK の行・列順で、実装のない3位置を省略しています。Vial から実機に保存した変更は含まれていません。ハードウェアの基準は [ReeL b9bacc7](https://github.com/cotrin8672/ReeL/tree/b9bacc7681b5a1461f23b08cf7a2b1b867c73b0e/hardware/pcb) です。

## 書き込み

[Build ReeL ZMK firmware](https://github.com/cotrin8672/zmk-config-ReeL/actions/workflows/build.yml) の成功した同じ実行から **reel-zmk-firmware** をダウンロードし、外側のアーカイブだけを展開します。

| ファイル | 対象 | 手順 |
| --- | --- | --- |
| reel_left.uf2 | 左 XIAO | USB 接続しリセットを2回押す。現れたドライブにコピー |
| reel_right.uf2 | 右 XIAO | 同上 |
| reel_dongle.zip | Raytac CX-40 | ZIP を展開せず、工場ブートローダーに USB DFU で書き込む |

Windows のドングルには [Nordic nRF Util](https://www.nordicsemi.com/Products/Development-tools/nRF-Util) を使います。ドングルのボタンを押したまま USB に挿し、ブートローダーの COM 番号をデバイスマネージャーで確認します。以下の COM5 は実際の番号に置き換えてください。

```powershell
nrfutil install nrf5sdk-tools
nrfutil nrf5sdk-tools dfu usb-serial -pkg reel_dongle.zip -p COM5
```

ドングルを PC に接続し、左右を起動します。初回は左右をドングルの近くに置いて split 接続を待ちます。この構成ではドングルが必要です。元に戻す場合は左右とも対応する RMK UF2 を書き戻します。事前に必要な Vial 設定を export してください。

## 動作確認

- 全41キー、左右をまたぐ修飾キー、Space/Enter の長押しで L1/L2 に入ること。
- 左エンコーダーは通常時に縦スクロール、L2 で横スクロール、左 GUI＋縦回転で音量。1クリックずつ両方向で確認。
- 右ボールは1600 CPI固定。縦横の向き、移動後の J/K/L（左・中・右クリック）、5秒後の通常配列への復帰を確認。
- 左右の LCD が標準の Peripheral 画面を表示すること。片側の電源再投入とドングルの抜き差しで入力が再開すること。

トラックボールの非直交校正・設定保存、Vial 通信、独自のエンコーダー判定、RMK の LCD 画面は含みません。ビルドの成功は実機での動作確認を意味しません。
