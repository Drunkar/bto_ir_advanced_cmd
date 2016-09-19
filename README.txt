# bto_ir_advanced_cmd

* このリポジトリは、ビット・トレード・ワン USB赤外線リモコンアドバンスのUNIX系環境用コマンドライン操作ツール&GUI操作ツール Ver 1.0.0を配置しただけのものであり、リポジトリ作成者はソースコード開発者ではありません。
* git clone可能な場所に当システムを配置する必要があったため作成したリポジトリです。

# オリジナルのREADME

bto_advanced_USBIR_cmd

ビット・トレード・ワン社提供の、C#言語で記述された
USB赤外線リモコンアドバンス用ライブラリ(USB_IR_Remote_Controller_Advance_Library_v4.1.0.0) を
C言語ソースに移植し、コマンドラインインターフェースを付けたもの。

著作権者:(C) 2015 ビット・トレード・ワン社
ライセンス: ADL(Assembly Desk License)

usage: bto_advanced_USBIR_cmd <option>
  -f <freq>    -dオプションまたは -rオプションを指定した場合のみ使用できます。
               指定しない場合は38000になります。
  -t {AEHA,NEC,SONY,MITSUBISHI}
  -c <code>    家電協(AEHA)フォーマット、NECフォーマット、SONYフォーマット、三菱フォーマットの
               コードデータをこの -cオプション引数として指定します。
               書式は-dオプションと同じ 0xFF,0xFF,... です。必ず-tオプションとセットで指定します。
  -C <Code>    家電協(AEHA)フォーマット、NECフォーマット、SONYフォーマット、三菱フォーマットの
               コードデータをこの -Cオプション引数として指定します。
               書式は0xの付かない16進文字列 FFFF... です。必ず-tオプションとセットで指定します。
  -d <data>    受信設定または送信設定コンフィグレーションツールで、クリップボードに
               コピーボタンでコピーしたデータ、または、-gオプションで取得したデータが、
               この -dオプション引数として使用可能です。
               -fオプションのみ追加で指定可能です。
  -r           受信開始を指令します。
               -fオプションのみ追加で指定可能です。
               受信設定または送信設定コンフィグレーションツールで取得できるデータが、
               本コマンドでも取得できます。
  -s           受信停止を指令します。
  -g           直前に受信を終えたデータが所得できます。
  --Plarail_StopA              このオプションは必ず単独で指定します。
  --Plarail_StopB              このオプションは必ず単独で指定します。
  --Plarail_Speed_UpAF         このオプションは必ず単独で指定します。
  --Plarail_Speed_UpAB         このオプションは必ず単独で指定します。
  --Plarail_Speed_UpBF         このオプションは必ず単独で指定します。
  --Plarail_Speed_UpBB         このオプションは必ず単独で指定します。
  --Plarail_Speed_DownA        このオプションは必ず単独で指定します。
  --Plarail_Speed_DownB        このオプションは必ず単独で指定します。
  --version
※ getopt_longモジュールの制限を回避する為、プラレール用のオプションは末尾まで正確に指定して下さい。

使い方の例
受信系
$ bto_advanced_USBIR_cmd -r         # (生データ)受信開始
$ bto_advanced_USBIR_cmd -s         # (生データ)受信停止
$ bto_advanced_USBIR_cmd -g | tee data.txt  # 生データ所得

送信系
$ bto_advanced_USBIR_cmd -d `cat data.txt`
$ bto_advanced_USBIR_cmd -t AEHA       -C 123456789ABC
$ bto_advanced_USBIR_cmd -t NEC        -C 08F6817E
$ bto_advanced_USBIR_cmd -t SONY       -C 08F6817E
$ bto_advanced_USBIR_cmd -t MITSUBISHI -C 08F6817E
$ bto_advanced_USBIR_cmd --Plarail_Speed_UpAF

※open_device関数とclose_device関数はkjmkznr 氏作の bto_ir_cmdからコピー
bto_ir_cmdはMITライセンス。ちなみにもちろんプログラム名:bto_advanced_USBIR.cmdもbto_ir_cmdに由来
しています。advancedと付けていますが、それは半ばジョークであって、別にbto_ir_cmdと比べてadvancedな
わけではありません。

※今のところ--Plarail_StopAオプションで出力される赤外線命令と、--Plarail_StopBオプションで出力される
赤外線命令は同じです。プラレール用の赤外線命令はUSB_IR_Plarail.exeから実際に出力される赤外線命令を
コピーしたものなので、USB_IR_Plarail.exeの状態(問題)をそのまま引き継いでいます。

※今のところ本プログラムには、USB_IR_Remote_Controller_Advance_Library_v4.1.0.0に実装されている、
命令をリピートする機能を実装していません。リピート命令は主に赤外線リモコンをマウスの代わりに使う
際に必要なものだと思われるので、主に送信系を中心に使われると思われる非Windows環境では必要性が
低いと思われる為です。



ビルド＆インストール(必要ライブラリ libusb-1.0)
$ make; sudo make install

アンインストール
$ sudo make uninstall


使い方の例
受信系
$ bto_advanced_USBIR_cmd -r         # (生データ)受信開始
$ bto_advanced_USBIR_cmd -s         # (生データ)受信停止
$ bto_advanced_USBIR_cmd -g | tee data.txt  # 生データ所得

送信系
$ bto_advanced_USBIR_cmd -d `cat data.txt`
$ bto_advanced_USBIR_cmd -t AEHA       -C 123456789ABC
$ bto_advanced_USBIR_cmd -t NEC        -C 08F6817E
$ bto_advanced_USBIR_cmd -t SONY       -C 08F6817E
$ bto_advanced_USBIR_cmd -t MITSUBISHI -C 08F6817E
$ bto_advanced_USBIR_cmd --Plarail_Speed_UpAF


移植作業者: disklessfun@gmail.com
