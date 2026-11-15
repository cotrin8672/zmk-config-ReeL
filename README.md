# ReeL ZMK

RMKから移行する場合は [実機PC向けの引き継ぎと診断手順](HANDOFF-ja.md) を参照してください。

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
- 右ボールは1600 CPI。ZMK標準のXYスケーラーで移動量を半分にする（約800 CPI相当）。通常キーでAMLを解除し、タイピング後500 msは再突入しない。J/K/Lと手前のマウスボタン2個は解除対象外。ボール入力が5秒途切れると通常配列に戻る。L1/L2ではポインターだけが動き、AMLを起動しない。標準AMLは移動量のしきい値を持たない。
- 左右の LCD は RMK の縦型ステータス画面。各側の電池残量・充電状態・ドングルとの接続を表示し、ドングルから受信したレイヤーとホストのBluetoothプロファイルを表示する。切断中・状態受信前は選択マークを消す。片側の電源再投入とドングルの抜き差しで表示と入力が再開すること。

トラックボールの非直交校正・設定保存、Vial 通信、独自のエンコーダー判定は含みません。ビルドの成功は実機での動作確認を意味しません。

## LCDの移植

元の68×160の画像と数字をLVGL 9の画像・フォントに変換し、ZMKのカスタムステータス画面として実装しています。描画更新にはZMKの表示用ワークキューを使用し、横160×縦68のパネルへの回転は公式nice_viewと同じLVGLのAPIを使います。SPI、反転信号（VCOM）、画面転送はZephyrのls0xxドライバーに任せています。

左右ともPeripheralなので、レイヤーとプロファイルはCentralのドングルからZMKの既存のリモートbehavior経路で送ります。プロファイルの四角はドングル側のホスト用Bluetoothプロファイルを示し、左右のsplit接続先ではありません。L0/L1/L2/L3のラベルは元画像のBASE/NUMBER/SYMBOL/MOUSEです。充電マークはXIAOのP0.17の実際の充電状態をZephyrのGPIO入力ドライバー経由で取得します。

画像の出典・再生成は [display/assets/README.md](display/assets/README.md)、描画の検証手順は [tests/display/README.md](tests/display/README.md) を参照してください。LCD復元版では、同期処理を持つドングルと画面を持つ左右の3台を同じ成功ビルドに更新してください。
