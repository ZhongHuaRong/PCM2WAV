#include "pcmaudio.h"
#include <QAudioFormat>
#include <QFile>
#include <QDebug>
#include <QDateTime>

PCMAudio::PCMAudio(QObject *parent) :
    QObject(parent),
    srcType(OTHER),
    dstType(OTHER),
    output(nullptr)
{
    srcBuffer.setBuffer(&srcData);
    srcBuffer.open(QIODevice::ReadOnly);
    srcBuffer.seek(0);
    dstBuffer.setBuffer(&dstData);
    dstBuffer.open(QIODevice::ReadOnly);
    dstBuffer.seek(0);
}

void PCMAudio::setFilePath(const QUrl &url)
{
    if(url.isEmpty()){
        emit debugMsg("path is empty");
        return;
    }

    QString msg("path:%1\nfileName:%2");
    srcUrl = url;
    msg = msg.arg(srcUrl.toString(QUrl::PreferLocalFile)).arg(srcUrl.fileName());
    emit debugMsg(msg);
    _setType();
}

void PCMAudio::playMusic(bool isSrc,int rate,AVSampleFormat format,int channels)
{
    auto f = makePlayFormat(rate,format,channels);
    stopMusic();
    output = new QAudioOutput(f);
    _setData();
    if(isSrc){
        srcBuffer.seek(0);
        output->start(&srcBuffer);
    }
    else{
        dstBuffer.seek(0);
        output->start(&dstBuffer);
    }
}

void PCMAudio::stopMusic()
{
    if(output != nullptr){
        output->stop();
        delete output;
        output = nullptr;
    }
}

QAudioFormat PCMAudio::makePlayFormat(int rate, AVSampleFormat format, int channels)
{
    QAudioFormat f;
    f.setChannelCount(channels);
    f.setSampleRate(rate);
    switch(format){
    case AV_SAMPLE_FMT_U8:
        f.setSampleType(QAudioFormat::UnSignedInt);
        f.setSampleSize(8);
        break;
    case AV_SAMPLE_FMT_S16:
        f.setSampleType(QAudioFormat::SignedInt);
        f.setSampleSize(16);
        break;
    case AV_SAMPLE_FMT_S32:
        f.setSampleType(QAudioFormat::SignedInt);
        f.setSampleSize(32);
        break;
    case AV_SAMPLE_FMT_DBL:
        emit debugMsg("现在无法播放double类型音频，默认转为float");
        f.setSampleSize(64);
        f.setSampleType(QAudioFormat::Float);
        break;
    case AV_SAMPLE_FMT_FLT:
        f.setSampleType(QAudioFormat::Float);
        f.setSampleSize(32);
        break;
    default:
        f.setSampleType(QAudioFormat::SignedInt);
        f.setSampleSize(16);
    }
    f.setCodec("audio/pcm");
    f.setByteOrder(QAudioFormat::LittleEndian);
    return f;
}

void PCMAudio::startChange()
{
    changeFlag = true;
    _setData();
    dstData.clear();
    bool f;
    if(srcLayout != dstLayout || srcSampleFormat != dstSampleFormat || srcSampleRate != dstSampleRate)
        f = _resample();
    else{
        dstData = srcData;
        f = true;
    }
    /*无论成功或者失败，都将发送该信号*/
    emit finish(f);
    if(f){
        emit debugMsg("Ready to write to file");
        _saveFile();
    }
}

void PCMAudio::stopChange()
{
    changeFlag = false;
}

