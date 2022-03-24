#include "mainwindow.h"
#include <QtWidgets>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);

  // Qt sets the locale in the QApplication constructor, but libmpv requires
  // the LC_NUMERIC category to be set to "C", so change it back.
  std::setlocale(LC_NUMERIC, "C");

  MainWindow w;
  w.show();

  return a.exec();
}