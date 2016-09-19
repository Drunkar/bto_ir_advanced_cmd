/*

bto_advanced_USBIR_cmd

ビット・トレード・ワン社提供の、C#言語で記述された
USB赤外線リモコンアドバンス用ライブラリ(USB_IR_Remote_Controller_Advance_Library_v4.1.0.0) を
C言語ソースに移植し、コマンドラインインターフェースを付けたもの。

著作権者:(C) 2015 ビット・トレード・ワン社
ライセンス: ADL(Assembly Desk License)

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

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <libusb-1.0/libusb.h>
#include <getopt.h>

#define APP_VERSION "1.0.0"

#define VENDOR_ID  0x22ea
#define PRODUCT_ID 0x003a
#define BTO_EP_IN  0x84
#define BTO_EP_OUT 0x04

typedef unsigned char byte;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char bool;
#define true	1
#define false	0

#define BUFF_SIZE 64
#define FORMAT_NUM 4
#define OPTION_NUM 10

#define IR_FREQ_MIN						25000	// 赤外線周波数設定最小値 25KHz
#define IR_FREQ_MAX						50000	// 赤外線周波数設定最大値 50KHz
#define IR_SEND_DATA_USB_SEND_MAX_LEN	14		// USB送信１回で送信する最大ビット数
#define IR_SEND_DATA_MAX_LEN			300		// 赤外線送信データ設定最大長[byte]

#define IR_FREQ_DEFAULT					38000
static uint frequency = IR_FREQ_DEFAULT;

#define MAX_BYTE_ARRAY_SIZE				9600

#define FORMAT_AEHA_READER_CODE			0x007B003D	// 家電協 Reader Code ON:3.2ms/26us=123(0x7B), OFF:1.6ms/26us=61(0x3D)
#define FORMAT_AEHA_BIT_OFF				0x000F000F	// 家電協 BitOFF ON:0.4ms/26us=15(0x0F), OFF:0.4ms/26us=15(0x0F)
#define FORMAT_AEHA_BIT_ON				0x000F002E	// 家電協 BitON  ON:0.4ms/26us=15(0x0F), OFF:1.2ms/26us=46(0x2E)
#define FORMAT_NEC_READER_CODE			0x015A00AD	// NEC Reader Code ON:9.0ms/26us=346(0x15A), OFF:4.5ms/26us=173(0xAD)
#define FORMAT_NEC_BIT_OFF				0x00160016	// NEC BitOFF ON:0.56ms/26us=22(0x16), OFF:0.56ms/26us=22(0x16)
#define FORMAT_NEC_BIT_ON				0x00160041	// NEC BitON  ON:0.56ms/26us=22(0x16), OFF:1.68ms/26us=65(0x41)
#define FORMAT_SONY_READER_CODE			0x005C0017	// SONY Reader Code ON:2.4ms/26us=92(0x5C), OFF:0.6ms/26us=23(0x17)
#define FORMAT_SONY_BIT_OFF				0x00170017	// SONY BitOFF ON:0.6ms/26us=23(0x17), OFF:0.6ms/26us=23(0x17)
#define FORMAT_SONY_BIT_ON				0x002E0017	// SONY BitON  ON:1.2ms/26us=46(0x2E), OFF:0.6ms/26us=23(0x17)
#define FORMAT_MITSU_READER_CODE		0x00000000	// MITSUBISHI Reader Code なし
#define FORMAT_MITSU_BIT_OFF			0x000F001F	// MITSUBISHI BitOFF ON:0.4ms/26us=15(0x0F), OFF:0.8ms/26us=31(0x1F)
#define FORMAT_MITSU_BIT_ON				0x000F004D	// MITSUBISHI BitON  ON:0.4ms/26us=15(0x0F), OFF:2.0ms/26us=77(0x4D)
#define FORMAT_STOP_CODE				0x00170619	// STOP CODE

enum
{
    IR_FORMAT_AEHA = 1,
    IR_FORMAT_NEC,
    IR_FORMAT_SONY,
    IR_FORMAT_MITSUBISHI
};
enum
{
    PLARAIL_BAND_BAND_A = 1,
    PLARAIL_BAND_BAND_B
};
enum
{
    PLARAIL_DIRECTION_FORWARD = 1,
    PLARAIL_DIRECTION_BACKWARD
};
enum
{
    Plarail_StopA,
    Plarail_StopB,
    Plarail_Speed_UpAF,
    Plarail_Speed_UpAB,
    Plarail_Speed_UpBF,
    Plarail_Speed_UpBB,
    Plarail_Speed_DownA,
    Plarail_Speed_DownB
};
static char FORMATlist[FORMAT_NUM][11] =
{
    "AEHA",
    "NEC",
    "SONY",
    "MITSUBISHI"
};
static char PLAOPTIONlist[OPTION_NUM-2][20] =
{
    "Plarail_StopA",
    "Plarail_StopB",
    "Plarail_Speed_UpAF",
    "Plarail_Speed_UpAB",
    "Plarail_Speed_UpBF",
    "Plarail_Speed_UpBB",
    "Plarail_Speed_DownA",
    "Plarail_Speed_DownB"
};

void close_device(libusb_context *ctx, libusb_device_handle *devh);
libusb_device_handle* open_device(libusb_context *ctx);
void setup_optargs(struct option options[]);
void usage(char *fname);
void version(char *fname);
int writeUSBIR(struct libusb_device_handle *devh, uint format_type, byte code[], int code_len);
int writeUSBIRCode(struct libusb_device_handle *devh, uint freq, uint reader_code, uint bit_0, uint bit_1, uint stop_code, byte code[], uint bit_len);
int writeUSBIRData(struct libusb_device_handle *devh, uint freq, byte data[], uint bit_len, uint data_count);
int writeUSBIRData_Ushort(struct libusb_device_handle *devh, uint freq, ushort data[], uint bit_len, uint ele_num);
int writeUSBIR_Plarail_Stop(struct libusb_device_handle *devh, uint band);
int writeUSBIR_Plarail_Speed_Up(struct libusb_device_handle *devh, uint band, uint dir);
int writeUSBIR_Plarail_Speed_Down(struct libusb_device_handle *devh, uint band);
int recUSBIRData_Start(struct libusb_device_handle *devh, uint freq);
int recUSBIRData_Stop(struct libusb_device_handle *devh);
int readUSBIRData(struct libusb_device_handle *devh, byte data[], uint data_buff_len, uint *bit_len);

void close_device(libusb_context *ctx, libusb_device_handle *devh) {
  libusb_close(devh);
  libusb_exit(ctx);
}

libusb_device_handle* open_device(libusb_context *ctx) {
  struct libusb_device_handle *devh = NULL;
  libusb_device *dev;
  libusb_device **devs;

  int r = 1;
  int i = 0;
  int cnt = 0;

  libusb_set_debug(ctx, 3);
  
  if ((libusb_get_device_list(ctx, &devs)) < 0) {
    perror("no usb device found");
    exit(1);
  }

  /* check every usb devices */
  while ((dev = devs[i++]) != NULL) {
    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(dev, &desc) < 0) {
      perror("failed to get device descriptor\n");
    }
    /* count how many device connected */
    if (desc.idVendor == VENDOR_ID && desc.idProduct == PRODUCT_ID) {
      cnt++;
    }
  }

  /* device not found */
  if (cnt == 0) {
    fprintf(stderr, "device not connected\n");
    exit(1);
  }

  if (cnt > 1) {
    fprintf(stderr, "multi device is not implemented yet\n");
    exit(1);
  }


  /* open device */
  if ((devh = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID)) < 0) {
    perror("can't find device\n");
    close_device(ctx, devh);
    exit(1);
  } 

  /* detach kernel driver if attached. */
  r = libusb_kernel_driver_active(devh, 3);
  if (r == 1) {
    /* detaching kernel driver */
    r = libusb_detach_kernel_driver(devh, 3);
    if (r != 0) {
      perror("detaching kernel driver failed");
      exit(1);
    }
  }

  r = libusb_claim_interface(devh, 3);
  if (r < 0) {
    fprintf(stderr, "claim interface failed (%d): %s\n", r, strerror(errno));
    exit(1);
  }

  return devh;
}

