#include <sstream>

#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QGridLayout>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QStatusBar>

#include "qthelper.hpp"

#include "mainwindow.h"

// ffmpeg
// extern "C"
// {
// #include <libavcodec/avcodec.h>
// #include <libavformat/avformat.h>
// #include <libavutil/imgutils.h>
// #include <libavutil/samplefmt.h>
// #include <libavutil/timestamp.h>
// }

static void wakeup(void *ctx)
{
    // This callback is invoked from any mpv thread (but possibly also
    // recursively from a thread that is calling the mpv API). Just notify
    // the Qt GUI thread to wake up (so that it can process events with
    // mpv_wait_event()), and return as quickly as possible.
    MainWindow *mainwindow = (MainWindow *)ctx;
    emit mainwindow->mpv_events();
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(tr("Video Cutter"));
    setMinimumSize(640, 480);

    // menu
    QMenu *menu = menuBar()->addMenu(tr("&File"));

    QAction *menu_open = new QAction(tr("&Open"), this);
    menu_open->setShortcuts(QKeySequence::Open);
    menu_open->setStatusTip(tr("Open a video file"));
    connect(menu_open, &QAction::triggered, this, &MainWindow::on_file_open);
    menu->addAction(menu_open);

    QAction *menu_exit = new QAction(tr("&Exit"), this);
    menu_exit->setShortcuts(QKeySequence::Quit);
    menu_exit->setStatusTip(tr("Exit the application"));
    connect(menu_exit, &QAction::triggered, this, &QApplication::quit);
    menu->addAction(menu_exit);

    // status bar
    statusBar();

    // init
    mpv = mpv_create();
    if (!mpv)
        throw std::runtime_error("can't create mpv instance");

    // layout
    mpv_container = new QWidget();
    mpv_container->setAttribute(Qt::WA_DontCreateNativeAncestors);
    mpv_container->setAttribute(Qt::WA_NativeWindow);

    int64_t wid = mpv_container->winId();
    mpv_set_option(mpv, "wid", MPV_FORMAT_INT64, &wid);

    mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "percent-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "track-list", MPV_FORMAT_NODE);
    mpv_observe_property(mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "filename", MPV_FORMAT_STRING);

    // From this point on, the wakeup function will be called. The callback
    // can come from any thread, so we use the QueuedConnection mechanism to
    // relay the wakeup in a thread-safe way.
    connect(this, &MainWindow::mpv_events, this, &MainWindow::on_mpv_events,
            Qt::QueuedConnection);
    mpv_set_wakeup_callback(mpv, wakeup, this);

    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("mpv failed to initialize");

    // mpv controll
    btn_mpv_play_pause = new QPushButton("Play");
    btn_mpv_play_pause->setFocusPolicy(Qt::NoFocus);
    connect(btn_mpv_play_pause, &QPushButton::clicked, this, &MainWindow::on_mpv_play_pause);
    btn_mpv_stop = new QPushButton("Stop");
    btn_mpv_stop->setFocusPolicy(Qt::NoFocus);
    connect(btn_mpv_stop, &QPushButton::clicked, this, &MainWindow::on_mpv_stop);
    btn_fragment_start = new QPushButton("Start");
    btn_fragment_start->setFocusPolicy(Qt::NoFocus);
    connect(btn_fragment_start, &QPushButton::clicked, this, &MainWindow::on_clip_start);
    btn_fragment_end = new QPushButton("End");
    btn_fragment_end->setFocusPolicy(Qt::NoFocus);
    connect(btn_fragment_end, &QPushButton::clicked, this, &MainWindow::on_clip_end);

    mpv_controll = new QHBoxLayout();
    mpv_controll->setAlignment(Qt::AlignCenter);
    mpv_controll->addWidget(btn_mpv_play_pause);
    mpv_controll->addWidget(btn_mpv_stop);
    mpv_controll->addWidget(btn_fragment_start);
    mpv_controll->addWidget(btn_fragment_end);

    // save controll
    btn_save_mp4 = new QPushButton("MP4");
    btn_save_mp4->setFocusPolicy(Qt::NoFocus);
    connect(btn_save_mp4, &QPushButton::clicked, this, &MainWindow::on_save_mp4);
    btn_save_webm = new QPushButton("WebM");
    btn_save_webm->setFocusPolicy(Qt::NoFocus);
    connect(btn_save_webm, &QPushButton::clicked, this, &MainWindow::on_save_webm);

    save_controll = new QHBoxLayout();
    save_controll->setAlignment(Qt::AlignCenter);
    save_controll->addWidget(btn_save_mp4);
    save_controll->addWidget(btn_save_webm);

    // video slider
    video_slider = new VideoSlider(this);
    connect(video_slider, &VideoSlider::valueChanged, this, &MainWindow::on_slider_value_changed);

    // main layout
    grid_layout = new QGridLayout();
    grid_layout->addWidget(mpv_container, 0, 0);
    grid_layout->addWidget(video_slider, 1, 0);
    grid_layout->addLayout(mpv_controll, 2, 0);
    grid_layout->addLayout(save_controll, 3, 0);

    main_widget = new QWidget(this);
    main_widget->setLayout(grid_layout);

    setCentralWidget(main_widget);
}

