#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "videoslider.h"
#include <QMainWindow>
#include <mpv/client.h>

QT_FORWARD_DECLARE_CLASS(QTextEdit);
QT_FORWARD_DECLARE_CLASS(QGridLayout);
QT_FORWARD_DECLARE_CLASS(QPushButton);
QT_FORWARD_DECLARE_CLASS(QMenuBar);
QT_FORWARD_DECLARE_CLASS(QHBoxLayout);

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void on_slider_value_changed(int value);

private slots:
    void on_file_open();
    void on_mpv_events();

    void on_mpv_play_pause();
    void on_mpv_stop();

    void on_clip_start();
    void on_clip_end();

    void on_save_mp4();
    void on_save_webm();

    void on_close();

    void closeEvent(QCloseEvent *event);

signals:
    void mpv_events();

private:
    QByteArray c_input_filename;

    QWidget *main_widget;

    QGridLayout *grid_layout;

    QWidget *mpv_container;
    VideoSlider *video_slider;
    QHBoxLayout *mpv_controll;
    QHBoxLayout *save_controll;

    QPushButton *btn_mpv_play_pause;
    QPushButton *btn_mpv_stop;
    QPushButton *btn_fragment_start;
    QPushButton *btn_fragment_end;
    QPushButton *btn_save_mp4;
    QPushButton *btn_save_webm;

    mpv_handle *mpv;

    bool is_paused;
    bool is_stopped;

    double clip_start = 0;
    double clip_end = 0;

    double time_pos;
    double duration;

    void create_player();
    void handle_mpv_event(mpv_event *event);
    void keyReleaseEvent(QKeyEvent *event);

    void mpv_rewind();
    void mpv_forward();
    void mpv_toggle_pause();
    void mpv_next_frame();
    void mpv_prev_frame();
};

#endif // MAINWINDOW_H
