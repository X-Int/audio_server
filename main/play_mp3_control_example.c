#include <stdio.h>
#include <string.h>
//任务管理
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//日志输出
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_check.h"
//i2s
#include "driver/i2s_std.h"
#include "i2sconfigure.h"
//wav
#include "wav_header.h"
//adf音频处理框架
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
//音频流处理
#include "i2s_stream.h"
#include "spiffs_stream.h"
//MP3解码
#include "mp3_decoder.h"
//外围设备的初始化及其管理
#include "esp_peripherals.h"
#include "periph_spiffs.h"
//初始化和控制硬件
#include "board.h"

//日志标签
static const char *TAG = "RECORD_TO_WAV";
//I2S:
static i2s_chan_handle_t tx_handle = NULL;
static i2s_chan_handle_t rx_handle = NULL;

static i2s_chan_handle_t tx_handle2 = NULL;
static i2s_chan_handle_t rx_handle2 = NULL;
static const char err_reason[][30] = {"input param is invalid",
                                      "operation timeout"
                                     };

uint8_t* record_data;//存储从sph0645中读取的数据
size_t record_size;
//wav
const int WAVE_HEADER_SIZE = PCM_WAV_HEADER_SIZE;
//音乐片段：
static struct marker{
    int pos;
    const uint8_t *start;
    const uint8_t *end;
} file_marker;

extern const uint8_t lr_mp3_start[] asm("_binary_music_16b_2c_44100hz_mp3_start");
extern const uint8_t lr_mp3_end[]   asm("_binary_music_16b_2c_44100hz_mp3_end");



void serPrint(uint8_t* wav_buffer){
  /*打印录音wav文件的header以及前五十个音频样本*/
  int buffer_size = record_size;
  /*打印wav文件*/
  printf("WAV Buffer Size: %u bytes\n", buffer_size);
  // 打印 WAV 文件头（前 44 字节）
  printf("WAV Header:\n");
  for (size_t i = 0; i < WAVE_HEADER_SIZE; i++) {
    printf("0x%02X ", wav_buffer[i]);
    if ((i + 1) % 16 == 0) printf("\n");  // 每 16 字节换行
  }
  printf("\n");
  printf("Audio Data:\n");
  int rows = 1000;//打印五十个样本值
  uint8_t* ptr = (uint8_t*)(wav_buffer + WAVE_HEADER_SIZE);
  for(int i = 0; i < rows;i++){
    printf("%d",*ptr);
    ptr ++;
    printf(",");
    if((i + 1)% 50 == 0){
      printf("\n");
    }
  }
}

static esp_err_t sph0645_driver_init()
{
    /*初始化SPH0645的I2S*/
//设定I2S默认参数：编号 主/从设备
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // 自动清除DMA缓冲区的旧数据
//创建一个rx_handle
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));
//配置I2S模式：
    i2s_std_config_t rx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),//时钟配置
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
//引脚配置
        .gpio_cfg = {
            .mclk = I2S_MCK_IO,
            .bclk = I2S_BCK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_DO_IO,
            .din = I2S_DI_IO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
             },
        },
    };
    rx_std_cfg.clk_cfg.mclk_multiple = MCLK_MULTIPLE;
    rx_std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &rx_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    return ESP_OK;
}

static esp_err_t max_driver_init()
{
    /*初始化MAX98375的I2S*/
//设定I2S默认参数：编号 主/从设备
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // 自动清除DMA缓冲区的旧数据
//创建一个tx_handle
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));
//配置I2S模式：
    i2s_std_config_t tx_std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
        .mclk = I2S_GPIO_UNUSED,    // some codecs may require mclk signal, this example doesn't need it
        .bclk = I2S_BCK_IO2,
        .ws   = I2S_WS_IO2,
        .dout = I2S_DO_IO2,
        .din  = I2S_DI_IO2,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv   = false,
        },
    },
};
    tx_std_cfg.clk_cfg.mclk_multiple = MCLK_MULTIPLE;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &tx_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

    return ESP_OK;
}



