//i2s：STD
#pragma once

#include "sdkconfig.h"

/* Example configurations */
#define BUF_SIZE   (10000) //缓冲区大小
#define SAMPLE_RATE     (16000) //采样率
#define MCLK_MULTIPLE   (384) //主时钟频率倍数 If not using 24-bit data width, 256 should be enough
#define MCLK_FREQ_HZ    (SAMPLE_RATE * MCLK_MULTIPLE) //主时钟频率
#define EXAMPLE_VOICE_VOLUME    CONFIG_EXAMPLE_VOICE_VOLUME
#if CONFIG_EXAMPLE_MODE_ECHO
#define EXAMPLE_MIC_GAIN        CONFIG_EXAMPLE_MIC_GAIN //麦克风增益
#endif

//定义引脚
#define SEL 10
#define LRCL 15
#define DOUT 16
#define BCLK 17

#define LRCL2 4
#define BLCK2 5
#define DIN 6
//sph0645
#define I2S_NUM (I2S_NUM_0)
//#define SEL (GPIO_NUM_10)
// #define I2S_WS_IO (GPIO_NUM_11)
// #define I2S_DO_IO (-1)
// #define I2S_DI_IO (GPIO_NUM_12)
// #define I2S_BCK_IO (GPIO_NUM_13)
// #define I2S_MCK_IO (-1)
#define I2S_WS_IO (LRCL)
#define I2S_DO_IO (-1)
#define I2S_DI_IO (DOUT)
#define I2S_BCK_IO (BCLK)
#define I2S_MCK_IO (-1)

//max98357
#define I2S_NUM2 (I2S_NUM_1)
#define SEL2 (-1)
#define I2S_WS_IO2 (GPIO_NUM_4)
#define I2S_DO_IO2 (GPIO_NUM_6)
#define I2S_DI_IO2 (-1)
#define I2S_BCK_IO2 (GPIO_NUM_5)
#define I2S_MCK_IO2 (-1)