void MainWindow::handle_mpv_event(mpv_event *event)
{
    switch (event->event_id)
    {
    case MPV_EVENT_PROPERTY_CHANGE:
    {
        mpv_event_property *prop = (mpv_event_property *)event->data;
        if (strcmp(prop->name, "time-pos") == 0)
        {
            if (prop->format == MPV_FORMAT_DOUBLE)
            {
                time_pos = *(double *)prop->data;
                std::stringstream ss;
                ss << "START: " << clip_start << " END: " << clip_end << " LENGTH: " << clip_end - clip_start << " POS: " << time_pos;
                statusBar()->showMessage(QString::fromStdString(ss.str()));
            }
            else if (prop->format == MPV_FORMAT_NONE)
            {
                statusBar()->showMessage("");
            }
        }
        else if (strcmp(prop->name, "pause") == 0)
        {
            if (prop->format == MPV_FORMAT_FLAG)
            {
                bool paused = *(int *)prop->data;
                if (paused)
                {
                    is_paused = true;
                    btn_mpv_play_pause->setText("Play");
                }
                else
                {
                    is_paused = false;
                    btn_mpv_play_pause->setText("Pause");
                }
            }
        }
        else if (strcmp(prop->name, "track-list") == 0)
        {
            int64_t count;
            mpv_get_property(mpv, "track-list/count", MPV_FORMAT_INT64, &count);
            if (count == 0)
            {
                video_slider->setEnabled(false);
                btn_mpv_play_pause->setEnabled(false);
                btn_mpv_stop->setEnabled(false);
                btn_fragment_start->setEnabled(false);
                btn_fragment_end->setEnabled(false);
                btn_save_mp4->setEnabled(false);
                btn_save_webm->setEnabled(false);
                setWindowTitle(tr("Video Cutter"));
            }
            else
            {
                video_slider->setEnabled(true);
                btn_mpv_play_pause->setEnabled(true);
                btn_mpv_stop->setEnabled(true);
                btn_fragment_start->setEnabled(true);
                btn_fragment_end->setEnabled(true);
                btn_save_mp4->setEnabled(true);
                btn_save_webm->setEnabled(true);
            }
        }
        else if (strcmp(prop->name, "percent-pos") == 0)
        {
            if (prop->format == MPV_FORMAT_DOUBLE)
            {
                double percent = *(double *)prop->data;
                video_slider->setValue(percent);
            }
        }
        else if (strcmp(prop->name, "duration") == 0)
        {
            if (prop->format == MPV_FORMAT_DOUBLE)
            {
                duration = *(double *)prop->data;
            }
        }
        else if (strcmp(prop->name, "filename") == 0)
        {
            if (prop->format == MPV_FORMAT_STRING)
            {
                setWindowTitle(QString::fromUtf8(*(char **)prop->data));
            }
        }
        break;
    }
    case MPV_EVENT_VIDEO_RECONFIG:
    {
        // Retrieve the new video size.
        int64_t w, h;
        if (mpv_get_property(mpv, "dwidth", MPV_FORMAT_INT64, &w) >= 0 &&
            mpv_get_property(mpv, "dheight", MPV_FORMAT_INT64, &h) >= 0 &&
            w > 0 && h > 0)
        {
            // Note that the MPV_EVENT_VIDEO_RECONFIG event doesn't necessarily
            // imply a resize, and you should check yourself if the video
            // dimensions really changed.
            // mpv itself will scale/letter box the video to the container size
            // if the video doesn't fit.
            std::stringstream ss;
            ss << "Reconfig: " << w << " " << h;
            statusBar()->showMessage(QString::fromStdString(ss.str()));
        }
        break;
    }
    case MPV_EVENT_SHUTDOWN:
    {
        mpv_terminate_destroy(mpv);
        mpv = NULL;
        break;
    }
    default:;
    }
}

void MainWindow::on_mpv_events()
{
    while (mpv)
    {
        mpv_event *event = mpv_wait_event(mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;
        handle_mpv_event(event);
    }
}

void MainWindow::on_file_open()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Open a video file"));
    if (mpv)
    {
        c_input_filename = filename.toUtf8();
        const char *args[] = {"loadfile", c_input_filename.data(), NULL};
        mpv_command_async(mpv, 0, args);
    }
}

void MainWindow::on_slider_value_changed(int value)
{
    if (mpv)
    {
        double new_time_pos = value / 100.0 * duration;
        mpv_set_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &new_time_pos);
    }
}

void MainWindow::on_mpv_play_pause()
{
    mpv_toggle_pause();
}