void setup_optargs(struct option options[])
{
  int fi;
  for(fi=0;fi<OPTION_NUM-2;fi++)
  {
    options[fi].name = PLAOPTIONlist[fi];
    options[fi].has_arg = no_argument;
    options[fi].flag = NULL;
    options[fi].val = 1;
  }

  options[OPTION_NUM - 2].name = "version";
  options[OPTION_NUM - 2].has_arg = no_argument;
  options[OPTION_NUM - 2].flag = NULL;
  options[OPTION_NUM - 2].val = 2;
  options[OPTION_NUM - 1].name = 0;
  options[OPTION_NUM - 1].has_arg = 0;
  options[OPTION_NUM - 1].flag = NULL;
  options[OPTION_NUM - 1].val = 0;
}

void usage(char *fname) {
  fprintf(stderr, "usage: %s <option>\n", fname);
  fprintf(stderr, "  -f <freq>\t-dオプションまたは -rオプションを指定した場合のみ使用できます。\n");
  fprintf(stderr, "           \t指定しない場合は%dになります。\n", IR_FREQ_DEFAULT);
  fprintf(stderr, "  -t {AEHA,NEC,SONY,MITSUBISHI}\n");
  fprintf(stderr, "  -c <code>\t家電協(AEHA)フォーマット、NECフォーマット、SONYフォーマット、三菱フォーマットの\n");
  fprintf(stderr, "           \tコードデータをこの -cオプション引数として指定します。\n");
  fprintf(stderr, "           \t書式は-dオプションと同じ 0xFF,0xFF,... です。必ず-tオプションとセットで指定します。\n");
  fprintf(stderr, "  -C <Code>\t家電協(AEHA)フォーマット、NECフォーマット、SONYフォーマット、三菱フォーマットの\n");
  fprintf(stderr, "           \tコードデータをこの -Cオプション引数として指定します。\n");
  fprintf(stderr, "           \t書式は0xの付かない16進文字列 FFFF... です。必ず-tオプションとセットで指定します。\n");
  fprintf(stderr, "  -d <data>\t受信設定または送信設定コンフィグレーションツールで、クリップボードに\n");
  fprintf(stderr, "           \tコピーボタンでコピーしたデータ、または、-gオプションで取得したデータが、\n");
  fprintf(stderr, "           \tこの -dオプション引数として使用可能です。\n");
  fprintf(stderr, "           \t-fオプションのみ追加で指定可能です。\n");
  fprintf(stderr, "  -r       \t受信開始を指令します。\n");
  fprintf(stderr, "           \t-fオプションのみ追加で指定可能です。\n");
  fprintf(stderr, "           \t受信設定または送信設定コンフィグレーションツールで取得できるデータが、\n");
  fprintf(stderr, "           \t本コマンドでも取得できます。\n");
  fprintf(stderr, "  -s       \t受信停止を指令します。\n");
  fprintf(stderr, "  -g       \t直前に受信を終えたデータが所得できます。\n");
  fprintf(stderr, "  --Plarail_StopA\t\tこのオプションは必ず単独で指定します。\n");
  fprintf(stderr, "  --Plarail_StopB\t\tこのオプションは必ず単独で指定します。\n");
  fprintf(stderr, "  --Plarail_Speed_UpAF\t\tこのオプションは必ず単独で指定します。\n");
  fprintf(stderr, "  --Plarail_Speed_UpAB\t\tこのオプションは必ず単独で指定します。\n");
  fprintf(stderr, "  --Plarail_Speed_UpBF\t\tこのオプションは必ず単独で指定します。\n");
  fprintf(stderr, "  --Plarail_Speed_UpBB\t\tこのオプションは必ず単独で指定します。\n");
  fprintf(stderr, "  --Plarail_Speed_DownA\t\tこのオプションは必ず単独で指定します。\n");
  fprintf(stderr, "  --Plarail_Speed_DownB\t\tこのオプションは必ず単独で指定します。\n");
  fprintf(stderr, "  --version\n");
  fprintf(stderr, "※ getopt_longモジュールの制限を回避する為、プラレール用のオプションは末尾まで正確に指定して下さい。\n\n");
  fprintf(stderr, "使い方の例\n");
  fprintf(stderr, "受信系\n");
  fprintf(stderr, "$ bto_advanced_USBIR_cmd -r         # (生データ)受信開始\n");
  fprintf(stderr, "$ bto_advanced_USBIR_cmd -s         # (生データ)受信停止\n");
  fprintf(stderr, "$ bto_advanced_USBIR_cmd -g | tee data.txt  # 生データ所得\n\n");
  fprintf(stderr, "送信系\n");
  fprintf(stderr, "$ bto_advanced_USBIR_cmd -d `cat data.txt`\n");
  fprintf(stderr, "$ bto_advanced_USBIR_cmd -t AEHA       -C 123456789ABC\n");
  fprintf(stderr, "$ bto_advanced_USBIR_cmd -t NEC        -C 08F6817E\n");
  fprintf(stderr, "$ bto_advanced_USBIR_cmd -t SONY       -C 08F6817E\n");
  fprintf(stderr, "$ bto_advanced_USBIR_cmd -t MITSUBISHI -C 08F6817E\n");
  fprintf(stderr, "$ bto_advanced_USBIR_cmd --Plarail_Speed_UpAF\n");
}

