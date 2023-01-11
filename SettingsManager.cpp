#include "SettingsManager.h"
#include "MultiChannelForwarder.h"
#include <ZeroMQSink.h>

#include <QSettings>

#define EXTRA_BW_FACTOR 1.1

template<typename ... arg>
void
SettingsManager::error(const char *fmt, arg ... args)
{
  int size_s = std::snprintf(nullptr, 0, fmt, args...) + 1;

  if (size_s <= 0)
    throw Suscan::Exception(
        __FILE__,
        __LINE__,
        "Failed to measure size in snprintf");

  auto size = static_cast<size_t>(size_s);
  char buffer[size];
  std::snprintf(buffer, size, fmt, args...);

  emit loadError(QString(buffer));
}


SettingsManager::SettingsManager(QObject *parent) : QObject(parent)
{
}

void
SettingsManager::abortLoad()
{
  m_aborted = true;
}

bool
SettingsManager::loadSettings(const char *path)
{
  QSettings settings(path, QSettings::IniFormat);

  m_aborted = false;

  switch (settings.status()) {
    case QSettings::Status::AccessError:
      error("Cannot open file %s. File access error", path);
      return false;

    case QSettings::Status::FormatError:
      error("Cannot load settings from %s (invalid format)", path);
      return false;

    default:
      break;
  }

  m_zmqAddress = settings.value("zmq_address").value<QString>();
  m_tunerFreq  = settings.value("center_frequency").value<qint64>();
  m_lnbFreq    = settings.value("mix_offset").value<qint64>();
  m_correctDc  = settings.value("correct_dc_bias").value<bool>();

  // Read master VFOs
  int msize = settings.beginReadArray("main_vfos");

  for (int i = 0; i < msize && !m_aborted; ++i) {
    settings.setArrayIndex(i);

    // From this, we deduce frequency, bandwidth and name
    auto vfo_out_rate = settings.value("out_rate").value<qint64>();
    auto vfo_freq     = settings.value("frequency").value<qint64>();
    auto out_topic    = settings.value("zmq_topic").value<QString>();
    bool disabled     = settings.value("SigDigger.disabled").value<bool>();

    if (out_topic.size() == 0)
      out_topic = "MASTER_" + QString::number(i + 1);
    auto masterName   = out_topic.toStdString();
    auto bandwidth    = vfo_out_rate * EXTRA_BW_FACTOR;

    if (vfo_freq == 0) {
      error(
        "Central frequency of a master channel `%s' cannot be undefined (or zero)",
         masterName.c_str());
      return false;
    }

    if (vfo_out_rate == 0) {
      error(
        "Bandwidth of master channel `%s' cannot be undefined (or zero)",
        masterName.c_str());
      return false;
    }

    emit createMaster(out_topic, vfo_freq, bandwidth, !disabled);
  }

  settings.endArray();

  if (m_aborted)
    return false;

  // Read VFOs
  int size = settings.beginReadArray("vfos");
  for (int i = 0; i < size && !m_aborted; ++i) {
    settings.setArrayIndex(i);

    // And again, from this we deduce frequency, bandwidth and name
    auto filterbw     = settings.value("filter_bandwidth").value<qint64>();
    auto fiterbw      = settings.value("fiter_bandwidth").value<qint64>();
    auto vfo_freq     = settings.value("frequency").value<qint64>();
    auto out_topic    = settings.value("topic").value<QString>();
    auto demod        = settings.value("SigDigger.demod").value<QString>();
    auto vfo_out_rate = settings.value("out_rate").value<qint64>();
    auto data_rate    = settings.value("data_rate").value<qint64>();
    bool disabled     = settings.value("SigDigger.disabled").value<bool>();
    auto channelName  = out_topic.toStdString();

    // Assume USB if not present
    if (demod == "")
      demod = "audio:usb";

    if (channelName.size() == 0) {
      error("Anonymous channels are not yet supported");
      return false;
    }

    if (filterbw == 0)
      filterbw = fiterbw;

    if (vfo_out_rate == 0) {
      switch (data_rate){
        case 600:
            vfo_out_rate = 12000;
            break;

        case 1200:
            vfo_out_rate = 24000;
            break;

        default:
            vfo_out_rate = 48000;
            break;
      }
    }

    if (filterbw == 0)
      filterbw = vfo_out_rate;

    if (demod == "audio:usb")
      vfo_freq += filterbw / 2;
    else if (demod == "audio:lsb")
      vfo_freq -= filterbw / 2;

    emit createVFO(out_topic, vfo_freq, filterbw, demod, vfo_out_rate, !disabled);
  }

  if (m_aborted)
    return false;

  settings.endArray();

  return true;
}

