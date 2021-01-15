//
// Created by aruba on 2020/7/6.
//

#ifndef FFMPEGAPPLICATION_OPENSL_HELPER_H
#define FFMPEGAPPLICATION_OPENSL_HELPER_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

class OpenslHelper {
public:
    //返回结果
    SLresult result;
    //opensl引擎
    SLObjectItf engine;
    //引擎接口
    SLEngineItf engineInterface;
    //混音器
    SLObjectItf mix;
    //环境混响混音器接口
    SLEnvironmentalReverbItf environmentalReverbItf;
    //环境混响混音器环境
    const SLEnvironmentalReverbSettings settings = SL_I3DL2_ENVIRONMENT_PRESET_DEFAULT;
    //播放器
    SLObjectItf player;
    //播放接口
    SLPlayItf playInterface;
    //缓冲区
    SLAndroidSimpleBufferQueueItf bufferQueueItf;
    //音量
    SLVolumeItf volumeItf;
    //播放状态 SL_PLAYSTATE_XXX
    SLuint32 playState;

    /**
     * 创建opensl引擎接口
     * @return SLresult
     */
    SLresult createEngine();

    /**
     * 是否成功
     * @param result 
     * @return 
     */
    bool isSuccess(SLresult &result);

    /**
     * 创建混音器
     * @return SLresult
     */
    SLresult createMix();

    /**
     * 创建播放器
     * @param numChannels   声道数
     * @param samplesRate   采样率 SL_SAMPLINGRATE_XX
     * @param bitsPerSample 采样位数（量化格式） SL_PCMSAMPLEFORMAT_FIXED_XX
     * @param channelMask   立体声掩码 SL_SPEAKER_XX | SL_SPEAKER_XX
     * @return SLresult
     */
    SLresult createPlayer(int numChannels, long samplesRate, int bitsPerSample, int channelMask);

    //播放
    SLresult play();

    //暂停
    SLresult pause();

    //停止
    SLresult stop();

    /**
    * 播放器会不断调用此函数，我们需要在此回调中不断给缓冲区填充数据
    * @param bufferQueueItf 
    * @param pContext 
    */
    SLresult
    registerCallback(slAndroidSimpleBufferQueueCallback callback);

    ~OpenslHelper();
};


#endif //FFMPEGAPPLICATION_OPENSL_HELPER_H