static esp_err_t i2s_driver_init(i2s_port_t i2s_num,int8_t i2s_bck, int8_t i2s_ws, int8_t i2s_dout, 
                                int8_t i2s_din, int8_t i2s_mck,i2s_chan_handle_t* tx0,
                                i2s_chan_handle_t* rx0)
{
    /*初始化SPH0645的I2S*/
    //设定I2S默认参数：编号 主/从设备
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(i2s_num, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // 自动清除DMA缓冲区的旧数据
    //创建两个通道 一个tx_handle 一个rx_handle
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, tx0, rx0));
    ESP_LOGI(TAG, "TX Handle: %p, RX Handle: %p", *tx0, *rx0);
    //配置I2S模式：
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),//时钟配置
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),//位宽16 立体声
        //引脚配置
        .gpio_cfg = {
            .mclk = i2s_mck,
            .bclk = i2s_bck,
            .ws = i2s_ws,
            .dout = i2s_dout,
            .din = i2s_din,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    std_cfg.clk_cfg.mclk_multiple = MCLK_MULTIPLE;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*tx0, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(*rx0, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(*tx0));
    ESP_ERROR_CHECK(i2s_channel_enable(*rx0));
    return ESP_OK;
}



//录制音频并存为wav文件
static uint8_t* i2s_record(size_t *out_size){
    //初始化 sph0645-I2S
    printf("sph0645 initial start:\n");
    if (sph0645_driver_init() != ESP_OK) {
        ESP_LOGE(TAG, "sph0645 driver init failed");
        abort();
    } else {
        ESP_LOGI(TAG, "sph0645 driver init success");
    }
    if (rx_handle == NULL) {
        ESP_LOGE(TAG, "RX handle is NULL");
        return NULL;
    }

    //初始化wav头文件：
    i2s_data_bit_width_t rx_data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
    i2s_slot_mode_t rxSlotMode = I2S_SLOT_MODE_MONO;
    uint32_t sample_rate = SAMPLE_RATE;
    uint16_t sample_width = (uint16_t)rx_data_bit_width;
    uint16_t num_channels = (uint16_t)rxSlotMode;
    size_t rec_size = BUF_SIZE;//录音字节大小数目
    const pcm_wav_header_t wav_header = PCM_WAV_HEADER_DEFAULT(rec_size, sample_width, sample_rate, num_channels);
    *out_size = 0;
    //输出头文件的信息
    ESP_LOGI(TAG,"Record WAV: rate:%lu, bits:%u, channels:%u, size:%zu", sample_rate, sample_width, num_channels, rec_size);

    //输入头文件
    uint8_t* wav_buf = (uint8_t*) malloc(rec_size + WAVE_HEADER_SIZE);
    if(wav_buf == NULL){
        ESP_LOGI(TAG,"Failed to allocate WAV buffer with size %u", rec_size + WAVE_HEADER_SIZE);
        return NULL;
    }
    memcpy(wav_buf, &wav_header, WAVE_HEADER_SIZE);//复制进入wav头文件
    //读取数据：
    esp_err_t ret = ESP_OK;
    size_t bytes_read = 0;//读取字节数

    /*  (void*)(wav_buf + WAVE_HEADER_SIZE):存储读取数据
        rec_size ：音频数据大小
        bytes_read：返回读取的字节数
    */
    ret = i2s_channel_read(rx_handle,(uint8_t*)(wav_buf + WAVE_HEADER_SIZE),rec_size,&bytes_read,portMAX_DELAY);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Read %d bytes from I2S", bytes_read);
        *out_size = rec_size + WAVE_HEADER_SIZE;
        serPrint(wav_buf);
        return wav_buf;
    } else {
        ESP_LOGE(TAG, "Failed to read from I2S, error code: %d", ret);
    }
    free(wav_buf);
    return NULL;
}   

