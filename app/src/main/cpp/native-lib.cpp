#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>

extern "C" {
//编码
#include "libavcodec/avcodec.h"
//封装格式处理
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
//像素处理
#include "libswscale/swscale.h"
#include "_opensl_helper.h"
}

#define  LOG_TAG    "aruba"
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

/**
 * 视频解码成YUV文件
 */
extern "C"
JNIEXPORT void JNICALL
Java_com_aruba_ffmpegapplication_DecodeActivity_decodeVideo(JNIEnv *env, jobject instance,
                                                            jstring inputFilePath_,
                                                            jstring outputFilePath_) {
    const char *inputFilePath = env->GetStringUTFChars(inputFilePath_, 0);
    const char *outputFilePath = env->GetStringUTFChars(outputFilePath_, 0);

    //注册FFmpeg中各大组件
    av_register_all();

    //打开文件
    AVFormatContext *formatContext = avformat_alloc_context();
    if (avformat_open_input(&formatContext, inputFilePath, NULL, NULL) != 0) {
        LOGE("打开失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        env->ReleaseStringUTFChars(outputFilePath_, outputFilePath);
        return;
    }

    //将文件信息填充进AVFormatContext
    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        LOGE("获取文件信息失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        env->ReleaseStringUTFChars(outputFilePath_, outputFilePath);
        return;
    }

    //获取视频流的编解码器上下文
    AVCodecContext *codecContext = NULL;
    int vidio_stream_idx = -1;
    for (int i = 0; i < formatContext->nb_streams; ++i) {
        if (formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {//如果是视频流
            codecContext = formatContext->streams[i]->codec;
            vidio_stream_idx = i;
            break;
        }
    }

    if (codecContext == NULL) {
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        env->ReleaseStringUTFChars(outputFilePath_, outputFilePath);
        return;
    }

    //根据编解码器上下文的id获取视频流解码器
    AVCodec *codec = avcodec_find_decoder(codecContext->codec_id);
    //打开解码器
    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        LOGE("解码失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        env->ReleaseStringUTFChars(outputFilePath_, outputFilePath);
        return;
    }

    //开始读每一帧
    //存放压缩数据
    AVPacket *pkt = (AVPacket *) (av_malloc(sizeof(AVPacket)));
    av_init_packet(pkt);

    //存放解压数据
    AVFrame *picture = av_frame_alloc();

    //存放转码数据
    AVFrame *picture_yuv = av_frame_alloc();
    //为转码数据分配内存
    uint8_t *data_size = (uint8_t *) (av_malloc(
            (size_t) avpicture_get_size(AV_PIX_FMT_YUV420P, codecContext->width,
                                        codecContext->height)));
    avpicture_fill((AVPicture *) picture_yuv, data_size, AV_PIX_FMT_YUV420P, codecContext->width,
                   codecContext->height);

    int picture_ptr = 0;

    //转码组件上下文,前三个参数为原视频的宽高和编码，后三个为转码后的视频宽高和编码，还可以传入过滤器对视频做处理，这边不做处理
    SwsContext *swsContext = sws_getContext(codecContext->width, codecContext->height,
                                            codecContext->pix_fmt,
                                            codecContext->width, codecContext->height,
                                            AV_PIX_FMT_YUV420P, SWS_BILINEAR, NULL, NULL, NULL
    );

    //文件
    FILE *output_file = fopen(outputFilePath, "wb");

    while (av_read_frame(formatContext, pkt) == 0) {//读到每一帧的压缩数据存放在AVPacket
        if (pkt->stream_index == vidio_stream_idx) {
            //解码
            avcodec_decode_video2(codecContext, picture, &picture_ptr, pkt);

            if (picture_ptr > 0) {
                LOGE("picture_ptr %d", picture_ptr);

                //转码 data中存放着真实数据，linesize为一行的数据，0为转码起始位置，高度为整个画面高
                sws_scale(swsContext, picture->data, picture->linesize, 0, picture->height,
                          picture_yuv->data, picture_yuv->linesize);

                //一帧的数据大小
                size_t size_y = codecContext->width * codecContext->height;
                //写文件 y:u:v为4：1：1
                //写y
                fwrite(picture_yuv->data[0], sizeof(uint8_t), size_y, output_file);
                //写u
                fwrite(picture_yuv->data[1], sizeof(uint8_t), size_y / 4, output_file);
                //写v
                fwrite(picture_yuv->data[2], sizeof(uint8_t), size_y / 4, output_file);
            }

        }
        av_free_packet(pkt);
    }

    //关闭文件
    fclose(output_file);
    //释放资源
    sws_freeContext(swsContext);
    av_frame_free(&picture_yuv);
    av_frame_free(&picture);
    avcodec_close(codecContext);
    avformat_free_context(formatContext);
    env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
    env->ReleaseStringUTFChars(outputFilePath_, outputFilePath);
}

bool is_playing = false;

/**
 * 视频解码YUV原生播放
 */
extern "C"
JNIEXPORT jint JNICALL
Java_com_aruba_ffmpegapplication_YuvPlayView_00024PlayThread_render(JNIEnv *env, jobject instance,
                                                                    jstring filePath_,
                                                                    jobject surface) {
    const char *filePath = env->GetStringUTFChars(filePath_, 0);

    //注册FFmpeg中各大组件
    av_register_all();

    //打开文件
    AVFormatContext *formatContext = avformat_alloc_context();
    if (avformat_open_input(&formatContext, filePath, NULL, NULL) != 0) {
        LOGE("打开失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(filePath_, filePath);
        return 1;
    }

    //将文件信息填充进AVFormatContext
    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        LOGE("获取文件信息失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(filePath_, filePath);
        return 2;
    }

    //获取视频流的编解码器上下文
    AVCodecContext *codecContext = NULL;
    int vidio_stream_idx = -1;
    for (int i = 0; i < formatContext->nb_streams; ++i) {
        if (formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {//如果是视频流
            codecContext = formatContext->streams[i]->codec;
            vidio_stream_idx = i;
            break;
        }
    }

    if (codecContext == NULL) {
        LOGE("获取编解码器上下文失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(filePath_, filePath);
        return 3;
    }

    //根据编解码器上下文的id获取视频流解码器
    AVCodec *codec = avcodec_find_decoder(codecContext->codec_id);
    //打开解码器
    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        LOGE("打开解码失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(filePath_, filePath);
        return 4;
    }

    //开始读每一帧
    is_playing = true;

    //存放压缩数据
    AVPacket *pkt = (AVPacket *) (av_malloc(sizeof(AVPacket)));
    av_init_packet(pkt);

    //存放解压数据
    AVFrame *picture = av_frame_alloc();

    //存放转码数据
    AVFrame *picture_rgb = av_frame_alloc();
    //为转码数据分配内存
    uint8_t *data_size = (uint8_t *) (av_malloc(
            (size_t) avpicture_get_size(AV_PIX_FMT_RGBA, codecContext->width,
                                        codecContext->height)));
    avpicture_fill((AVPicture *) picture_rgb, data_size, AV_PIX_FMT_RGBA, codecContext->width,
                   codecContext->height);

    int picture_ptr = 0;

    //获取底层绘制对象
    ANativeWindow *aNativeWindow = ANativeWindow_fromSurface(env, surface);
    //为Window配置长宽和像素编码
    ANativeWindow_setBuffersGeometry(aNativeWindow, codecContext->width, codecContext->height,
                                     AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM);
    //视频缓存区 
    ANativeWindow_Buffer out_buff;

    //转码组件上下文,前三个参数为原视频的宽高和编码，后三个为转码后的视频宽高和编码，还可以传入过滤器对视频做处理，这边不做处理
    SwsContext *swsContext = sws_getContext(codecContext->width, codecContext->height,
                                            codecContext->pix_fmt,
                                            codecContext->width, codecContext->height,
                                            AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL
    );

    while (av_read_frame(formatContext, pkt) == 0 && is_playing) {//读到每一帧的压缩数据存放在AVPacket
        if (pkt->stream_index == vidio_stream_idx) {
            //解码
            avcodec_decode_video2(codecContext, picture, &picture_ptr, pkt);

            LOGE("picture_ptr %d", picture_ptr);
            if (picture_ptr > 0) {
                
                //转码 data中存放着真实数据，linesize为一行的数据，0为转码起始位置，高度为整个画面高
                sws_scale(swsContext, picture->data, picture->linesize, 0, picture->height,
                          picture_rgb->data, picture_rgb->linesize);
                //锁定
                ANativeWindow_lock(aNativeWindow, &out_buff, NULL);

                //将转码后的frame（picture_rgb）中的每一行数据复制到window的视频缓冲区（out_buff）的每一行
                //picture_rgb中二维数据的首地址
                uint8_t *frame_data_p = picture_rgb->data[0];
                //视频缓存区中二维数据的首地址
                uint8_t *buff_data_p = (uint8_t *) (out_buff.bits);
                //视频缓存区有多少个字节 RGBA8888占4个字节
                int destStride = out_buff.stride * 4;
                for (int i = 0; i < codecContext->height; i++) {
                    memcpy(buff_data_p, frame_data_p, picture_rgb->linesize[0]);
                    buff_data_p += destStride;
                    frame_data_p += picture_rgb->linesize[0];
                }

                ANativeWindow_unlockAndPost(aNativeWindow);
                //16ms播放一帧
                usleep(16 * 1000);
            }
        }

        av_free_packet(pkt);
    }

    //释放资源
    sws_freeContext(swsContext);
    ANativeWindow_release(aNativeWindow);
    av_frame_free(&picture_rgb);
    av_frame_free(&picture);
    free(data_size);
    avcodec_close(codecContext);
    avformat_free_context(formatContext);
    env->ReleaseStringUTFChars(filePath_, filePath);

    return 0;
}

/**
 * 结束播放
 */
extern "C"
JNIEXPORT void JNICALL
Java_com_aruba_ffmpegapplication_YuvPlayView_00024PlayThread_stopPlay(JNIEnv *env,
                                                                      jobject instance) {
    is_playing = false;
}

/**
 * 音频解码成PCM文件
 */
extern "C"
JNIEXPORT void JNICALL
Java_com_aruba_ffmpegapplication_DecodeActivity_decodeAudio(JNIEnv *env, jobject instance,
                                                            jstring inputFilePath_,
                                                            jstring outputFilePath_) {
    const char *inputFilePath = env->GetStringUTFChars(inputFilePath_, 0);
    const char *outputFilePath = env->GetStringUTFChars(outputFilePath_, 0);

    //注册FFmpeg中各大组件
    av_register_all();

    //打开文件
    AVFormatContext *formatContext = avformat_alloc_context();
    if (avformat_open_input(&formatContext, inputFilePath, NULL, NULL) != 0) {
        LOGE("打开失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        env->ReleaseStringUTFChars(outputFilePath_, outputFilePath);
        return;
    }

    //将文件信息填充进AVFormatContext
    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        LOGE("获取文件信息失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        env->ReleaseStringUTFChars(outputFilePath_, outputFilePath);
        return;
    }

    //获取视频流的编解码器上下文
    AVCodecContext *codecContext = NULL;
    int audio_stream_idx = -1;
    for (int i = 0; i < formatContext->nb_streams; ++i) {
        if (formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {//如果是音频流
            codecContext = formatContext->streams[i]->codec;
            audio_stream_idx = i;
            break;
        }
    }

    if (codecContext == NULL) {
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        env->ReleaseStringUTFChars(outputFilePath_, outputFilePath);
        return;
    }

    //根据编解码器上下文的id获取视频流解码器
    AVCodec *codec = avcodec_find_decoder(codecContext->codec_id);
    //打开解码器
    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        LOGE("解码失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        env->ReleaseStringUTFChars(outputFilePath_, outputFilePath);
        return;
    }

    //开始读每一帧
    //存放压缩数据
    AVPacket *pkt = (AVPacket *) (av_malloc(sizeof(AVPacket)));
    av_init_packet(pkt);

    //存放解压数据
    AVFrame *picture = av_frame_alloc();

    int picture_ptr = 0;

    //音频转码组件上下文
    SwrContext *swrContext = swr_alloc();
//    struct SwrContext *swr_alloc_set_opts(struct SwrContext *s,
//                                          int64_t out_ch_layout, enum AVSampleFormat out_sample_fmt, int out_sample_rate,
//                                          int64_t  in_ch_layout, enum AVSampleFormat  in_sample_fmt, int  in_sample_rate,
//                                          int log_offset, void *log_ctx);
    //AV_CH_LAYOUT_STEREO:双声道  AV_SAMPLE_FMT_S16:量化格式 16位 codecContext->sample_rate:采样率 Hz
    swr_alloc_set_opts(swrContext, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
                       codecContext->sample_rate,//输出采样率和输入采样率应相同
                       codecContext->channel_layout, codecContext->sample_fmt,
                       codecContext->sample_rate, 0, NULL
    );
    swr_init(swrContext);

    //原音频通道数
    int channel_count = av_get_channel_layout_nb_channels(codecContext->channel_layout);
    //单通道最大存放转码数据 所占字节 = 采样率*量化格式 / 8
    int out_size = 44100 * 16 / 8;
    uint8_t *out = (uint8_t *) (av_malloc(out_size));

    //文件
    FILE *output_file = fopen(outputFilePath, "wb");

    while (av_read_frame(formatContext, pkt) == 0) {//读到每一帧的压缩数据存放在AVPacket
        if (pkt->stream_index == audio_stream_idx) {
            //解码
            avcodec_decode_audio4(codecContext, picture, &picture_ptr, pkt);

            LOGE("picture_ptr %d", picture_ptr);
            if (picture_ptr > 0) {
                //转码
                swr_convert(swrContext, &out, out_size,
                            (const uint8_t **) (picture->data), picture->nb_samples);

//                int av_samples_get_buffer_size(int *linesize, int nb_channels, int nb_samples,
//                                               enum AVSampleFormat sample_fmt, int align);
                //缓冲区真实大小
                int size = av_samples_get_buffer_size(NULL, channel_count, picture->nb_samples,
                                                      AV_SAMPLE_FMT_S16, 1);
                fwrite(out, sizeof(uint8_t), size, output_file);
            }

        }
        av_free_packet(pkt);
    }

    //关闭文件
    fclose(output_file);
    //释放资源
    av_freep(out);
    swr_free(&swrContext);
    av_frame_free(&picture);
    avcodec_close(codecContext);
    avformat_free_context(formatContext);
    env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
    env->ReleaseStringUTFChars(outputFilePath_, outputFilePath);
}

/**
 * 音频解码PCM，AudioTrack播放
 */
extern "C"
JNIEXPORT void JNICALL
Java_com_aruba_ffmpegapplication_PcmPlayActivity_playByAudio(JNIEnv *env, jobject instance,
                                                             jstring inputFilePath_) {
    const char *inputFilePath = env->GetStringUTFChars(inputFilePath_, 0);

    //注册FFmpeg中各大组件
    av_register_all();

    //打开文件
    AVFormatContext *formatContext = avformat_alloc_context();
    if (avformat_open_input(&formatContext, inputFilePath, NULL, NULL) != 0) {
        LOGE("打开失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        return;
    }

    //将文件信息填充进AVFormatContext
    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        LOGE("获取文件信息失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        return;
    }

    //获取视频流的编解码器上下文
    AVCodecContext *codecContext = NULL;
    int audio_stream_idx = -1;
    for (int i = 0; i < formatContext->nb_streams; ++i) {
        if (formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {//如果是音频流
            codecContext = formatContext->streams[i]->codec;
            audio_stream_idx = i;
            break;
        }
    }

    if (codecContext == NULL) {
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        return;
    }

    //根据编解码器上下文的id获取视频流解码器
    AVCodec *codec = avcodec_find_decoder(codecContext->codec_id);
    //打开解码器
    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        LOGE("解码失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        return;
    }

    //开始读每一帧
    //存放压缩数据
    AVPacket *pkt = (AVPacket *) (av_malloc(sizeof(AVPacket)));
    av_init_packet(pkt);

    //存放解压数据
    AVFrame *picture = av_frame_alloc();

    int picture_ptr = 0;

    //音频转码组件上下文
    SwrContext *swrContext = swr_alloc();
    //AV_CH_LAYOUT_STEREO:双声道  AV_SAMPLE_FMT_S16:量化格式 16位 codecContext->sample_rate:采样率 Hz
    swr_alloc_set_opts(swrContext, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
                       codecContext->sample_rate,//输出采样率和输入采样率应相同
                       codecContext->channel_layout, codecContext->sample_fmt,
                       codecContext->sample_rate, 0, NULL
    );
    swr_init(swrContext);

    //原音频通道数
    int channel_count = av_get_channel_layout_nb_channels(codecContext->channel_layout);
    //单通道最大存放转码数据 所占字节 = 采样率*量化格式 / 8
    int out_size = 44100 * 16 / 8;
    uint8_t *out = (uint8_t *) (av_malloc(out_size));

    //调用java层create方法
    jclass jclz = env->GetObjectClass(instance);
    jmethodID create_method_id = env->GetMethodID(jclz, "create", "(II)V");
    env->CallVoidMethod(instance, create_method_id, 44100,
                        av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO));
    jmethodID play_method_id = env->GetMethodID(jclz, "play", "([BI)V");

    while (av_read_frame(formatContext, pkt) == 0) {//读到每一帧的压缩数据存放在AVPacket
        if (pkt->stream_index == audio_stream_idx) {
            //解码
            avcodec_decode_audio4(codecContext, picture, &picture_ptr, pkt);

            LOGE("picture_ptr %d", picture_ptr);
            if (picture_ptr > 0) {
                //转码
                swr_convert(swrContext, &out, out_size,
                            (const uint8_t **) (picture->data), picture->nb_samples);

                //缓冲区真实大小
                int size = av_samples_get_buffer_size(NULL, channel_count, picture->nb_samples,
                                                      AV_SAMPLE_FMT_S16, 1);

                jbyteArray array = env->NewByteArray(size);
                env->SetByteArrayRegion(array, 0, size, (const jbyte *) (out));
                env->CallVoidMethod(instance, play_method_id, array, size);
                env->DeleteLocalRef(array);
            }
        }
        av_free_packet(pkt);
    }

    //释放资源
    av_freep(out);
    swr_free(&swrContext);
    av_frame_free(&picture);
    avcodec_close(codecContext);
    avformat_free_context(formatContext);
    env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
}


OpenslHelper helper;
uint8_t *out;
int buff_size;
//char *filePath;
AVFormatContext *formatContext;
AVCodecContext *codecContext;
int audio_stream_idx;
AVPacket *pkt;
AVFrame *picture;
SwrContext *swrContext;
int channel_count;
int out_size;

void playerCallback(SLAndroidSimpleBufferQueueItf bq, void *pContext);

/**
 * 音频解码PCM，OpenSL播放
 */
extern "C"
JNIEXPORT void JNICALL
Java_com_aruba_ffmpegapplication_PcmPlayActivity_playByOpenSL(JNIEnv *env, jobject instance,
                                                              jstring inputFilePath_) {
    //初始化opensl
    SLresult result = helper.createEngine();
    if (!helper.isSuccess(result)) {
        LOGE("createEngine失败");
        return;
    }
    result = helper.createMix();
    if (!helper.isSuccess(result)) {
        LOGE("createMix失败");
        return;
    }

    const char *inputFilePath = env->GetStringUTFChars(inputFilePath_, 0);
//    const int size = sizeof(inputFilePath);
//    filePath = static_cast<char *>(malloc(size));
//    memcpy(filePath, inputFilePath, size);

    //注册FFmpeg中各大组件
    av_register_all();
    avformat_network_init();

    //打开文件
    formatContext = avformat_alloc_context();
    if (avformat_open_input(&formatContext, inputFilePath, NULL, NULL) != 0) {
        LOGE("打开失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        return;
    }

    //将文件信息填充进AVFormatContext
    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        LOGE("获取文件信息失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        return;
    }

    //获取视频流的编解码器上下文
    codecContext = NULL;
    audio_stream_idx = -1;
    for (int i = 0; i < formatContext->nb_streams; ++i) {
        if (formatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {//如果是音频流
            codecContext = formatContext->streams[i]->codec;
            audio_stream_idx = i;
            break;
        }
    }

    if (codecContext == NULL) {
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        return;
    }

    //根据编解码器上下文的id获取视频流解码器
    AVCodec *codec = avcodec_find_decoder(codecContext->codec_id);
    //打开解码器
    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        LOGE("解码失败");
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
        return;
    }

    //开始读每一帧
    //存放压缩数据
    pkt = (AVPacket *) (av_malloc(sizeof(AVPacket)));
    av_init_packet(pkt);

    //存放解压数据
    picture = av_frame_alloc();

    //音频转码组件上下文
    swrContext = swr_alloc();
    //AV_CH_LAYOUT_STEREO:双声道  AV_SAMPLE_FMT_S16:量化格式 16位 codecContext->sample_rate:采样率 Hz
    swr_alloc_set_opts(swrContext, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
                       codecContext->sample_rate,//输出采样率和输入采样率应相同
                       codecContext->channel_layout, codecContext->sample_fmt,
                       codecContext->sample_rate, 0, NULL
    );
    swr_init(swrContext);

    //原音频通道数
    channel_count = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    //单通道最大存放转码数据 所占字节 = 采样率*量化格式 / 8
    out_size = 44100 * 16 / 8;
    out = (uint8_t *) (av_malloc(out_size));

    //开始播放
    result = helper.createPlayer(2, SL_SAMPLINGRATE_44_1, SL_PCMSAMPLEFORMAT_FIXED_16,
                                 SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT);
    if (!helper.isSuccess(result)) {
        LOGE("createPlayer失败");
        //释放资源
        av_free(out);
        out = NULL;
        swr_free(&swrContext);
        av_frame_free(&picture);
        avcodec_close(codecContext);
        avformat_free_context(formatContext);
        env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);

        return;
    }

    helper.registerCallback(playerCallback);
    helper.play();
    playerCallback(helper.bufferQueueItf, NULL);

    env->ReleaseStringUTFChars(inputFilePath_, inputFilePath);
}

void release() {
    //释放资源
    av_free_packet(pkt);
    av_freep(out);
    out = NULL;
    swr_free(&swrContext);
    av_frame_free(&picture);
    avcodec_close(codecContext);
    avformat_free_context(formatContext);
}

void getData(uint8_t **out, int *buff_size) {
    if (out == NULL || buff_size == NULL) {
        return;
    }
    int picture_ptr = 0;

    while (av_read_frame(formatContext, pkt) == 0) {//读到每一帧的压缩数据存放在AVPacket
        if (pkt->stream_index == audio_stream_idx) {
            //解码
            avcodec_decode_audio4(codecContext, picture, &picture_ptr, pkt);

            LOGE("picture_ptr %d", picture_ptr);
            if (picture_ptr > 0) {
                //转码
                swr_convert(swrContext, out, out_size,
                            (const uint8_t **) (picture->data), picture->nb_samples);

                //缓冲区真实大小
                *buff_size = av_samples_get_buffer_size(NULL, channel_count, picture->nb_samples,
                                                        AV_SAMPLE_FMT_S16, 1);
                break;
            }
        }
    }

    av_free_packet(pkt);
}

/**
   * 播放器会不断调用此函数，我们需要在此回调中不断给缓冲区填充数据
   * @param bufferQueueItf 
   * @param pContext 
   */
void playerCallback(SLAndroidSimpleBufferQueueItf bq, void *pContext) {
    if (helper.playState == SL_PLAYSTATE_PLAYING) {
        getData(&out, &buff_size);
        if (out != NULL && buff_size != 0) {
            (*bq)->Enqueue(bq, out, (SLuint32) (buff_size));
        }
    } else if (helper.playState == SL_PLAYSTATE_STOPPED) {
        release();
        helper.~OpenslHelper();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_aruba_ffmpegapplication_PcmPlayActivity_stopByOpenSL(JNIEnv *env, jobject instance) {
    helper.stop();
}