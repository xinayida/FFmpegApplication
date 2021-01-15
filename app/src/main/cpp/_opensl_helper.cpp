//
// Created by aruba on 2020/7/6.
//

#include "_opensl_helper.h"

/**
 * 是否成功
 * @param result 
 * @return 
 */
bool OpenslHelper::isSuccess(SLresult &result) {
    if (result == SL_RESULT_SUCCESS) {
        return true;
    }

    return false;
}

/**
 * 创建opensl引擎接口
 * @return SLresult
 */
SLresult OpenslHelper::createEngine() {
    //创建引擎
    result = slCreateEngine(&engine, 0, NULL, 0, NULL, NULL);
    if (!isSuccess(result)) {
        return result;
    }

    //实例化引擎,第二个参数为：是否异步
    result = (*engine)->Realize(engine, SL_BOOLEAN_FALSE);
    if (!isSuccess(result)) {
        return result;
    }

    //获取引擎接口
    result = (*engine)->GetInterface(engine, SL_IID_ENGINE, &engineInterface);
    if (!isSuccess(result)) {
        return result;
    }

    return result;
}

/**
 * 创建混音器
 * @return SLresult
 */
SLresult OpenslHelper::createMix() {
    //获取混音器
    result = (*engineInterface)->CreateOutputMix(engineInterface, &mix, 0,
                                                 0, 0);
    if (!isSuccess(result)) {
        return result;
    }

    //实例化混音器
    result = (*mix)->Realize(mix, SL_BOOLEAN_FALSE);
    if (!isSuccess(result)) {
        return result;
    }

    //获取环境混响混音器接口
    SLresult environmentalResult = (*mix)->GetInterface(mix, SL_IID_ENVIRONMENTALREVERB,
                                                        &environmentalReverbItf);
    if (isSuccess(environmentalResult)) {
        //给混音器设置环境
        (*environmentalReverbItf)->SetEnvironmentalReverbProperties(environmentalReverbItf,
                                                                    &settings);
    }

    return result;
}

/**
 * 创建播放器
 * @param numChannels   声道数
 * @param samplesRate   采样率 SL_SAMPLINGRATE_XX
 * @param bitsPerSample 采样位数（量化格式） SL_PCMSAMPLEFORMAT_FIXED_XX
 * @param channelMask   立体声掩码 SL_SPEAKER_XX | SL_SPEAKER_XX
 * @return SLresult
 */
SLresult OpenslHelper::createPlayer(int numChannels, long samplesRate, int bitsPerSample,
                                    int channelMask) {
    //1.关联音频流缓冲区  设为2是防止延迟 可以在播放另一个缓冲区时填充新数据
    SLDataLocator_AndroidSimpleBufferQueue buffQueque = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                         2};

    //缓冲区格式
//    typedef struct SLDataFormat_PCM_ {
//        SLuint32 		formatType;    //格式pcm
//        SLuint32 		numChannels;   //声道数
//        SLuint32 		samplesPerSec; //采样率
//        SLuint32 		bitsPerSample; //采样位数（量化格式）
//        SLuint32 		containerSize; //包含位数
//        SLuint32 		channelMask;   //立体声
//        SLuint32		endianness;    //结束标志位
//    } SLDataFormat_PCM;
    SLDataFormat_PCM dataFormat_pcm = {SL_DATAFORMAT_PCM, (SLuint32)numChannels, (SLuint32)samplesRate, (SLuint32)bitsPerSample,
                                       (SLuint32) bitsPerSample, (SLuint32)channelMask,
                                       SL_BYTEORDER_LITTLEENDIAN};

    //存放缓冲区地址和格式地址的结构体
//    typedef struct SLDataSource_ {
//        void *pLocator;//缓冲区
//        void *pFormat;//格式
//    } SLDataSource;
    SLDataSource audioSrc = {&buffQueque, &dataFormat_pcm};

    //2.关联混音器
//    typedef struct SLDataLocator_OutputMix {
//        SLuint32 		locatorType;
//        SLObjectItf		outputMix;
//    } SLDataLocator_OutputMix;
    SLDataLocator_OutputMix dataLocator_outputMix = {SL_DATALOCATOR_OUTPUTMIX, mix};

    //混音器快捷方式
//    typedef struct SLDataSink_ {
//        void *pLocator;
//        void *pFormat;
//    } SLDataSink;
    SLDataSink audioSnk = {&dataLocator_outputMix, NULL};

    //3.通过引擎接口创建播放器
//    SLresult (*CreateAudioPlayer) (
//            SLEngineItf self,
//            SLObjectItf * pPlayer,
//            SLDataSource *pAudioSrc,
//            SLDataSink *pAudioSnk,
//            SLuint32 numInterfaces,
//            const SLInterfaceID * pInterfaceIds,
//            const SLboolean * pInterfaceRequired
//    );
    SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND, SL_IID_VOLUME};
    SLboolean required[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    result = (*engineInterface)->CreateAudioPlayer(engineInterface, &player, &audioSrc, &audioSnk,
                                                   3, ids,
                                                   required);
    if (!isSuccess(result)) {
        return result;
    }

    //播放器实例化
    result = (*player)->Realize(player, SL_BOOLEAN_FALSE);
    if (!isSuccess(result)) {
        return result;
    }

    //获取播放接口
    result = (*player)->GetInterface(player, SL_IID_PLAY, &playInterface);
    if (!isSuccess(result)) {
        return result;
    }

    result = (*player)->GetInterface(player, SL_IID_VOLUME, &volumeItf);
    if (!isSuccess(result)) {
        return result;
    }

    //注册缓冲区
    result = (*player)->GetInterface(player, SL_IID_BUFFERQUEUE, &bufferQueueItf);
    if (!isSuccess(result)) {
        return result;
    }

    return result;
}

/**
 * 设置回调接口
 * @return SLresult
 */
SLresult OpenslHelper::registerCallback(slAndroidSimpleBufferQueueCallback callback) {
    //设置回调接口
    result = (*bufferQueueItf)->RegisterCallback(bufferQueueItf, callback, NULL);
    return result;
}

//播放
SLresult OpenslHelper::play() {
    playState = SL_PLAYSTATE_PLAYING;
    result = (*playInterface)->SetPlayState(playInterface, SL_PLAYSTATE_PLAYING);

    return result;
}

//暂停
SLresult OpenslHelper::pause() {
    playState = SL_PLAYSTATE_PAUSED;
    result = (*playInterface)->SetPlayState(playInterface, SL_PLAYSTATE_PAUSED);

    return result;
}

//停止
SLresult OpenslHelper::stop() {
    playState = SL_PLAYSTATE_STOPPED;
    result = (*playInterface)->SetPlayState(playInterface, SL_PLAYSTATE_STOPPED);

    return result;
}

//析构
OpenslHelper::~OpenslHelper() {
    //播放器
    if (player != NULL) {
        (*player)->Destroy(player);
        player = NULL;
        //播放接口
        playInterface = NULL;
        //缓冲区
        bufferQueueItf = NULL;
        //音量
        volumeItf = NULL;
    }

    //混音器
    if (mix != NULL) {
        (*mix)->Destroy(mix);
        mix = NULL;
        //环境混响混音器接口
        environmentalReverbItf = NULL;
    }

    //opensl引擎
    if (engine != NULL) {
        (*engine)->Destroy(engine);
        engine = NULL;
        //引擎接口
        engineInterface = NULL;
    }

    result = NULL;
}