bool PCMAudio::_resample()
{
    uint8_t **src_data = nullptr, **dst_data = nullptr;
    int src_linesize, dst_linesize;
    int src_nb_samples = 1024, dst_nb_samples, max_dst_nb_samples;
    int ret;
    auto swr_ctx = swr_alloc();
    if(!swr_ctx){
        fprintf(stderr, "Could not allocate resampler context\n");
        return false;
    }

    av_opt_set_int(swr_ctx, "in_channel_layout",    srcLayout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate",       srcSampleRate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", srcSampleFormat, 0);

    av_opt_set_int(swr_ctx, "out_channel_layout",    dstLayout, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate",       dstSampleRate, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dstSampleFormat, 0);

    if((ret = swr_init(swr_ctx)) < 0){
        fprintf(stderr, "Failed to initialize the resampling context\n");
        freep(&swr_ctx,&src_data,&dst_data);
        return false;
    }

    auto src_nb_channels = av_get_channel_layout_nb_channels(srcLayout);
    ret = av_samples_alloc_array_and_samples(&src_data, &src_linesize, src_nb_channels,
                                             src_nb_samples, srcSampleFormat, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate source samples\n");
        freep(&swr_ctx,&src_data,&dst_data);
        return false;
    }

    max_dst_nb_samples = dst_nb_samples =
        av_rescale_rnd(src_nb_samples, dstSampleRate,
                       srcSampleRate, AV_ROUND_UP);
    auto dst_nb_channels = av_get_channel_layout_nb_channels(dstLayout);
    ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,
                                                 dst_nb_samples, dstSampleFormat, 0);

    if (ret < 0) {
         fprintf(stderr, "Could not allocate destination samples\n");
         freep(&swr_ctx,&src_data,&dst_data);
         return false;
     }

    qint64 t = 0;
    int dst_bufsize;
    srcBuffer.seek(0);
    emit progress(0,srcBuffer.size());
    do{
        t = srcBuffer.read((char *)src_data[0],src_nb_samples * 8);

        /* compute destination number of samples */
        dst_nb_samples =
                av_rescale_rnd(swr_get_delay(swr_ctx, srcSampleRate) + src_nb_samples,
                               dstSampleRate,
                               srcSampleRate,
                               AV_ROUND_UP);
        if (dst_nb_samples > max_dst_nb_samples) {
            av_freep(&dst_data[0]);
            ret = av_samples_alloc(dst_data, &dst_linesize, dst_nb_channels,
                                   dst_nb_samples, dstSampleFormat, 1);
            if (ret < 0)
                break;
            max_dst_nb_samples = dst_nb_samples;
        }

        /* convert to destination format */
        ret = swr_convert(swr_ctx, dst_data, dst_nb_samples, (const uint8_t **)src_data, src_nb_samples);
        if (ret < 0) {
            fprintf(stderr, "Error while converting\n");
            freep(&swr_ctx,&src_data,&dst_data);
            return false;
        }
        dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,
                                                 ret, dstSampleFormat, 1);
        if (dst_bufsize < 0) {
            fprintf(stderr, "Could not get sample buffer size\n");
            freep(&swr_ctx,&src_data,&dst_data);
            return false;
        }
        //printf("t:%d in:%ld out:%d\n", dst_nb_samples, t, dst_bufsize);
        dstData.append((char *)dst_data[0],dst_bufsize);
        emit progress(srcBuffer.pos(),srcBuffer.size());
    }while(!srcBuffer.atEnd() && changeFlag);

    freep(&swr_ctx,&src_data,&dst_data);
    return true;
}

void PCMAudio::freep(SwrContext **ctx, uint8_t ***srcData, uint8_t ***dstData)
{
    if(*srcData != nullptr){
        av_freep(&(*srcData)[0]);
    }
    av_freep(srcData);

    if(*dstData != nullptr){
        av_freep(&(*dstData)[0]);
    }
    av_freep(dstData);

    swr_free(ctx);
}

void PCMAudio::_setType()
{
    QFile file(srcUrl.toString(QUrl::PreferLocalFile));
    if(!file.open(QFile::ReadOnly)){
        emit debugMsg("file open error");
        srcType = Error;
        return;
    }

    /*只完成两种格式*/
    QByteArray ba = file.read(16);
    if(ba.left(4) == "RIFF" && ba.mid(8,4) == "WAVE"){
        srcType = WAV;
        return;
    }
    else{
        srcType = PCM;
        return;
    }

}

