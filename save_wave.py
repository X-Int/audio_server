import serial
import wave
import struct

# 配置串口参数
SERIAL_PORT = 'COM14'  # 例如，Windows上可能是 'COM3'
BAUD_RATE = 115200
SAMPLE_RATE = 16000  # 假设采样率为16000Hz
CHANNELS = 1  # 单声道
SAMPLE_WIDTH = 2  # 每个样本16位，所以是2字节
FRAMES_PER_BUFFER = 1024  # 一次读取的帧数

# 打开串口
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)

# 打开一个WAV文件用于写入
output_file = 'output_audio.wav'
with wave.open(output_file, 'wb') as wf:
    # 设置WAV文件的参数
    wf.setnchannels(CHANNELS)  # 通道数
    wf.setsampwidth(SAMPLE_WIDTH)  # 采样宽度（字节数，16位是2字节）
    wf.setframerate(SAMPLE_RATE)  # 采样率

    print("开始接收音频数据...")

    try:
        while True:
            # 从串口读取音频数据，大小为1024帧
            audio_data = ser.read(FRAMES_PER_BUFFER * SAMPLE_WIDTH)  # 每个样本2字节
            if not audio_data:
                break

            # 将字节数据写入WAV文件
            wf.writeframes(audio_data)

            # 如果你想看到进度，可以打印
            print(f"写入了 {len(audio_data)} 字节的数据")

    except KeyboardInterrupt:
        print("接收中断，保存音频文件结束")
        pass

    print(f"音频数据已保存为 {output_file}")