void version(char *fname) {
  fprintf(stderr, "%s\t%s\n", fname, APP_VERSION);
  fprintf(stderr, "Copyright (C) 2015   Bit Trade One, LTD.\n");
  fprintf(stderr, "ライセンス:ADL   ADLの内容は:http://bit-trade-one.co.jp/adl/\n");
}

int main(int argc, char *argv[]) {
  libusb_context *ctx = NULL;
  int ret = 1;
  struct option *options;
  int option_index;
  int freq_flag = 0;
  int type_flag = 0;
  char type_arg[20] = "\0";
  int code_flag = 0;
  int Code_flag = 0;
  char Code_arg[600] = "\0";
  char hex_buff[5] = "\0";
  int str_len = 0;
  int data_flag = 0;
  int pla_flag = 0;
  int read_flag = 0;
  int stop_flag = 0;
  int get_flag = 0;
  char *s;
  byte *data = NULL;
  uint dataCount = 0;
  byte *code = NULL;
  uint codeCount = 0;
  char *endPtr;
  int placounter = 0, plaindex;
  int fi, typeindex = -1;
  uint ibit_len = 0;

  options = malloc(sizeof(struct option) * OPTION_NUM);
  setup_optargs(options);
  while ((ret = getopt_long(argc, argv, "f:t:c:C:d:rsgh", options, &option_index)) != -1)
  {
    switch(ret)
    {
      case 1:
        pla_flag = 1;
        placounter++;
        plaindex = option_index;
        break;
      case 2:
        version(argv[0]);
        exit(0);
      case 'd':
        data_flag = 1;
          while((s = strtok(optarg, ", ")) != NULL)
          {
             optarg = NULL;
             if(data != NULL){
               data = realloc(data, dataCount + 1);
             }else{
               data = malloc(dataCount + 1);
             }
             data[dataCount++] = (byte)strtol(s, &endPtr, 0);
          }
        break;
      case 'c':
        code_flag = 1;
          while((s = strtok(optarg, ", ")) != NULL)
          {
             optarg = NULL;
             if(code != NULL){
               code = realloc(code, codeCount + 1);
             }else{
               code = malloc(codeCount + 1);
             }
             code[codeCount++] = (byte)strtol(s, &endPtr, 0);
          }
        break;
      case 'C':
        Code_flag = 1;
        strcpy(Code_arg, optarg);
        break;
      case 'f':
        freq_flag = 1;
        frequency = atoi(optarg);
        break;
      case 't':
        type_flag = 1;
        strcpy(type_arg, optarg);
        break;
      case 'r':
        read_flag = 1;
        break;
      case 's':
        stop_flag = 1;
        break;
      case 'g':
        get_flag = 1;
        break;
      case 'h':
        usage(argv[0]);
        exit(0);
        break;
      default:
        usage(argv[0]);
        exit(1);
        break;
    }
  }


if((!type_flag)&&(!freq_flag)&&(!code_flag)&&(!Code_flag)&&(!data_flag)&&(!pla_flag)&&(!read_flag)&&(!stop_flag)&&(!get_flag))
{
  usage(argv[0]);
  exit(1);
}
else if((code_flag||Code_flag)&&data_flag)
{
  fprintf(stderr, "エラー: -dオプションと-cまたは-Cオプションとは同時に指定できません。\n");
  exit(1);
}
else if(code_flag&&Code_flag)
{
  fprintf(stderr, "エラー: -cオプションと-Cオプションとは同時に指定できません。\n");
  exit(1);
}
else if(freq_flag&&(code_flag||Code_flag))
{
  fprintf(stderr, "エラー: -rまたは-Cオプションと-fオプションは同時に指定できません。\n");
  exit(1);
}
else if((code_flag||Code_flag||data_flag)&&(read_flag||stop_flag||get_flag))
{
  fprintf(stderr, "エラー: 送信系専用のオプション：-c、-C、-dオプションと受信系専用のオプション：-r、-s、-gオプションは同時に指定できません。\n");
  exit(1);
}
else if((read_flag&&stop_flag)||(stop_flag&&get_flag)||(read_flag&&get_flag))
{
  fprintf(stderr, "エラー: -rオプション、-sオプション、-gオプションは同時に指定できません。\n");
  exit(1);
}
else if(freq_flag&&(stop_flag||get_flag))
{
  fprintf(stderr, "エラー: -sまたは-gオプションと-fオプションは同時に指定できません。\n");
  exit(1);
}
else if(((!type_flag)&&(code_flag||Code_flag))||(((!code_flag)&&(!Code_flag))&&type_flag))
{
  fprintf(stderr, "エラー: -tオプションと-cまたは-Cオプションとは必ずセットで指定して下さい。\n");
  exit(1);
}
else if(((!data_flag)||(!read_flag))&&freq_flag)
{
  fprintf(stderr, "エラー: -fオプションは-dオプションまたは-rオプションを指定した場合のみ使用できます。\n");
  exit(1);
}
else if(pla_flag&&(freq_flag||type_flag||data_flag||code_flag||read_flag||stop_flag||get_flag))
{
  fprintf(stderr, "エラー: プラレール赤外線命令オプションは単独で指定して下さい。\n");
  exit(1);
}
else if(stop_flag&&(freq_flag||type_flag||data_flag||code_flag||read_flag||get_flag))
{
  fprintf(stderr, "エラー: -sオプションは単独で指定して下さい。\n");
  exit(1);
}
else if(placounter>1)
{
  fprintf(stderr, "エラー: プラレール赤外線命令オプションは単独で指定して下さい。\n");
  exit(1);
}
else if(type_flag)
{
  for(fi=0;fi<FORMAT_NUM;fi++)
  {
    if((ret = strcmp(type_arg, FORMATlist[fi]))==0)
    {
      typeindex=fi;
      break;
    }
  }
  if(typeindex<0)
  {
    fprintf(stderr, "エラー: 正しい赤外線フォーマットのタイプを指定して下さい。\n");
    exit(1);
  }
  typeindex++;	// enumの方は1から開始だから
}

if(data_flag)
{
  if((dataCount % 2) != 0)
  {
    fprintf(stderr, "エラー: データの総数は偶数である必要があります。: %d\n", dataCount);
    exit(1);
  }
}
else if(Code_flag)
{
  str_len=strlen(Code_arg);
  if((str_len % 2) !=0)
  {
    fprintf(stderr, "エラー: コード長は2の倍数である必要があります。: %d\n", str_len);
    exit(1);
  }
  for(fi=0;fi<str_len / 2;fi++)
  {
    strcpy(hex_buff, "0x");
    strncat(hex_buff, &Code_arg[fi*2], 2);

    if(code != NULL){
      code = realloc(code, codeCount + 1);
    }else{
      code = malloc(codeCount + 1);
    }
    code[codeCount++] = (byte)strtol(hex_buff, &endPtr, 0);
  }
}


/* libusb initialize*/
ret = libusb_init(&ctx);
if (ret < 0)
{
  perror("libusb_init\n");
  exit(1);
}

/* open device */
libusb_device_handle *devh = open_device(ctx);

if (data_flag)
{
    if((ret = writeUSBIRData(devh, frequency, data, dataCount/4, dataCount))<0)
        fprintf(stderr, "error %d\n", ret);
}
else if (code_flag||Code_flag)
{
    if((ret = writeUSBIR(devh, typeindex, code, codeCount*8))<0)
        fprintf(stderr, "error %d\n", ret);
}
else if (read_flag)
{
    if((ret = recUSBIRData_Start(devh, frequency))<0)
        fprintf(stderr, "error %d\n", ret);
    else
        fprintf(stdout, "受信を開始しました。\n");
}
else if (stop_flag)
{
    if((ret = recUSBIRData_Stop(devh))<0)
        fprintf(stderr, "error %d\n", ret);
    else
        fprintf(stdout, "受信を停止しました。\n");
}
else if (get_flag)
{
    data = malloc(MAX_BYTE_ARRAY_SIZE);
    memset(data, 0x00, MAX_BYTE_ARRAY_SIZE);

    if((ret = readUSBIRData(devh, data, MAX_BYTE_ARRAY_SIZE, &ibit_len))<0)
        fprintf(stderr, "error %d\n", ret);
    else
    {
        fprintf(stderr, "取得したbyte配列の要素数:%d\n", ibit_len*4);
        if(ibit_len>0)
        {
            for(fi=0;fi<ibit_len*4-1;fi++)
                fprintf(stdout, "0x%02x,", data[fi]);
            fprintf(stdout, "0x%02x", data[ibit_len*4-1]);
        }
    }
}
else if(pla_flag)
{
    switch(plaindex)
    {
    case Plarail_StopA:
        if((ret = writeUSBIR_Plarail_Stop(devh, PLARAIL_BAND_BAND_A))<0)
            fprintf(stderr, "error %d\n", ret);
        break;
    case Plarail_StopB:
        if((ret = writeUSBIR_Plarail_Stop(devh, PLARAIL_BAND_BAND_B))<0)
            fprintf(stderr, "error %d\n", ret);
        break;
    case Plarail_Speed_UpAF:
        if((ret = writeUSBIR_Plarail_Speed_Up(devh, PLARAIL_BAND_BAND_A, PLARAIL_DIRECTION_FORWARD))<0)
            fprintf(stderr, "error %d\n", ret);
        break;
    case Plarail_Speed_UpAB:
        if((ret = writeUSBIR_Plarail_Speed_Up(devh, PLARAIL_BAND_BAND_A, PLARAIL_DIRECTION_BACKWARD))<0)
            fprintf(stderr, "error %d\n", ret);
        break;
    case Plarail_Speed_UpBF:
        if((ret = writeUSBIR_Plarail_Speed_Up(devh, PLARAIL_BAND_BAND_B, PLARAIL_DIRECTION_FORWARD))<0)
            fprintf(stderr, "error %d\n", ret);
        break;
    case Plarail_Speed_UpBB:
        if((ret = writeUSBIR_Plarail_Speed_Up(devh, PLARAIL_BAND_BAND_B, PLARAIL_DIRECTION_BACKWARD))<0)
            fprintf(stderr, "error %d\n", ret);
        break;
    case Plarail_Speed_DownA:
        if((ret = writeUSBIR_Plarail_Speed_Down(devh, PLARAIL_BAND_BAND_A))<0)
            fprintf(stderr, "error %d\n", ret);
        break;
    case Plarail_Speed_DownB:
        if((ret = writeUSBIR_Plarail_Speed_Down(devh, PLARAIL_BAND_BAND_B))<0)
            fprintf(stderr, "error %d\n", ret);
        break;
    default:
        break;
    }
}

/* close device */
close_device(ctx, devh);
return ret;
}


