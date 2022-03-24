#ifndef VIDEOSLIDER_H
#define VIDEOSLIDER_H

#include <QSlider>

class VideoSlider : public QSlider
{
    Q_OBJECT

public:
    explicit VideoSlider(QWidget *parent = 0);
    ~VideoSlider();

private slots:

signals:

private:
};

#endif // VIDEOSLIDER_H