void PCMAudio::_setData()
{
    srcData.clear();
    srcBuffer.seek(0);
    QFile file(srcUrl.toString(QUrl::PreferLocalFile));
    file.open(QFile::ReadOnly);
    srcData = file.readAll();
    switch(srcType){
    case WAV:
        break;
    case PCM:
        break;
    default:
        break;
    }
}

void PCMAudio::_saveFile()
{
    QFile file;
    auto list = srcUrl.fileName().split(".");
    switch(dstType){
    case WAV:
    {
        file.setFileName(list.first() + QDateTime::currentDateTime().toString("_yyyy_MM_dd_hh-mm-ss") + ".wav");
        file.open(QFile::WriteOnly);
        _writeHead(file,dstData.size());
        auto size = file.write(dstData);
        if(size == dstData.size()){
            emit debugMsg("write file success");
            file.flush();
            file.close();
        }
        else{
            emit debugMsg("write file error");
            file.close();
            file.remove();
        }
        break;
    }
    case PCM:
    {
        file.setFileName(list.first() + QDateTime::currentDateTime().toString("_yyyy_MM_dd_hh-mm-ss") + ".pcm");
        file.open(QFile::WriteOnly);
        auto size = file.write(dstData);
        if(size == dstData.size()){
            emit debugMsg("write file success");
            file.flush();
            file.close();
        }
        else{
            emit debugMsg("write file error");
            file.close();
            file.remove();
        }
        break;
    }
    default:
        break;
    }
}

void PCMAudio::_writeHead(QFile &file,uint32_t dataSize)
{
    switch(dstType){
    case WAV:
    {
        uint8_t wav_header[] = {
            'R', 'I', 'F', 'F',                                                   /*"RIFF"标志*/
            0, 0, 0, 0,                                                           /*文件长度*/
            'W', 'A', 'V', 'E',                                                   /*"WAVE"标志*/
            'f', 'm', 't', ' ',                                                   /*"fmt"标志*/
            16, 0, 0, 0,                                                          /*过渡字节（不定）*/
            1, 0,                                                                 /*格式类别*/
            0, 0,                                                                 /*声道数*/
            0, 0, 0, 0,                                                           /*采样*/
            0, 0, 0, 0,                                                           /*位速*/
            0, 0,                                                                 /*一个采样多声道数据块大小*/
            16, 0,                                                                /*一个采样占的bit数*/
            'd', 'a', 't', 'a',                                                   /*数据标记符＂data＂*/
            0,0, 0, 0                                                             /*语音数据的长度，比文件长度小36*/
        };
        uint32_t fileSize = dataSize + sizeof(wav_header);
        memcpy(wav_header + 0x04, &fileSize,4);
        uint16_t channels = 0;
        if(dstLayout ==AV_CH_LAYOUT_STEREO)
            channels = 2;
        else
            channels = 1;
        memcpy(wav_header + 0x16, &channels,2);
        memcpy(wav_header + 0x18, &dstSampleRate,4);
        uint16_t bits;
        switch(dstSampleFormat){
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_U8P:
            bits = 8;
            break;
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S16P:
            bits = 16;
            break;
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_S32P:
        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            bits = 32;
            break;
        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP:
        case AV_SAMPLE_FMT_S64:
        case AV_SAMPLE_FMT_S64P:
            bits = 64;
            break;
        default:
            bits = 0;
        }
        uint16_t block = channels * bits / 8;
        uint32_t speed = dstSampleRate * block;
        memcpy(wav_header + 0x1C, &speed , 4);
        memcpy(wav_header + 0x20, &block , 2);
        memcpy(wav_header + 0x22, &bits, 2);
        memcpy(wav_header + 0x28, &dataSize, 4);
        file.write((char *)wav_header,sizeof(wav_header));
        break;
    }
    case PCM:
    default:
        break;
    }
}