//播放音频
static void i2s_music(uint8_t *args,size_t data_size)
{
    printf("max initial start:\n");
    if (max_driver_init() != ESP_OK) {
        ESP_LOGE(TAG, "max driver init failed");
        abort();
    } else {
        ESP_LOGI(TAG, "max driver init success");
    }
    if (tx_handle == NULL) {
        ESP_LOGE(TAG, "TX handle is NULL");
    }

    esp_err_t ret = ESP_OK;
    size_t bytes_write = BUF_SIZE;
    uint8_t* data_ptr = args;

    //预加载数据到I2S中
    ESP_ERROR_CHECK(i2s_channel_disable(tx_handle));
    ESP_ERROR_CHECK(i2s_channel_preload_data(tx_handle, data_ptr, data_size, &bytes_write));

    //播放音频
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    while (1) {
        ret = i2s_channel_write(tx_handle, data_ptr, data_size, &bytes_write, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[music] i2s write failed, %s", err_reason[ret == ESP_ERR_TIMEOUT]);
            abort();
        }
        if (bytes_write > 0) {
            ESP_LOGI(TAG, "[music] i2s music played, %d bytes are written.", bytes_write);
        } else {
            ESP_LOGE(TAG, "[music] i2s music play failed.");
            abort();
        }
        //vTaskDelay(1000 / portTICK_PERIOD_MS);//延时1s
    }
    //vTaskDelete(NULL);
}



int mp3_music_read(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx)
{
    int read_size = file_marker.end - file_marker.start - file_marker.pos;
    if (read_size == 0) {
        return AEL_IO_DONE;
    } else if (len < read_size) {
        read_size = len;
    }
    memcpy(buf, file_marker.start + file_marker.pos, read_size);
    memcpy(record_data, buf, read_size);
    file_marker.pos += read_size;
    return read_size;
}

void mp3decoder_read(){
    audio_element_handle_t mp3_decoder;
    mp3_decoder_cfg_t mp3_decoder_cfg = DEFAULT_MP3_DECODER_CONFIG();
    mp3_decoder = mp3_decoder_init(&mp3_decoder_cfg);
    if (mp3_decoder == NULL) {
        ESP_LOGE("MP3", "MP3解码器初始化失败");
        return;
    }
    audio_element_set_read_cb(mp3_decoder, mp3_music_read, NULL);
}

int volume_adjustment_read(audio_element_handle_t el, char *buf, int len, TickType_t wait_time, void *ctx) {
    // 获取音频数据
    int read_size = file_marker.end - file_marker.start - file_marker.pos;
    if (read_size == 0) {
        return AEL_IO_DONE;
    } else if (len < read_size) {
        read_size = len;
    }
    uint8_t *audio_data = (uint8_t *)buf;
    float volume_factor = 0.1f; // 音量因子 0.5表示减半，2.0表示放大两倍
    for (int i = 0; i < read_size; i++) {
        audio_data[i] = (uint8_t)(audio_data[i] * volume_factor);  // 调整音量
        // 限制音量范围，避免溢出
        if (audio_data[i] > 256) {
            audio_data[i] = 256;
        } 
    }
    file_marker.pos += read_size;
    return read_size;
}