int writeUSBIR(struct libusb_device_handle *devh, uint format_type, byte code[], int code_len)
{
uint reader_code = 0, bit_0 = 0, bit_1 = 0, stop_code = FORMAT_STOP_CODE;
int i_ret = -1;

int code_len_check = (int)(code_len / 8);
if ((code_len % 8) > 0)
{
    code_len_check++;
}
// パラメータチェック
if (devh != NULL &&
	0 < code_len &&
	code_len <= (IR_SEND_DATA_MAX_LEN*8))
//	code.Length >= code_len_check)
{
    switch(format_type)
    {
        case IR_FORMAT_AEHA:
            reader_code = FORMAT_AEHA_READER_CODE;
            bit_0 = FORMAT_AEHA_BIT_OFF;
            bit_1 = FORMAT_AEHA_BIT_ON;
            break;
        case IR_FORMAT_NEC:
            reader_code = FORMAT_NEC_READER_CODE;
            bit_0 = FORMAT_NEC_BIT_OFF;
            bit_1 = FORMAT_NEC_BIT_ON;
            break;
        case IR_FORMAT_SONY:
            reader_code = FORMAT_SONY_READER_CODE;
            bit_0 = FORMAT_SONY_BIT_OFF;
            bit_1 = FORMAT_SONY_BIT_ON;
            break;
        case IR_FORMAT_MITSUBISHI:
            reader_code = FORMAT_MITSU_READER_CODE;
            bit_0 = FORMAT_MITSU_BIT_OFF;
            bit_1 = FORMAT_MITSU_BIT_ON;
            break;
        default:
            break;
    }
    if (writeUSBIRCode(devh, IR_FREQ_DEFAULT, reader_code, bit_0, bit_1, stop_code, code, (uint)code_len) == 0)
    {
        i_ret = 0;
    }
    else
    {
        i_ret = -3;
    }
}
else
{   // パラメータエラー
    i_ret = -2;
}
return i_ret;
}


