#include "videoslider.h"
#include <QSlider>
#include <iostream>

VideoSlider::VideoSlider(QWidget *parent) : QSlider(parent)
{
    setOrientation(Qt::Horizontal);
    setFocusPolicy(Qt::NoFocus);
}

VideoSlider::~VideoSlider()
{
}
