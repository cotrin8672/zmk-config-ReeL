# ReeL ZMK：実機PCへの引き継ぎ

対象は、RMKで使っていた左右のXIAO nRF52840と、工場出荷時のUSB DFUを使うRaytac MDBT50Q-CX-40（USB-C）。キーマップはRMK mainの既定値。Vialの実機設定は未移植。2026-11-15に起動位置を直したUF2で起動し、続いてcb74b90のドングルを書き込み、通常入力とAMLを使えることをユーザーが確認した。

## 分かったこと

- 旧ビルド 979e9df の左右UF2はアプリ開始が 0x27000。移行元の [RMK memory.x](https://github.com/cotrin8672/rmk-config-ReeL/blob/45bbb9d688d6e555129441cf5bf2af7871a0a07f/memory.x) は 0x1000。ボールの調整より先に起動位置を合わせる必要がある。実機のブートローダーが実際にどちらへジャンプしたかは未観測。
- 修正版は公式ZMKの XIAO ボードと nrf52840-nosd snippet、nice_view、ZephyrのPMW3610・エンコーダードライバーを使用。RaytacにはこのZephyr版に同型番のボード定義がないため、基板・フラッシュ配置のみリポジトリ内で宣言。外部のRaytac/PMW3610モジュールは使わない。
- 旧nice_view標準のPeripheral画面はUSB給電中に充電マークを表示する。LCD復元版はP0.17の実際の充電信号を表示する。SharpのメモリLCDは画面が残ることもあり、アイコン単独では起動・停止を区別できない。

## LCD復元版

RMKの縦型68×160の画像・数字をLVGL 9の画像とフォントへ変換し、ZMKのカスタムステータス画面に移植した。SPIやVCOMの処理は公式nice_viewとZephyrのls0xxドライバーを使う。9704413版はLVGL内部の比較を通ったが、実機で白黒反転が報告された。修正版では標準のnice_view反転設定を使用し、実際のZephyr白黒変換を通したLCD向け送信データを検証する。

左右のLCDには各側の電池・充電・ドングル接続状態を表示し、レイヤーはドングルから既存ZMK split通信で受信する。USBドングル運用では使わないLCD下部のBluetoothアイコンとプロファイル欄は削除。同期前と切断時は選択マークを消す。9704413版のドングルとは互換性があり、白黒反転と表示削除の修正は左右UF2の更新で反映される。配布物のreel-lcd-preview.pngで画面例を確認できる。

## 実機PCで試す順序

同じ成功ビルドの **reel-zmk-firmware** を展開する。古い Downloads の 979e9df とは混ぜない。左右用はこのReeLのRMKからの移行を対象としており、SoftDeviceを使う別のキーボードには書かない。Vialがまだ開ける場合は先にレイアウトを保存する。

1. **Raytacだけを確認。** 左右の電源を切り、ドングルをボタンを押さずにPCに挿す。デバイスマネージャーを開き、抜き差しで増減するUSB複合デバイス／HIDキーボードを確認する。表示名が「ReeL」でなくてもよい。以前BluetoothペアリングしたReeLは判断に使わない。
2. **ドングルを更新。** [Nordic nRF Util](https://www.nordicsemi.com/Products/Development-tools/nRF-Util) を用意し、PowerShellで下記を実行。ボタンを押しながら挿し、赤LEDの明滅と新しく出たCOM番号を確認。COM5は例。ZIPは展開しない。完了後、通常挿入で手順1のUSB認識を再確認する。DFUでCOMは出るが通常挿入でHIDが出ない場合は、左右のペアリングよりドングルの起動を先に調べる。

   ```powershell
   nrfutil install nrf5sdk-tools
   nrfutil nrf5sdk-tools dfu usb-serial -pkg .\reel_dongle.zip -p COM5
   ```

3. **左だけ。** 左XIAOをデータ対応USBケーブルで接続し、リセットを素早く2回押す。現れたドライブの INFO_UF2.TXT を控える。reel_left.uf2 をコピーし、転送・再起動後にUSBを外し、電池で起動。ドングルをPCに挿したまま、メモ帳で左のQを試す。LCDの充電・接続アイコンの変化も控える。電池で起動しない場合はUSB給電でも試す。
4. **右だけ、次に両方。** 同様に右XIAOへ reel_right.uf2 をコピー。左を切って右のP、ボールを試す。最後に両方を入れ、左右同時のキー・エンコーダー・スリープ復帰を確認する。左右のUSB端子は通常のキー出力先ではない。ホストで左右を個別にBluetoothペアリングする必要はない。
5. UF2ドライブが出ない場合はケーブル・ポートとダブルリセットを再確認。通常HIDは出るのに左右とも入力しない場合は、まず左右のUF2／起動位置と既存ZMKのsplit bondを切り分ける。初手でフラッシュ全消去やブートローダー交換はしない。元に戻す場合は、最後に動作した **左右一組のRMK UF2** を使う。

## 次の担当に渡す記録

使ったActions実行URLまたはコミット、ドングル通常挿入時のデバイス名とハードウェアID、DFU時のCOMとコマンドの終了表示、左右それぞれのINFO_UF2.TXT、コピー後にドライブが消えたか、USB給電／電池それぞれでのLCDとQ/P入力の結果。CIはファイル生成の証拠で、これらの実機結果は別に記録する。