int writeUSBIRCode(struct libusb_device_handle *devh, uint freq, uint reader_code, uint bit_0, uint bit_1, uint stop_code, byte code[], uint bit_len)
{
uint fi;
int byte_pos, bit_pos, bit_mask, tmp_data;
int i_ret = -1;
byte outbuffer[BUFF_SIZE];
byte inbuffer[BUFF_SIZE];
int BytesWritten = 0;
int BytesRead = 0;
bool error_flag = false;
uint send_bit_num = 1 + bit_len + 1;    // 送信ビット数　リーダーコード(1) + コード + 終了コード(1)
uint send_bit_pos = 0;                  // 送信セット済みビット位置
uint set_bit_size = 0;

int code_len_check = (int)(bit_len / 8);
if((bit_len % 8) > 0)
{
    code_len_check++;
}
// パラメータチェック
if (devh != NULL &&
	IR_FREQ_MIN <= freq &&
	freq <= IR_FREQ_MAX &&
	0 < bit_len &&
	bit_len <= (IR_SEND_DATA_MAX_LEN*8))
//	repeat_code.Length >= repeat_code_len)
{
    // データセット
    while (true)
    {
        outbuffer[0] = 0x34;
        //送信総ビット数
        outbuffer[1] = (byte)((send_bit_num >> 8) & 0xFF);
        outbuffer[2] = (byte)(send_bit_num & 0xFF);
        outbuffer[3] = (byte)((send_bit_pos >> 8) & 0xFF);
        outbuffer[4] = (byte)(send_bit_pos & 0xFF);
        if (send_bit_num > send_bit_pos)
        {
            set_bit_size = send_bit_num - send_bit_pos;
            if (set_bit_size > IR_SEND_DATA_USB_SEND_MAX_LEN)
            {
                set_bit_size = IR_SEND_DATA_USB_SEND_MAX_LEN;
            }
        }
        else
        {   // 送信データなし
            set_bit_size = 0;
        }
        outbuffer[5] = (byte)(set_bit_size & 0xFF);

        if (set_bit_size > 0)
        {
            // データセット
            // 赤外線コードコピー
            for (fi = 0; fi < set_bit_size; fi++)
            {
                if (send_bit_pos == 0)
                {   // Reader Code
                    // ON Count
                    outbuffer[6 + (fi * 4)] = (byte)((reader_code >> 24) & 0xFF);
                    outbuffer[6 + (fi * 4) + 1] = (byte)((reader_code >> 16) & 0xFF);
                    // OFF Count
                    outbuffer[6 + (fi * 4) + 2] = (byte)((reader_code >> 8) & 0xFF);
                    outbuffer[6 + (fi * 4) + 3] = (byte)(reader_code & 0xFF);
                }
                else if (send_bit_pos == (send_bit_num - 1))
                {   // Stop Code
                    // ON Count
                    outbuffer[6 + (fi * 4)] = (byte)((stop_code >> 24) & 0xFF);
                    outbuffer[6 + (fi * 4) + 1] = (byte)((stop_code >> 16) & 0xFF);
                    // OFF Count
                    outbuffer[6 + (fi * 4) + 2] = (byte)((stop_code >> 8) & 0xFF);
                    outbuffer[6 + (fi * 4) + 3] = (byte)(stop_code & 0xFF);
                }
                else
                {
                    byte_pos = (int)(send_bit_pos - 1) / 8;
                    bit_pos = (int)(send_bit_pos - 1) % 8;
                    bit_mask = 0x01 << bit_pos;
                    tmp_data = code[byte_pos];

                    if ((tmp_data & bit_mask & 0xFF) != 0)
                    {   // Bit 1
                        // ON Count
                        outbuffer[6 + (fi * 4)] = (byte)((bit_1 >> 24) & 0xFF);
                        outbuffer[6 + (fi * 4) + 1] = (byte)((bit_1 >> 16) & 0xFF);
                        // OFF Count
                        outbuffer[6 + (fi * 4) + 2] = (byte)((bit_1 >> 8) & 0xFF);
                        outbuffer[6 + (fi * 4) + 3] = (byte)(bit_1 & 0xFF);
                    }
                    else
                    {   // bit 0
                        // ON Count
                        outbuffer[6 + (fi * 4)] = (byte)((bit_0 >> 24) & 0xFF);
                        outbuffer[6 + (fi * 4) + 1] = (byte)((bit_0 >> 16) & 0xFF);
                        // OFF Count
                        outbuffer[6 + (fi * 4) + 2] = (byte)((bit_0 >> 8) & 0xFF);
                        outbuffer[6 + (fi * 4) + 3] = (byte)(bit_0 & 0xFF);
                    }
                }
                send_bit_pos++;
            }
            if(libusb_interrupt_transfer(devh, BTO_EP_OUT, outbuffer, BUFF_SIZE ,&BytesWritten, 5000) == 0)
            {
                //Now get the response packet from the firmware.
                {
                    if(libusb_interrupt_transfer(devh, BTO_EP_IN, inbuffer, BUFF_SIZE, &BytesRead, 5000) == 0)
                    {
                        //INBuffer[0] is an echo back of the command (see microcontroller firmware).
                        //INBuffer[1] contains the I/O port pin value for the pushbutton (see microcontroller firmware).  
                        if (inbuffer[0] == 0x34)
                        {
                            if (inbuffer[1] != 0x00)
                            {
                                // NG
                                error_flag = true;
                            }
                        }
                    }
                    else
                    {
                        // NG
                        error_flag = true;
                    }
                }
            }
            else
            {
                // NG
                error_flag = true;
            }

        }
        else
        {   // 送信データなし
            break;
        }
    }

    // データ送信要求セット
    if (error_flag == false)
    {
        outbuffer[0] = 0x35;		//0x81 is the "Get Pushbutton State" command in the firmware
        outbuffer[1] = (byte)((freq >> 8) & 0xFF);
        outbuffer[2] = (byte)(freq & 0xFF);
        outbuffer[3] = (byte)((send_bit_num >> 8) & 0xFF);
        outbuffer[4] = (byte)(send_bit_num & 0xFF);

        //To get the pushbutton state, first, we send a packet with our "Get Pushbutton State" command in it.
        if(libusb_interrupt_transfer(devh, BTO_EP_OUT, outbuffer, BUFF_SIZE ,&BytesWritten, 5000) == 0)
        {
            //Now get the response packet from the firmware.
            {
                if(libusb_interrupt_transfer(devh, BTO_EP_IN, inbuffer, BUFF_SIZE, &BytesRead, 5000) == 0)
                {
                    //INBuffer[0] is an echo back of the command (see microcontroller firmware).
                    //INBuffer[1] contains the I/O port pin value for the pushbutton (see microcontroller firmware).  
                    if (inbuffer[0] == 0x35)
                    {
                        if (inbuffer[1] == 0x00)
                        {   // OK
                            i_ret = 0;
                        }
                        else
                        {
                            // NG
                            error_flag = true;
                        }
                    }
                }
                else
                {
                    // NG
                    i_ret = -5;
                }
            }
        }
        else
        {
            // NG
            i_ret = -4;
        }
    }
    else
    {   // データセットエラー
        i_ret = -3;
    }
}
else
{   // パラメータエラー
    i_ret = -2;
}
return i_ret;
}


