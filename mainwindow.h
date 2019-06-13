#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFile>
#include "pcmaudio.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    int getSrcSampleRate();
    int getDstSampleRate();
    AVSampleFormat getSrcFormat();
    AVSampleFormat getDstFormat();
    int getSrcChannels();
    int getDstChannels();

    PCMAudio::FileType getDstType();
signals:
    void startChange();
    void stopChange();
public slots:
    void on_pathSelectButton_clicked(bool);
    void on_playButton_clicked(bool);
    void on_testButton_clicked(bool);
    void on_startButton_clicked(bool);

    void rcvDebug(const QString &msg);
    void updateProgress(int finish,int total);
    void resampleResult(bool result);
private:
    Ui::MainWindow *ui;

    QThread thread;
    PCMAudio pcmAudio;
};

#endif // MAINWINDOW_H