void play_mp3() {
    esp_err_t ret = ESP_OK;
    //1.音频文件设置
    file_marker.pos = 0;
    file_marker.start = lr_mp3_start;
    file_marker.end = lr_mp3_end;
    
    audio_pipeline_handle_t pipeline;//音频管道的句柄
    audio_element_handle_t i2s_stream_writer, mp3_decoder;//i2s流写入器 MP3解码器
    //2.初始化音频管道
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);
    // 3.MP3解码器配置
    mp3_decoder_cfg_t mp3_decoder_cfg = DEFAULT_MP3_DECODER_CONFIG();
        // 初始化MP3解码器
    mp3_decoder = mp3_decoder_init(&mp3_decoder_cfg);
    if (mp3_decoder == NULL) {
        ESP_LOGE("MP3", "MP3解码器初始化失败");
        return;
    }
        //设置回调函数
    ESP_LOGE("MP3", "设定MP3解码器回调函数");
    ret = audio_element_set_read_cb(mp3_decoder, mp3_music_read, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE("MP3", "设置读取回调失败");
        audio_element_deinit(mp3_decoder);
        return;
    }
    //4.i2s流音频数据配置
    ESP_LOGE(TAG, "设定i2s流");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    //5.音量调节元素、
    ESP_LOGE(TAG, "设定音量调节元素");
    audio_pipeline_handle_t volume_element;
    audio_element_cfg_t volume_cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    volume_element = audio_element_init(&volume_cfg);
    ret = audio_element_set_read_cb(volume_element, volume_adjustment_read, NULL);//回调函数
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置音量调整回调失败");
        return;
    }

        //MP3解码器和I2S流到音频管道
    audio_pipeline_register(pipeline, mp3_decoder, "mp3");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");
    //audio_pipeline_register(pipeline,volume_element,"volume");
        //音频数据流：mp3-->i2s
    const char *link_tag[] = {"mp3", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);

    //5、音频播放
    audio_pipeline_run(pipeline);
    // while (1) 
    // {
    //     audio_event_iface_msg_t msg;
    //     esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
    //     if (ret != ESP_OK) {
    //         continue;
    //     }
    //     if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) mp3_decoder
    //         && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
    //         audio_element_info_t music_info = {0};
    //         audio_element_getinfo(mp3_decoder, &music_info);
    //         ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d",
    //                 music_info.sample_rates, music_info.bits, music_info.channels);
    //         i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
    //         continue;
    //     }
    // }
    //6.清理
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);
    audio_pipeline_unregister(pipeline, mp3_decoder);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);
    ESP_LOGI(TAG, "MP3播放完成");

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Make sure audio_pipeline_remove_listener is called before destroying event_iface */
    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(mp3_decoder);
    audio_element_deinit(volume_element);
}


void app_main(void){
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    //初始化外设设备：
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    //挂载SPIFFS文件系统：
    ESP_LOGI(TAG, "[ 1 ] Mount spiffs");
    periph_spiffs_cfg_t spiffs_cfg = {
        .root = "/spiffs",//文件根目录路径
        .partition_label = NULL,
        .max_files = 5,//最大可存储文件数目
        .format_if_mount_failed = true
    };
    esp_periph_handle_t spiffs_handle = periph_spiffs_init(&spiffs_cfg);
    esp_periph_start(set, spiffs_handle);
    while (!periph_spiffs_is_mounted(spiffs_handle)) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    //存储数据
    vTaskDelay(1000 / portTICK_PERIOD_MS);//延时1s

    record_data = i2s_record(&record_size);
    //读取wavheader:
    pcm_wav_header_t* wav_test_header = (pcm_wav_header_t*)record_data;
    uint16_t bit_depth = wav_test_header->fmt_chunk.bits_per_sample;
    uint32_t sample_rate = wav_test_header->fmt_chunk.sample_rate;
    uint16_t num_of_channels = wav_test_header->fmt_chunk.num_of_channels;
    printf("Bit depth:%u\n",bit_depth);
    printf("Sample rate:%lu\n",sample_rate);
    printf("Number of channels:%u\n",num_of_channels);
    //存储为wav文件到flash
    ESP_LOGI(TAG, "Write file");
    FILE* f = fopen("/spiffs/record_test1.wav","w");
    if(f == NULL){
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    size_t written = fwrite(record_data,1,record_size,f);
    if (written != record_size) {
        ESP_LOGE(TAG, "Error writing data to file");
    } else {
        ESP_LOGI(TAG, "Data written successfully, written %d bytes", written);
    }
    fclose(f);
    printf("record_data:\n");
    serPrint(record_data);
    //max98375播放音频
    i2s_music(record_data,record_size);
}

