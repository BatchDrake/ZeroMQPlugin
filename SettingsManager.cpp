#include "SettingsManager.h"
#include "MultiChannelForwarder.h"

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

    emit createMaster(out_topic, vfo_freq, bandwidth);
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
    auto data_rate    = settings.value("date_rate").value<qint64>();
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

    emit createVFO(out_topic, vfo_freq, filterbw, demod, vfo_out_rate);
  }

  if (m_aborted)
    return false;

  settings.endArray();

  return true;
}

bool
SettingsManager::saveSettings(const char *path, const MultiChannelForwarder *)
{
  return false;
}
