#include "ZeroMQSink.h"
#include <analyzer/inspector/params.h>
#include <zmq.hpp>

bool
ZeroMQSink::bind(const char *url)
{
  if (m_state)
    return false;

  m_zmq_socket = new zmq::socket_t(m_zmq_ctx, zmq::socket_type::pub);
  m_zmq_socket->bind(url);
  m_state = true;

  return true;
}

bool
ZeroMQSink::write(
    const char *topic,
    unsigned int sampleRate,
    const SUCOMPLEX *samples,
    SUSCOUNT size,
    ZeroMQDeliveryMask mask)
{
  uint32_t sampRate = sampleRate;
  unsigned int allocSize;

  if (!m_state)
    return false;

  // Convert data
  allocSize = mask == ZEROMQ_DELIVER_COMPLEX ? 2 * size : size;
  if (m_sampleBuffer.size() < allocSize)
    m_sampleBuffer.resize(allocSize);

  switch (mask) {
    case ZEROMQ_DELIVER_REAL:
      for (SUSCOUNT i = 0; i < size; ++i)
        m_sampleBuffer[i] = SU_FLOOR(SU_C_REAL(samples[i]) * ((1 << 15) - 1));

      break;

    case ZEROMQ_DELIVER_IMAG:
      for (SUSCOUNT i = 0; i < size; ++i)
        m_sampleBuffer[i] = SU_FLOOR(SU_C_IMAG(samples[i]) * ((1 << 15) - 1));
      break;

    case ZEROMQ_DELIVER_COMPLEX:
      for (SUSCOUNT i = 0; i < size; ++i) {
        m_sampleBuffer[2 * i + 0] = SU_FLOOR(SU_C_REAL(samples[i]) * ((1 << 15) - 1));
        m_sampleBuffer[2 * i + 1] = SU_FLOOR(SU_C_IMAG(samples[i]) * ((1 << 15) - 1));
      }
      break;
  }

  // Deliver sample rate
  m_zmq_socket->send(
        zmq::buffer(std::string(topic)),
        zmq::send_flags::sndmore);
  m_zmq_socket->send(
        zmq::buffer(&sampRate, sizeof(uint32_t)),
        zmq::send_flags::sndmore);
  m_zmq_socket->send(zmq::buffer(m_sampleBuffer.data(), sizeof(int16_t) * allocSize));

  return 0;
}

bool
ZeroMQSink::disconnect()
{
  if (!m_state)
    return false;

  delete m_zmq_socket;

  m_zmq_socket = nullptr;
  m_state = false;

  return true;
}

ZeroMQSink::~ZeroMQSink()
{
  if (m_zmq_socket != nullptr)
    delete m_zmq_socket;
}

ZeroMQConsumer::ZeroMQConsumer(
    ZeroMQSink *sink,
    const char *chanType,
    SUFLOAT audioSampRate)
{


  m_sampRate    = audioSampRate;
  m_channelType = chanType;
  m_zmq_sink    = sink;

  m_mask =
      strcmp(chanType, "raw") == 0
      ? ZEROMQ_DELIVER_COMPLEX
      : ZEROMQ_DELIVER_REAL;
}

ZeroMQConsumer::~ZeroMQConsumer()
{
  if (m_fp != nullptr)
    fclose(m_fp);
}

void
ZeroMQConsumer::opened(
    Suscan::Analyzer *analyzer,
    Suscan::Handle handle,
    ChannelDescription const &channel,
    Suscan::Config const &config)
{
  QString fileString;
  std::string file;

  m_topic = channel.name;

  if (channel.inspClass == "raw") {
    m_sampRate = channel.sampRate;
  } else if (channel.inspClass == "audio") {
    SUFREQ f_edge;
    SUFLOAT bw_new = m_sampRate * .45;
    Suscan::AnalyzerSourceInfo info = analyzer->getSourceInfo();
    Suscan::Config newConfig(config.getInstance());
    uint64_t demod = SUSCAN_INSPECTOR_AUDIO_DEMOD_DISABLED;
    // Audio inspector. In this case, we need to configure the appropriate
    // demodulator accordingly

    newConfig.set("audio.sample-rate", static_cast<uint64_t>(m_sampRate));
    newConfig.set("audio.cutoff", static_cast<SUFLOAT>(bw_new));
    newConfig.set("audio.volume", 1.f);

    if (m_channelType == "audio:fm")
      demod = SUSCAN_INSPECTOR_AUDIO_DEMOD_FM;
    else if (m_channelType == "audio:am")
      demod = SUSCAN_INSPECTOR_AUDIO_DEMOD_AM;
    else if (m_channelType == "audio:usb")
      demod = SUSCAN_INSPECTOR_AUDIO_DEMOD_USB;
    else if (m_channelType == "audio:lsb")
      demod = SUSCAN_INSPECTOR_AUDIO_DEMOD_LSB;

    newConfig.set("audio.demodulator", demod);


    // We need to improve filtering, so this is what we will do:
    //
    // For USB signals:
    //    1. The left edge of the signal is at f_edge = fc - bandwidth / 2
    //    2. The new bandwidth is just going to be bw_new = m_sampRate * .45
    //    3. The new center is going to be at f_edge + bw_new / 2
    //
    // For LSB signals:
    //    1. The right edge of the signal is at f_edge = fc + bandwidth / 2
    //    2. The new bandwidth is again bw_new = m_sampRate * .45
    //    3. The new center is going to be at f_edge - bw_new / 2

    if (demod == SUSCAN_INSPECTOR_AUDIO_DEMOD_USB) {
      f_edge = channel.offset - channel.bandwidth / 2;
      analyzer->setInspectorFreq(handle, f_edge + bw_new / 2);
      analyzer->setInspectorBandwidth(handle, bw_new);
    } else if (demod == SUSCAN_INSPECTOR_AUDIO_DEMOD_LSB) {
      f_edge = channel.offset + channel.bandwidth / 2;
      analyzer->setInspectorFreq(handle, f_edge - bw_new / 2);
      analyzer->setInspectorBandwidth(handle, bw_new);
    }

    // As soon as we start to receive samples, we now we were on the right
    // track, no need to track this request.
    analyzer->setInspectorConfig(handle, newConfig);
  }

  fileString = QString::fromStdString(m_channelType) + "_" + QString::number(m_sampRate) + ".raw";
  file = fileString.toStdString();

  m_fp = fopen(file.c_str(), "wb");
}

void
ZeroMQConsumer::samples(const SUCOMPLEX *samples, SUSCOUNT size)
{
  m_zmq_sink->write(
        m_topic.c_str(),
        static_cast<unsigned>(m_sampRate),
        samples,
        size,
        m_mask);
  if (m_fp != nullptr)
    fwrite(samples, size * sizeof(SUCOMPLEX), 1, m_fp);
}

void
ZeroMQConsumer::closed()
{
  if (m_fp != nullptr)
    fclose(m_fp);

  m_fp = nullptr;
}