void MainWindow::on_mpv_stop()
{
    if (mpv)
    {
        const char *args[] = {"stop", NULL};
        mpv_command_async(mpv, 0, args);
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    switch (event->key())
    {
    case Qt::Key_Left:
        mpv_rewind();
        break;
    case Qt::Key_Right:
        mpv_forward();
        break;
    case Qt::Key_Space:
        mpv_toggle_pause();
        break;
    case Qt::Key_Comma:
        mpv_prev_frame();
        break;
    case Qt::Key_Period:
        mpv_next_frame();
        break;
    default:;
    }
}

void MainWindow::mpv_rewind()
{
    if (mpv)
    {
        const char *args[] = {"seek", "-5", "relative", NULL};
        mpv_command_async(mpv, 0, args);
    }
}

void MainWindow::mpv_forward()
{
    if (mpv)
    {
        const char *args[] = {"seek", "+5", "relative", NULL};
        mpv_command_async(mpv, 0, args);
    }
}

void MainWindow::mpv_toggle_pause()
{
    if (mpv)
    {
        const char *args[] = {"cycle", "pause", NULL};
        mpv_command_async(mpv, 0, args);
    }
}

void MainWindow::mpv_prev_frame()
{
    if (mpv)
    {
        const char *args[] = {"frame-back-step", NULL};
        mpv_command_async(mpv, 0, args);
    }
}

void MainWindow::mpv_next_frame()
{
    if (mpv)
    {
        const char *args[] = {"frame-step", NULL};
        mpv_command_async(mpv, 0, args);
    }
}

void MainWindow::on_clip_start()
{
    clip_start = time_pos;
    std::stringstream ss;
    ss << "START: " << clip_start << " END: " << clip_end << " LENGTH: " << clip_end - clip_start << " POS: " << time_pos;
    statusBar()->showMessage(QString::fromStdString(ss.str()));
}

void MainWindow::on_clip_end()
{
    clip_end = time_pos;
    std::stringstream ss;
    ss << "START: " << clip_start << " END: " << clip_end << " LENGTH: " << clip_end - clip_start << " POS: " << time_pos;
    statusBar()->showMessage(QString::fromStdString(ss.str()));
}

void MainWindow::on_save_mp4()
{
    if (mpv == NULL)
    {
        statusBar()->showMessage("No mpv instance");
        return;
    }
    if (clip_start == clip_end || (clip_start > duration || clip_end > duration) || clip_start > clip_end)
    {
        statusBar()->showMessage("Invalid fragment");
        return;
    }
    QString filename = QFileDialog::getSaveFileName(this, "Save file", NULL, "MP4 files (*.mp4)");
    if (filename.isEmpty())
    {
        statusBar()->showMessage("No filename");
        return;
    }
    // TODO use C API
    std::stringstream ss;
    ss << "ffmpeg -y -ss " << clip_start << " -i \"" << c_input_filename.data()
       << "\" -t " << clip_end - clip_start
       << " -c:v libx264 -an -sn -map_chapters -1 -map_metadata -1 -crf 22 -pix_fmt yuv420p "
       << "-vf \"scale=iw*min(1\\,min(1280/iw\\,720/ih)):-2\" -preset slow \""
       << filename.toUtf8().data()
       << "\" 2>&1";
    btn_save_mp4->setEnabled(false);
    if (system(ss.str().c_str()) == 0)
    {
        statusBar()->showMessage("Saved");
    }
    else
    {
        statusBar()->showMessage("Failed");
    }
    btn_save_mp4->setEnabled(true);
}

void MainWindow::on_save_webm()
{
    if (mpv == NULL)
    {
        statusBar()->showMessage("No mpv instance");
        return;
    }
    if (clip_start == clip_end || (clip_start > duration || clip_end > duration) || clip_start > clip_end)
    {
        statusBar()->showMessage("Invalid fragment");
        return;
    }
    QString filename = QFileDialog::getSaveFileName(this, "Save file", NULL, "WebM files (*.webm)");
    if (filename.isEmpty())
    {
        statusBar()->showMessage("No filename");
        return;
    }
    // TODO use C API
    std::stringstream ss;
    ss << "ffmpeg -y -ss " << clip_start << " -i \"" << c_input_filename.data()
       << "\" -t " << clip_end - clip_start
       << " -c:v libvpx-vp9 -an -sn -map_chapters -1 -map_metadata -1 -crf 22 -pix_fmt yuva420p "
       << "-vf \"scale=iw*min(1\\,min(512/iw\\,512/ih)):-2\" -preset slow \""
       << filename.toUtf8().data()
       << "\" 2>&1";
    btn_save_webm->setEnabled(false);
    if (system(ss.str().c_str()) == 0)
    {
        statusBar()->showMessage("Saved");
    }
    else
    {
        statusBar()->showMessage("Failed");
    }
    btn_save_webm->setEnabled(true);
}

void MainWindow::on_close()
{
    if (mpv)
    {
        mpv_terminate_destroy(mpv);
        mpv = NULL;
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    on_close();
    event->accept();
}

MainWindow::~MainWindow()
{
    on_close();

    delete btn_save_webm;
    delete btn_save_mp4;
    delete save_controll;

    delete btn_mpv_play_pause;
    delete btn_mpv_stop;
    delete mpv_controll;

    delete video_slider;

    delete mpv_container;

    delete grid_layout;

    delete main_widget;
}