int writeUSBIRData(struct libusb_device_handle *devh, uint freq, byte data[], uint bit_len, uint data_count)
{
uint fi;
int i_ret = -1;
byte outbuffer[BUFF_SIZE];
byte inbuffer[BUFF_SIZE];
int BytesWritten = 0;
int BytesRead = 0;
bool error_flag = false;
uint send_bit_num = 0;
uint send_bit_pos = 0;                  // 送信セット済みビット位置
uint set_bit_size = 0;

// ↓元にしたソース:USBIR.csから独自に拡張した箇所。data_count/2が奇数の場合に対応。引数bit_lenは使用しない。代わりに追加した引数data_countを使う。
send_bit_num = data_count / 4 + (data_count % 4)/2; // 送信ビット数　リーダーコード + コード + 終了コード

// パラメータチェック
if (devh != NULL &&
	IR_FREQ_MIN <= freq &&
	freq <= IR_FREQ_MAX &&
	0 < send_bit_num &&
	send_bit_num <= (IR_SEND_DATA_MAX_LEN * 8))
    {
        // データセット
        while (true)
        {
            outbuffer[0] = 0x34;
            //送信総ビット数
            outbuffer[1] = (byte)((send_bit_num >> 8) & 0xFF);
            outbuffer[2] = (byte)(send_bit_num & 0xFF);
            outbuffer[3] = (byte)((send_bit_pos >> 8) & 0xFF);
            outbuffer[4] = (byte)(send_bit_pos & 0xFF);
            if (send_bit_num > send_bit_pos)
            {
                set_bit_size = send_bit_num - send_bit_pos;
                if (set_bit_size > IR_SEND_DATA_USB_SEND_MAX_LEN)
                {
                    set_bit_size = IR_SEND_DATA_USB_SEND_MAX_LEN;
                }
            }
            else
            {   // 送信データなし
                set_bit_size = 0;
            }
            outbuffer[5] = (byte)(set_bit_size & 0xFF);

            if (set_bit_size > 0)
            {
                // データセット
                // 赤外線コードコピー
                for (fi = 0; fi < set_bit_size; fi++)
                {
                    // ON Count
                    outbuffer[6 + (fi * 4)] = data[send_bit_pos * 4];
                    outbuffer[6 + (fi * 4) + 1] = data[(send_bit_pos * 4) + 1];
                    // OFF Count
                    outbuffer[6 + (fi * 4) + 2] = data[(send_bit_pos * 4) + 2];
                    outbuffer[6 + (fi * 4) + 3] = data[(send_bit_pos * 4) + 3];
                    send_bit_pos++;
                }
                if(libusb_interrupt_transfer(devh, BTO_EP_OUT, outbuffer, BUFF_SIZE ,&BytesWritten, 5000) == 0)
                {

                    //Now get the response packet from the firmware.
                    {
                        if(libusb_interrupt_transfer(devh, BTO_EP_IN, inbuffer, BUFF_SIZE, &BytesRead, 5000) == 0)
                        {
                            //INBuffer[0] is an echo back of the command (see microcontroller firmware).
                            //INBuffer[1] contains the I/O port pin value for the pushbutton (see microcontroller firmware).  
                            if (inbuffer[0] == 0x34)
                            {
                                if (inbuffer[1] != 0x00)
                                {
                                    // NG
                                    error_flag = true;
                                }
                            }
                        }
                        else
                        {
                            // NG
                            error_flag = true;
                        }
                    }
                }
                else
                {
                    // NG
                    error_flag = true;
                }
            }
            else
            {   // 送信データなし
                break;
            }
        }

        // データ送信要求セット
        if (error_flag == false)
        {
            outbuffer[0] = 0x35;		//0x81 is the "Get Pushbutton State" command in the firmware
            outbuffer[1] = (byte)((freq >> 8) & 0xFF);
            outbuffer[2] = (byte)(freq & 0xFF);
            outbuffer[3] = (byte)((send_bit_num >> 8) & 0xFF);
            outbuffer[4] = (byte)(send_bit_num & 0xFF);
            BytesRead = 0;

            //To get the pushbutton state, first, we send a packet with our "Get Pushbutton State" command in it.
            if(libusb_interrupt_transfer(devh, BTO_EP_OUT, outbuffer, BUFF_SIZE ,&BytesWritten, 5000) == 0)
            {
                //Now get the response packet from the firmware.
                {
                    if(libusb_interrupt_transfer(devh, BTO_EP_IN, inbuffer, BUFF_SIZE, &BytesRead, 5000) == 0)
                    {
                        //INBuffer[0] is an echo back of the command (see microcontroller firmware).
                        //INBuffer[1] contains the I/O port pin value for the pushbutton (see microcontroller firmware).  
                        if (inbuffer[0] == 0x35)
                        {
                            if (inbuffer[1] == 0x00)
                            {   // OK
                                i_ret = 0;
                            }
                            else
                            {
                                // NG
                                error_flag = true;
                            }
                        }
                    }
                    else
                    {
                        // NG
                        i_ret = -5;
                    }
                }
            }
            else
            {
                // NG
                i_ret = -4;
            }
        }
        else
        {   // データセットエラー
            i_ret = -3;
        }
    }
    else
    {   // パラメータエラー
        i_ret = -2;
    }
return i_ret;
}

int writeUSBIRData_Ushort(struct libusb_device_handle *devh, uint freq, ushort udata[], uint bit_len, uint ele_num)
{
int fi, ji;
uint dataCount = bit_len*4;
byte *data = malloc(dataCount);
memset(data, 0x00, dataCount);

for(fi=0;fi<ele_num*2;fi++)		// dataCountではなくele_num*2なのは、要素数が足りない場合に対応する為
{
	ji = fi / 2;

	if(fi % 2)
		data[fi]=udata[ji] & 0xFF;
	else
		data[fi]=(udata[ji] >> 8) & 0xFF;
}

return writeUSBIRData(devh, freq, data, bit_len, dataCount);
}

