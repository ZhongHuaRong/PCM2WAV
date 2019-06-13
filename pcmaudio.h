#ifndef PCMAUDIO_H
#define PCMAUDIO_H

#include <QObject>
#include <QThread>
#include <QUrl>
#include <QAudioOutput>
#include <QBuffer>
#include <QFile>
extern "C"{
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
}

class PCMAudio : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief The FileType enum
     * 暂时设置两种
     */
    enum FileType{
        Error = 0,
        PCM = 0x01,
        WAV,
        OTHER
    };

public:
    explicit PCMAudio(QObject *parent = nullptr);

    void setSrcSampleFormat(const AVSampleFormat &format);
    void setSrcLayout(const int64_t &layout);
    void setSrcRate(const int &rate);
    void setDstSampleFormat(const AVSampleFormat &format);
    void setDstLayout(const int64_t &layout);
    void setDstRate(const int &rate);
    void setDstType(const FileType &type);
    PCMAudio::FileType getType();

    void setFilePath(const QUrl &url);
    void playMusic(bool isSrc,int rate,AVSampleFormat format,int channels);
    const QByteArray & getFilePCMData();
    void stopMusic();

    QAudioFormat makePlayFormat(int rate,AVSampleFormat format,int channels);
signals:
    void debugMsg(const QString &msg);
    void progress(int finish,int total);
    void finish(bool result);
public slots:
    void startChange();
    void stopChange();
private:
    bool _resample();
    void freep(SwrContext **ctx,uint8_t ***srcData,uint8_t ***dstData);
    void _setType();
    void _setData();
    void _saveFile();
    void _writeHead(QFile &file,uint32_t dataSize);
private:
    int64_t srcLayout;
    int64_t dstLayout;
    AVSampleFormat srcSampleFormat;
    AVSampleFormat dstSampleFormat;
    int srcSampleRate;
    int dstSampleRate;
    QUrl srcUrl;
    PCMAudio::FileType srcType;
    PCMAudio::FileType dstType;
    QAudioOutput *output;
    QByteArray srcData;
    QBuffer srcBuffer;
    QByteArray dstData;
    QBuffer dstBuffer;

    volatile bool changeFlag;
};

inline void PCMAudio::setSrcSampleFormat(const AVSampleFormat &format)          {   srcSampleFormat = format;}
inline void PCMAudio::setSrcLayout(const int64_t &layout)                       {   srcLayout = layout;}
inline void PCMAudio::setSrcRate(const int &rate)                               {   srcSampleRate = rate;}
inline void PCMAudio::setDstSampleFormat(const AVSampleFormat &format)          {   dstSampleFormat = format;}
inline void PCMAudio::setDstLayout(const int64_t &layout)                       {   dstLayout = layout;}
inline void PCMAudio::setDstRate(const int &rate)                               {   dstSampleRate = rate;}
inline void PCMAudio::setDstType(const FileType &type)                          {   dstType = type;}
inline PCMAudio::FileType PCMAudio::getType()                                   {   return srcType;}
#endif // PCMAUDIO_H
