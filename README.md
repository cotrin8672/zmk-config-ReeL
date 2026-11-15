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
- 右ボールは1600 CPI、smart-mode有効。ドングル側で速度適応LPF（zmk-trackball-processorsのSpike Filter、強度3）を通し、ZMK標準のXYスケーラーで移動量を3/8倍にする（約600 CPI相当）。LPFは全レイヤー共通で、Response Curveは無効。通常キーでAMLを解除し、タイピング後500 msは再突入しない。J/K/Lと手前のマウスボタン2個は解除対象外。ボール入力が5秒途切れると通常配列に戻る。L1/L2ではポインターだけが動き、AMLを起動しない。標準AMLは移動量のしきい値を持たない。
- 左右の LCD は RMK の縦型ステータス画面。各側の電池残量・充電状態・ドングルとの接続と、ドングルから受信したレイヤーを表示する。USBドングル運用では使わないホスト用Bluetooth欄は削除。切断中・状態受信前はレイヤー選択マークを消す。片側の電源再投入とドングルの抜き差しで表示と入力が再開すること。

トラックボールの非直交校正・設定保存、Vial 通信、独自のエンコーダー判定は含みません。ビルドの成功は実機での動作確認を意味しません。

## ZMK Studio

Studioの接続先は、USBでPCにつなぐRaytacドングル（Central）です。左右のXIAOから届くキー入力をドングルで処理するため、左右をUSBで接続しなくても4レイヤーの割り当てを編集できます。

1. Studio対応版のドングルを書き込み、ボタンを押さず通常起動する。
2. ChromeまたはEdgeで [ZMK Studio](https://zmk.studio/) を開き、USB接続からドングルのシリアルポートを選ぶ。通常起動時のStudio用COMと、書き込み時のDFU用COMは別の機能です。
3. **Enterを長押ししてL2に入り、物理QとPを同時押し**すると編集ロックが解除される。既定配列ではL2のF1とF10の位置に相当します。
4. キー割り当てを編集し、Studioの保存操作でドングルに保存する。

41キーの座標は元のVial実配置から定義しています。Studioはドングルだけで有効にし、ロックは切断・アイドル時に再びかかります。保存後は、ファームの標準キーマップよりStudioで保存した配列が優先されます。標準配列に戻す場合はStudioの「Restore Stock Settings」を使ってください。CPI・smart-mode・AMLのタイマーはファーム設定です。

## LCDの移植

元の68×160の画像と数字をLVGL 9の画像・フォントに変換し、ZMKのカスタムステータス画面として実装しています。描画更新にはZMKの表示用ワークキューを使用し、横160×縦68のパネルへの回転は公式nice_viewと同じLVGLのAPIを使います。SPI、反転信号（VCOM）、画面転送はZephyrのls0xxドライバーに任せています。

左右ともPeripheralなので、レイヤーはCentralのドングルからZMKの既存のリモートbehavior経路で送ります。L0/L1/L2/L3のラベルは元画像のBASE/NUMBER/SYMBOL/MOUSEです。充電マークはXIAOのP0.17の実際の充電状態をZephyrのGPIO入力ドライバー経由で取得します。

このZephyr版の白黒変換に合わせ、左右は標準のCONFIG_NICE_VIEW_WIDGET_INVERTEDを有効にし、カスタム画面も同じパレット設定に従います。検証はLVGL内部の画像だけでなく、実際のZephyr変換を通したls0xx向けの送信データまで確認します。

大きな静止画像は、フラッシュ上のLVGL A8画像を直接描画します。A1画像を使うと、このLVGL版では一時展開に11523バイト必要となり、実機の8192バイトの表示用ヒープに収まりません。描画テストも同じ8KB制限を設け、画像だけが消える不具合を検出します。

画像の出典・再生成は [display/assets/README.md](display/assets/README.md)、描画の検証手順は [tests/display/README.md](tests/display/README.md) を参照してください。LCD復元版では、同期処理を持つドングルと画面を持つ左右の3台を同じ成功ビルドに更新してください。