int writeUSBIR_Plarail_Stop(struct libusb_device_handle *devh, uint band)
{
int i_ret = -1;
bool b_err = false;
ushort *code = NULL;    // 送信データ
ushort code_A_STOP[] = { 0x0016, 0x001D, 0x0016, 0x001D, 0x0016, 0x001D, 0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0280,
                         0x0016, 0x001D, 0x0016, 0x001D, 0x0016, 0x001D, 0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0000};
ushort code_B_STOP[] = { 0x0016, 0x001D, 0x0016, 0x001D, 0x0016, 0x001D, 0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0280,
                         0x0016, 0x001D, 0x0016, 0x001D, 0x0016, 0x001D, 0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0000};
if (band == PLARAIL_BAND_BAND_A)
{
    code = code_A_STOP;
}
else if (band == PLARAIL_BAND_BAND_B)
{
    code = code_B_STOP;
}
else
{
    i_ret = 2;
    b_err = true;
}

if (b_err == false)
{
    // USB DEVICEへ送信 パラメータ[USB DEVICEハンドル、周波数、送信赤外線コード、赤外線コードのビット長、要素数]
    i_ret = writeUSBIRData_Ushort(devh, IR_FREQ_DEFAULT, code, 16, 32);
    if (i_ret != 0)
    {
        i_ret = 3;
    }
}
return i_ret;
}


int writeUSBIR_Plarail_Speed_Up(struct libusb_device_handle *devh, uint band, uint dir)
{
int i_ret = -1;
bool b_err = false;
// 送信データ ON時間とOFF時間の組み合わせ
ushort *code = NULL;    // 送信データ
ushort code_A_UP_F[] = { 0x0016, 0x001D, 0x0016, 0x001D, 0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0013, 0x0013, 0x0280,
                         0x0016, 0x001D, 0x0016, 0x001D, 0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0013, 0x0016, 0x0000};
ushort code_A_UP_B[] = { 0x0016, 0x001D, 0x0016, 0x001D, 0x0034, 0x0013, 0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0016, 0x001D, 0x0034, 0x0280,
                         0x0016, 0x001D, 0x0016, 0x001D, 0x0034, 0x0013, 0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0016, 0x001D, 0x0034, 0x0000};
ushort code_B_UP_F[] = { 0x0016, 0x001D, 0x0034, 0x0013, 0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0016, 0x001D, 0x0034, 0x0013, 0x0013, 0x0280,
                         0x0016, 0x001D, 0x0034, 0x0013, 0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0016, 0x001D, 0x0034, 0x0013, 0x0016, 0x0000};
ushort code_B_UP_B[] = { 0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0016, 0x001D, 0x0034, 0x0013, 0x0016, 0x001D, 0x0016, 0x001D, 0x0034, 0x0280,
                         0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0016, 0x001D, 0x0034, 0x0013, 0x0016, 0x001D, 0x0016, 0x001D, 0x0034, 0x0000};

if (band == PLARAIL_BAND_BAND_A)
{
    if (dir == PLARAIL_DIRECTION_FORWARD)
    {
        code = code_A_UP_F;
    }
    else if (dir == PLARAIL_DIRECTION_BACKWARD)
    {
        code = code_A_UP_B;
    }
    else
    {
        i_ret = 3;
        b_err = true;
    }
}
else if (band == PLARAIL_BAND_BAND_B)
{
    if (dir == PLARAIL_DIRECTION_FORWARD)
    {
        code = code_B_UP_F;
    }
    else if (dir == PLARAIL_DIRECTION_BACKWARD)
    {
        code = code_B_UP_B;
    }
    else
    {
        i_ret = 3;
        b_err = true;
    }
}
else
{
    i_ret = 2;
    b_err = true;
}

if(b_err == false)
{
    // USB DEVICEへ送信 パラメータ[USB DEVICEハンドル、周波数、送信赤外線コード、赤外線コードのビット長、要素数]
    i_ret = writeUSBIRData_Ushort(devh, IR_FREQ_DEFAULT, code, 16, 32);
    if(i_ret != 0)
    {
        i_ret = 4;
    }
}
return i_ret;
}


int writeUSBIR_Plarail_Speed_Down(struct libusb_device_handle *devh, uint band)
{
int i_ret = -1;
bool b_err = false;
// 送信データ ON時間とOFF時間の組み合わせ
ushort *code = NULL;    // 送信データ
ushort code_A_DOWN_F[] = { 0x0016, 0x001D, 0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0016, 0x0013, 0x001D, 0x0013, 0x0280,
                           0x0016, 0x001D, 0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0013, 0x0016, 0x001D, 0x0016, 0x0000};
ushort code_B_DOWN_F[] = { 0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0013, 0x0016, 0x001D, 0x0016, 0x001D, 0x0013, 0x0280,
                           0x0016, 0x001D, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0013, 0x0034, 0x0013, 0x0016, 0x001D, 0x0016, 0x001D, 0x0016, 0x0000};

if (band == PLARAIL_BAND_BAND_A)
{
    code = code_A_DOWN_F;
    //if (dir == PLARAIL_DIRECTION.FORWARD)
    //{
    //    code = code_A_DOWN_F;
    //}
    //else if (dir == PLARAIL_DIRECTION.BACKWARD)
    //{
    //    code = code_B_DOWN_F;
    //}
    //else
    //{
    //    i_ret = 3;
    //    b_err = true;
    //}
}
else if (band == PLARAIL_BAND_BAND_B)
{
    code = code_B_DOWN_F;
    //if (dir == PLARAIL_DIRECTION.FORWARD)
    //{
    //    code = code_B_DOWN_F;
    //}
    //else if (dir == PLARAIL_DIRECTION.BACKWARD)
    //{
    //    code = code_B_DOWN_B;
    //}
    //else
    //{
    //    i_ret = 3;
    //    b_err = true;
    //}
}
else
{
    i_ret = 2;
    b_err = true;
}

if (b_err == false)
{
    // USB DEVICEへ送信 パラメータ[USB DEVICEハンドル、周波数、送信赤外線コード、赤外線コードのビット長、要素数]
    i_ret = writeUSBIRData_Ushort(devh, IR_FREQ_DEFAULT, code, 16, 32);
    if (i_ret != 0)
    {
        i_ret = 3;
    }
}
return i_ret;
}

