#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QUrl>
#include <QTime>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(&pcmAudio,&PCMAudio::debugMsg,this,&MainWindow::rcvDebug);
    connect(&pcmAudio,&PCMAudio::progress,this,&MainWindow::updateProgress);
    connect(&pcmAudio,&PCMAudio::finish,this,&MainWindow::resampleResult);
    connect(this,&MainWindow::startChange,&pcmAudio,&PCMAudio::startChange);
    connect(this,&MainWindow::stopChange,&pcmAudio,&PCMAudio::stopChange);
    pcmAudio.moveToThread(&thread);
    thread.start();
}

MainWindow::~MainWindow()
{
    if(thread.isRunning()){
        thread.exit();
        thread.wait(5 * 1000);
    }
    delete ui;
}

int MainWindow::getSrcSampleRate()
{
    if(ui->srcRate48000->isChecked())
        return 48000;
    else if(ui->srcRate44100->isChecked())
        return 44100;
    else if(ui->srcRate22050->isChecked())
        return 22050;
    else
        return 48000;
}

int MainWindow::getDstSampleRate()
{
    if(ui->dstRate48000->isChecked())
        return 48000;
    else if(ui->dstRate44100->isChecked())
        return 44100;
    else if(ui->dstRate22050->isChecked())
        return 22050;
    else
        return 48000;
}

AVSampleFormat MainWindow::getSrcFormat()
{
    if(ui->srcDouble->isChecked()){
        return AV_SAMPLE_FMT_DBL;
    }
    else if(ui->srcFloat->isChecked())
        return AV_SAMPLE_FMT_FLT;
    else if(ui->srcInt32->isChecked())
        return AV_SAMPLE_FMT_S32;
    else if(ui->srcInt16->isChecked())
        return AV_SAMPLE_FMT_S16;
    else if(ui->srcUint8->isChecked())
        return AV_SAMPLE_FMT_U8;
    else
        return AV_SAMPLE_FMT_S16;
}

AVSampleFormat MainWindow::getDstFormat()
{
    if(ui->dstDouble->isChecked()){
        return AV_SAMPLE_FMT_DBL;
    }
    else if(ui->dstFloat->isChecked())
        return AV_SAMPLE_FMT_FLT;
    else if(ui->dstInt32->isChecked())
        return AV_SAMPLE_FMT_S32;
    else if(ui->dstInt16->isChecked())
        return AV_SAMPLE_FMT_S16;
    else if(ui->dstUint8->isChecked())
        return AV_SAMPLE_FMT_U8;
    else
        return AV_SAMPLE_FMT_S16;
}

int MainWindow::getSrcChannels()
{
    if(ui->srcChannels1->isChecked())
        return 1;
    else if(ui->srcChannels2->isChecked())
        return 2;
    else
        return 2;
}

int MainWindow::getDstChannels()
{
    if(ui->dstChannels1->isChecked())
        return 1;
    else if(ui->dstChannels2->isChecked())
        return 2;
    else
        return 2;
}

PCMAudio::FileType MainWindow::getDstType()
{
    if(ui->pcmTypeButton->isChecked())
        return PCMAudio::PCM;
    else if(ui->wavTypeButton->isChecked())
        return PCMAudio::WAV;
    else
        return PCMAudio::OTHER;
}

void MainWindow::on_pathSelectButton_clicked(bool)
{
    QUrl url = QFileDialog::getOpenFileUrl(this,"select audio file");
    ui->srcPathLineEdit->setText(url.toString(QUrl::PreferLocalFile));
    pcmAudio.setFilePath(url);

    ui->playButton->setEnabled(true);
    ui->testButton->setEnabled(false);
    ui->playButton->setText("play");
    auto type = pcmAudio.getType();
    switch(type){
    case PCMAudio::Error:
        ui->playButton->setEnabled(false);
        break;
    case PCMAudio::WAV:
        ui->inputGroup->setEnabled(false);
        break;
    case PCMAudio::PCM:
        ui->inputGroup->setEnabled(true);
        break;
    case PCMAudio::OTHER:
        break;
    }
}

void MainWindow::on_playButton_clicked(bool)
{
    if(ui->playButton->text() == "play"){
        ui->playButton->setText("stop");
        ui->startButton->setEnabled(false);
        ui->pathSelectButton->setEnabled(false);
        pcmAudio.playMusic(true,getSrcSampleRate(),getSrcFormat(),getSrcChannels());
    }
    else{
        ui->playButton->setText("play");
        ui->startButton->setEnabled(true);
        ui->pathSelectButton->setEnabled(true);
        pcmAudio.stopMusic();
    }
}

void MainWindow::on_testButton_clicked(bool)
{
    if(ui->testButton->text() == "test"){
        ui->testButton->setText("stop");
        ui->startButton->setEnabled(false);
        ui->playButton->setEnabled(false);
        ui->pathSelectButton->setEnabled(false);
        pcmAudio.playMusic(false,getDstSampleRate(),getDstFormat(),getDstChannels());
    }
    else{
        ui->testButton->setText("test");
        ui->startButton->setEnabled(true);
        ui->playButton->setEnabled(true);
        ui->pathSelectButton->setEnabled(true);
        pcmAudio.stopMusic();
    }
}

void MainWindow::on_startButton_clicked(bool)
{
    if(ui->startButton->text() == "start"){
        ui->startButton->setText("stop");

        pcmAudio.setSrcSampleFormat(getSrcFormat());
        pcmAudio.setSrcRate(getSrcSampleRate());
        auto layout = getSrcChannels() == 2?AV_CH_LAYOUT_STEREO:AV_CH_LAYOUT_MONO;
        pcmAudio.setSrcLayout(layout);

        pcmAudio.setDstSampleFormat(getDstFormat());
        pcmAudio.setDstRate(getDstSampleRate());
        layout = getDstChannels() == 2?AV_CH_LAYOUT_STEREO:AV_CH_LAYOUT_MONO;
        pcmAudio.setDstLayout(layout);

        pcmAudio.setDstType(getDstType());
        emit startChange();
    }
    else{
        ui->startButton->setText("start");
        emit stopChange();
    }

}

void MainWindow::rcvDebug(const QString &msg)
{
    QString str("[%1]%2");
    ui->debugText->append(str.arg(QTime::currentTime().toString()).arg(msg));
}

void MainWindow::updateProgress(int finish, int total)
{
    if(ui->progressBar->maximum() != total)
        ui->progressBar->setMaximum(total);
    ui->progressBar->setValue(finish);
}

void MainWindow::resampleResult(bool result)
{
    ui->startButton->setText("start");
    if(result){
        rcvDebug("转换成功，可以点击ｔｅｓｔ按钮来试听");
        ui->testButton->setEnabled(true);
    }
    else
        rcvDebug("转换失败");
}
