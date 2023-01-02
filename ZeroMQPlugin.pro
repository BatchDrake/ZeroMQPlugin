QT += core widgets gui

TEMPLATE = lib
DEFINES += ZEROMQPLUGIN_LIBRARY

CONFIG += c++11

isEmpty(SUWIDGETS_PREFIX) {
  SUWIDGETS_INSTALL_HEADERS=$$[QT_INSTALL_HEADERS]/SuWidgets
} else {
  SUWIDGETS_INSTALL_HEADERS=$$SUWIDGETS_PREFIX/include/SuWidgets
}

isEmpty(SIGDIGGER_PREFIX) {
  SIGDIGGER_INSTALL_HEADERS=$$[QT_INSTALL_HEADERS]/SigDigger
} else {
  SIGDIGGER_INSTALL_HEADERS=$$SIGDIGGER_PREFIX/include
}

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    AddChanDialog.cpp \
    AddMasterDialog.cpp \
    MultiChannelTreeModel.cpp \
    Registration.cpp \
    ZeroMQWidget.cpp \
    ZeroMQWidgetFactory.cpp \
    MultiChannelForwarder.cpp

HEADERS += \
  AddChanDialog.h \
  AddMasterDialog.h \
  MultiChannelTreeModel.h \
  ZeroMQWidget.h \
    ZeroMQWidgetFactory.h \
    MultiChannelForwarder.h

INCLUDEPATH += $$SUWIDGETS_INSTALL_HEADERS $$SIGDIGGER_INSTALL_HEADERS

unix: CONFIG += link_pkgconfig
unix: PKGCONFIG += suscan sigutils fftw3 alsa sndfile

CONFIG += c++11


# Default rules for deployment.
unix {
    target.path = /usr/lib
}
!isEmpty(target.path): INSTALLS += target

FORMS += \
  AddChanDialog.ui \
  AddMasterDialog.ui \
  ZeroMQWidget.ui