int recUSBIRData_Start(struct libusb_device_handle *devh, uint freq)
{
int i_ret = -1;
byte outbuffer[BUFF_SIZE];
byte inbuffer[BUFF_SIZE];
int BytesWritten = 0;
int BytesRead = 0;
const uint ir_read_stop_on_time = 0x0008;       // 読み込み停止 ON時間
const uint ir_read_stop_off_time = 0x0474;      // 読み込み停止 OFF時間 30ms = 30ms / 38kHz = 1140 =0x474

// パラメータチェック
if (devh != NULL && IR_FREQ_MIN <= freq && freq <= IR_FREQ_MAX)
{
    // データセット
    outbuffer[0] = 0x31;
    outbuffer[1] = (byte)((freq >> 8) & 0xFF);
    outbuffer[2] = (byte)(freq & 0xFF);
    outbuffer[3] = 1;   // 読み込み停止フラグ　停止あり
    outbuffer[4] = (byte)((ir_read_stop_on_time >> 8) & 0xFF);      // 読み込み停止ON時間MSB
    outbuffer[5] = (byte)(ir_read_stop_on_time & 0xFF);             // 読み込み停止ON時間LSB
    outbuffer[6] = (byte)((ir_read_stop_off_time >> 8) & 0xFF);     // 読み込み停止OFF時間MSB
    outbuffer[7] = (byte)(ir_read_stop_off_time & 0xFF);            // 読み込み停止OFF時間LSB

    if(libusb_interrupt_transfer(devh, BTO_EP_OUT, outbuffer, BUFF_SIZE ,&BytesWritten, 5000) == 0)
    {
        //Now get the response packet from the firmware.
        {
            if(libusb_interrupt_transfer(devh, BTO_EP_IN, inbuffer, BUFF_SIZE, &BytesRead, 5000) == 0)
            {
                //INBuffer[0] is an echo back of the command (see microcontroller firmware).
                //INBuffer[1] contains the I/O port pin value for the pushbutton (see microcontroller firmware).  
                if (inbuffer[0] == 0x31)
                {
                    if (inbuffer[1] == 0x00)
                    {   // OK
                        i_ret = 0;
                    }
                    else
                    {
                        // NG
                        i_ret = -3;
                    }
                }
            }
            else
            {
                // NG
                i_ret = -3;
            }
        }
    }
    else
    {
        // NG
        i_ret = -3;
    }
}
else
{   // パラメータエラー
    i_ret = -2;
}
return i_ret;
}

int recUSBIRData_Stop(struct libusb_device_handle *devh)
{
int i_ret = -1;
byte outbuffer[BUFF_SIZE];
byte inbuffer[BUFF_SIZE];
int BytesWritten = 0;
int BytesRead = 0;

// パラメータチェック
if (devh != NULL)
{
    // データセット
    outbuffer[0] = 0x32;

    if(libusb_interrupt_transfer(devh, BTO_EP_OUT, outbuffer, BUFF_SIZE ,&BytesWritten, 5000) == 0)
    {
        //Now get the response packet from the firmware.
        {
            if(libusb_interrupt_transfer(devh, BTO_EP_IN, inbuffer, BUFF_SIZE, &BytesRead, 5000) == 0)
            {
                //INBuffer[0] is an echo back of the command (see microcontroller firmware).
                //INBuffer[1] contains the I/O port pin value for the pushbutton (see microcontroller firmware).  
                if (inbuffer[0] == 0x32)
                {   // OK
                    i_ret = 0;
                }
            }
            else
            {
                // NG
                i_ret = -3;
            }
        }
    }
    else
    {
        // NG
        i_ret = -3;
    }
}
else
{   // パラメータエラー
    i_ret = -2;
}
return i_ret;
}

int readUSBIRData(struct libusb_device_handle *devh, byte data[], uint data_buff_len, uint *bit_len)
{
uint fi;
int i_ret = -1;
byte outbuffer[BUFF_SIZE];
byte inbuffer[BUFF_SIZE];
int BytesWritten = 0;
int BytesRead = 0;
bool error_flag = false;

uint ir_read_data_get_byte_count = 0;

// パラメータチェック
if (devh != NULL)
//	data.Length == data_buff_len)
{
    // 出力変数初期化
    *bit_len = 0;

    // データセット
    while (error_flag == false)
    {
        outbuffer[0] = 0x33;

        if(libusb_interrupt_transfer(devh, BTO_EP_OUT, outbuffer, BUFF_SIZE ,&BytesWritten, 5000) == 0)
        {
            //Now get the response packet from the firmware.
            if(libusb_interrupt_transfer(devh, BTO_EP_IN, inbuffer, BUFF_SIZE, &BytesRead, 5000) == 0)
            {
                //INBuffer[0] is an echo back of the command (see microcontroller firmware).
                //INBuffer[1] contains the I/O port pin value for the pushbutton (see microcontroller firmware).  
                if (inbuffer[0] == 0x33)
                {
                    int tmp_total_size = 0;
                    int tmp_start_pos = 0;
                    byte tmp_read_size = 0;
                    tmp_total_size = inbuffer[1];
                    tmp_total_size = (tmp_total_size << 8) | inbuffer[2];
                    tmp_start_pos = inbuffer[3];
                    tmp_start_pos = (tmp_start_pos << 8) | inbuffer[4];
                    tmp_read_size = inbuffer[5];

                    if (tmp_total_size > 0 && tmp_total_size >= (tmp_start_pos + tmp_read_size) && tmp_read_size > 0)
                    {
                        for (fi = 0; fi < tmp_read_size; fi++)
                        {
                            if((ir_read_data_get_byte_count + 3) < data_buff_len)
                            {
                                data[ir_read_data_get_byte_count++] = inbuffer[6 + fi * 4];
                                data[ir_read_data_get_byte_count++] = inbuffer[7 + fi * 4];
                                data[ir_read_data_get_byte_count++] = inbuffer[8 + fi * 4];
                                data[ir_read_data_get_byte_count++] = inbuffer[9 + fi * 4];
                            }
                            else
                            {
                                // NG
                                error_flag = true;
                                i_ret = -5;
                            }
                        }
                    }
                    else
                    {   // 読み込み終了
                        i_ret =0;
                        *bit_len = (uint)tmp_total_size;
                        break;
                    }
                }
            }
            else
            {
                // NG
                error_flag = true;
                i_ret = -4;
            }
        }
        else
        {
            // NG
            error_flag = true;
            i_ret = -3;
        }
    }
}
else
{   // パラメータエラー
    i_ret = -2;
}
return i_ret;
}