QString
SettingsManager::getZmqAddres() const
{
  return m_zmqAddress;
}

bool
SettingsManager::getCorrectDC() const
{
  return m_correctDc;
}

SUFREQ
SettingsManager::getTunerFreq() const
{
  return m_tunerFreq;
}

SUFREQ
SettingsManager::getLNBFreq() const
{
  return m_lnbFreq;
}

void
SettingsManager::setZmqAddress(QString addr)
{
  m_zmqAddress = addr;
}

void
SettingsManager::setCorrectDC(bool dc)
{
  m_correctDc = dc;
}

void
SettingsManager::setTunerFreq(SUFREQ freq)
{
  m_tunerFreq = freq;
}

void
SettingsManager::setLNBFreq(SUFREQ off)
{
  m_lnbFreq = off;
}

bool
SettingsManager::saveSettings(const char *path, const MultiChannelForwarder *fwd)
{
  QSettings settings(path, QSettings::IniFormat);
  int ndx;

  settings.setValue("zmq_address", m_zmqAddress);
  settings.setValue("center_fequency", static_cast<qint64>(m_tunerFreq));
  settings.setValue("mix_offset", static_cast<qint64>(m_lnbFreq));
  settings.setValue("correct_dc_bias", m_correctDc);

  settings.beginWriteArray("main_vfos");

  ndx = 0;
  for (auto i = fwd->cbegin(); i != fwd->cend(); ++i) {
    const MasterChannel *master = *i;

    settings.setArrayIndex(ndx++);
    settings.setValue("frequency", static_cast<qint64>(master->frequency));
    settings.setValue("out_rate", static_cast<qint64>(master->bandwidth / EXTRA_BW_FACTOR));
    settings.setValue("out_topic", QString::fromStdString(master->name));
    settings.setValue("SigDigger.disabled", !master->enabled);
  }

  settings.endArray();

  settings.beginWriteArray("vfos");

  ndx = 0;

  for (ChannelHashConstIterator i = fwd->cChanHashBegin(); i != fwd->cChanHashEnd(); ++i) {
    const ChannelDescription *channel = i->second;
    ZeroMQConsumer *consumer = static_cast<ZeroMQConsumer *>(channel->consumer);
    QString demod = QString::fromStdString(consumer->getChannelType());
    qint64 bandwidth = static_cast<qint64>(channel->bandwidth);
    qint64 frequency = static_cast<qint64>(channel->parent->frequency + channel->offset);

    settings.setArrayIndex(ndx++);

    if (demod == "audio:usb")
      frequency -= bandwidth / 2;
    else if (demod == "audio:lsb")
      frequency += bandwidth / 2;

    settings.setValue("frequency", frequency);
    settings.setValue("filter_bandwidth", bandwidth);
    settings.setValue("topic", QString::fromStdString(channel->name));
    settings.setValue("SigDigger.demod", demod);
    settings.setValue("out_rate", static_cast<qint64>(consumer->getSampRate()));
    settings.setValue("SigDigger.disabled", !consumer->isEnabled());
  }

  settings.endArray();

  settings.sync();

  auto status = settings.status();

  return status == QSettings::Status::NoError;
}
