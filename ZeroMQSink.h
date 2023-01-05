#ifndef ZEROMQSINK_H
#define ZEROMQSINK_H

#include <MultiChannelForwarder.h>
#include <string>
#include <vector>
#include <zmq.hpp>
#include <cstdio>

enum ZeroMQDeliveryMask {
  ZEROMQ_DELIVER_REAL = 1,
  ZEROMQ_DELIVER_IMAG = 2,
  ZEROMQ_DELIVER_COMPLEX = 3
};

class ZeroMQSink {
  bool m_state = false;
  zmq::context_t m_zmq_ctx;
  zmq::socket_t *m_zmq_socket = nullptr;

  std::vector<int16_t> m_sampleBuffer;

public:
  bool bind(const char *url);
  bool write(
      const char *topic,
      unsigned int sampleRate,
      const SUCOMPLEX *samples,
      SUSCOUNT size,
      ZeroMQDeliveryMask mask = ZEROMQ_DELIVER_REAL);
  bool disconnect();
  ~ZeroMQSink();
};

class ZeroMQConsumer : public ChannelConsumer
{
  SUFLOAT m_sampRate = 0;
  std::string m_channelType;
  std::string m_topic;
  ZeroMQSink *m_zmq_sink = nullptr;
  ZeroMQDeliveryMask m_mask;
  FILE *m_fp = nullptr;

public:
  ZeroMQConsumer(ZeroMQSink *, const char *type, SUFLOAT audioSampRate);
  SUFLOAT getSampRate() const;

  virtual void opened(
      Suscan::Analyzer *,
      Suscan::Handle,
      ChannelDescription const &,
      Suscan::Config const &) override;
  virtual void samples(const SUCOMPLEX *, SUSCOUNT) override;
  virtual void closed() override;

  virtual ~ZeroMQConsumer();
};

#endif // ZEROMQSINK_H
