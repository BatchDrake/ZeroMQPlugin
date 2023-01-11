#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <Suscan/Library.h>

class MultiChannelForwarder;
class ZeroMQSink;

class SettingsManager : public QObject
{
  Q_OBJECT


  QString m_zmqAddress = "";
  bool m_correctDc = false;
  bool m_aborted = false;
  SUFREQ m_tunerFreq = 0;
  SUFREQ m_lnbFreq = 0;

  template<typename ... arg> void error(const char *fmt, arg ...);

public:
  SettingsManager(QObject *);
  bool loadSettings(const char *path);
  bool saveSettings(const char *path, const MultiChannelForwarder *);

  void abortLoad();

  QString getZmqAddres() const;
  bool getCorrectDC() const;
  SUFREQ getTunerFreq() const;
  SUFREQ getLNBFreq() const;

  void setZmqAddress(QString);
  void setCorrectDC(bool);
  void setTunerFreq(SUFREQ);
  void setLNBFreq(SUFREQ);

signals:
  void loadError(QString);

  void createMaster(QString, SUFREQ, SUFLOAT, bool);
  void createVFO(QString, SUFREQ, SUFLOAT, QString, qint64, bool);
};

#endif // SETTINGSMANAGER_H
