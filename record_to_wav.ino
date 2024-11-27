#include "ESP_I2S.h"
#include "FS.h"
#include "SPIFFS.h"
#include "wav_header.h"

//定义SPH6045引脚
#define SEL 10
#define LRCL 11
#define DOUT 12
#define BCLK 13
//定义max98357引脚
#define LRCL2 4
#define BLCK2 5
#define DIN 6
//配置I2S引脚
const uint8_t I2S_SCK = BCLK;//BLCK
const uint8_t I2S_WS = LRCL;//LRCL
const uint8_t I2S_DIN = DOUT;//dout 音频输出到esp输入端

const uint8_t I2S_SCK2 = BLCK2;
const uint8_t I2S_WS2 = LRCL2;
const uint8_t I2S_DOUT2 = DIN;
// 存储数据
uint8_t *wav_buffer;
size_t wav_size;
//wav头文件
const int WAVE_HEADER_SIZE = PCM_WAV_HEADER_SIZE;

void writeWavHeader(File &file, size_t data_size) {
  // 写入WAV头的内容，包括文件头、格式描述符等
  uint32_t file_size = data_size + 44 - 8; // 文件头是44字节
  uint32_t data_offset = 44; // 数据区域偏移量
  uint16_t channels = 1; // 单声道
  uint16_t bits_per_sample = 16; // 每个采样点的位数
  uint32_t sample_rate = 16000; // 采样率
  // RIFF头
  file.write((const uint8_t*)"RIFF", 4); // "RIFF"
  file.write((uint8_t*)&file_size, 4); // 文件大小
  file.write((const uint8_t*)"WAVE", 4); // "WAVE"
  // 格式部分
  file.write((const uint8_t*)"fmt ", 4); // "fmt "
  uint32_t fmt_chunk_size = 16; // fmt块大小
  file.write((uint8_t*)&fmt_chunk_size, 4); // fmt块大小
  uint16_t audio_format = 1; // PCM格式
  file.write((uint8_t*)&audio_format, 2); // 音频格式
  file.write((uint8_t*)&channels, 2); // 通道数
  file.write((uint8_t*)&sample_rate, 4); // 采样率
  uint32_t byte_rate = sample_rate * channels * bits_per_sample / 8; // 字节率
  file.write((uint8_t*)&byte_rate, 4); // 字节率
  uint16_t block_align = channels * bits_per_sample / 8; // 数据块对齐
  file.write((uint8_t*)&block_align, 2); // 数据块对齐
  file.write((uint8_t*)&bits_per_sample, 2); // 每个采样点的位数
  // 数据部分
  file.write((const uint8_t*)"data", 4); // "data"
  file.write((uint8_t*)&data_size, 4); // 数据大小
}

void serPrint(uint8_t* wav_buffer){
  /*打印录音wav文件的header以及前五十个音频样本*/
  int buffer_size = wav_size;
  /*打印wav文件*/
  Serial.printf("WAV Buffer Size: %lu bytes\n", buffer_size);
  // 打印 WAV 文件头（前 44 字节）
  Serial.println("WAV Header:");
  for (size_t i = 0; i < WAVE_HEADER_SIZE; i++) {
    Serial.printf("0x%02X ", wav_buffer[i]);
    if ((i + 1) % 16 == 0) Serial.println();  // 每 16 字节换行
  }
  Serial.println();
  Serial.println("Audio Data:");
  int rows = 50;//打印五十个样本值
  uint8_t* ptr = (uint8_t*)(wav_buffer + WAVE_HEADER_SIZE);
  for(int i = 0; i < rows;i++){
    Serial.print(*ptr);
    ptr ++;
    Serial.print(",");
    if((i + 1)% 50 == 0){
      Serial.println(' ');
    }
  }
}

void setup() {
  // 初始化串口：
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  //初始化sph0645的I2S：
  I2SClass i2s;
  i2s.setPins(I2S_SCK, I2S_WS, -1, I2S_DIN);
  // Initialize the I2S bus in standard mode
  //slot mode:单声道
  if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
    Serial.println("Failed to initialize I2S bus!");
    return;
  }
  // 5s音频数据
  wav_buffer = i2s.recordWAV(5, &wav_size);
  serPrint(wav_buffer);
  if (wav_buffer == nullptr) {
    Serial.println("录音失败");
    return;
  }
  Serial.println("录音成功!");

  //初始化SPIFFS:
   if (!SPIFFS.begin(true)) {  // 如果文件系统初始化失败，尝试格式化
    Serial.println("SPIFFS初始化失败");
    return;
  }
  Serial.println("SPIFFS初始化成功");

  //写入wav文件
  File wav_file = SPIFFS.open("/test_audio1.wav", "w");
  if (!wav_file) {
    Serial.println("无法打开文件进行写入");
    return;
  }
  //writeWavHeader(wav_file, wav_size);//写入wav文件头
  wav_file.write(wav_buffer, wav_size);
  wav_file.close();
  Serial.println("录音文件已保存");

  //读取wavheader:
  pcm_wav_header_t* wav_test_header = (pcm_wav_header_t*)wav_buffer;
  uint16_t bit_depth = wav_test_header->fmt_chunk.bits_per_sample;
  uint32_t sample_rate = wav_test_header->fmt_chunk.sample_rate;
  uint16_t num_of_channels = wav_test_header->fmt_chunk.num_of_channels;
  Serial.print("Bit depth: ");
  Serial.println(bit_depth);
  Serial.print("Sample rate: ");
  Serial.println(sample_rate);
  Serial.print("Number of channels: ");
  Serial.println(num_of_channels);

  //max98357 I2S初始化
  I2SClass I2S_MAX;
  I2S_MAX.setPins(I2S_SCK2,I2S_WS2,I2S_DOUT2);
  I2S_MAX.begin(I2S_MODE_STD,16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO,I2S_STD_SLOT_LEFT);
  if(! I2S_MAX.begin(I2S_MODE_STD,16000, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO)){
    Serial.println("Failed to initialize I2S_MAX bus!");
    return;
  }
  I2S_MAX.playWAV(wav_buffer,wav_size);
  Serial.println("播放完成");
 }

void loop() {}