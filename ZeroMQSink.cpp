#include "ZeroMQSink.h"
#include <analyzer/inspector/params.h>

ZeroMQSink::ZeroMQSink(const char *chanType, SUFLOAT audioSampRate)
{
  m_sampRate    = audioSampRate;
  m_channelType = chanType;
}

ZeroMQSink::~ZeroMQSink()
{
}

void
ZeroMQSink::opened(
    Suscan::Analyzer *analyzer,
    Suscan::Handle handle,
    ChannelDescription const &channel,
    Suscan::Config const &config)
{
  printf("Inspector %s: channel opened. Rate: %g\n", m_channelType.c_str(), channel.sampRate);

  if (channel.inspClass == "raw") {
    m_sampRate = channel.sampRate;
  } else if (channel.inspClass == "audio") {
    Suscan::Config newConfig(config.getInstance());
    uint64_t demod = SUSCAN_INSPECTOR_AUDIO_DEMOD_DISABLED;
    // Audio inspector. In this case, we need to configure the appropriate
    // demodulator accordingly

    newConfig.set("audio.sample-rate", static_cast<uint64_t>(m_sampRate));
    newConfig.set("audio.cutoff", static_cast<SUFLOAT>(m_sampRate));
    newConfig.set("audio.volume", 1.f); // We handle this at UI level

    if (m_channelType == "audio:fm")
      demod = SUSCAN_INSPECTOR_AUDIO_DEMOD_FM;
    else if (m_channelType == "audio:am")
      demod = SUSCAN_INSPECTOR_AUDIO_DEMOD_AM;
    else if (m_channelType == "audio:usb")
      demod = SUSCAN_INSPECTOR_AUDIO_DEMOD_USB;
    else if (m_channelType == "audio:lsb")
      demod = SUSCAN_INSPECTOR_AUDIO_DEMOD_LSB;

    newConfig.set("audio.demodulator", demod);

    // The central frequency of sideband inspectors is a bit off and needs to
    // be corrected. We correct this offset directly from its bandwidth
    switch (demod) {
      case SUSCAN_INSPECTOR_AUDIO_DEMOD_USB:
        analyzer->setInspectorFreq(handle, channel.offset + .5 * channel.bandwidth);
        break;

      case SUSCAN_INSPECTOR_AUDIO_DEMOD_LSB:
        analyzer->setInspectorFreq(handle, channel.offset - .5 * channel.bandwidth);
        break;

    }

    // As soon as we start to receive samples, we now we were on the right
    // track, no need to track this request.
    analyzer->setInspectorConfig(handle, newConfig);
  }
}

void
ZeroMQSink::samples(const SUCOMPLEX *, SUSCOUNT samples)
{
  printf("Inspector %s: %ld samples\n", m_channelType.c_str(), samples);
}

void
ZeroMQSink::closed()
{
  printf("Inspector %s: closed\n",  m_channelType.c_str());
}
