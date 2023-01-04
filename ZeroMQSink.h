#ifndef ZEROMQSINK_H
#define ZEROMQSINK_H

#include <MultiChannelForwarder.h>
#include <string>

class ZeroMQSink : public ChannelConsumer
{
  SUFLOAT m_sampRate = 0;
  std::string m_channelType;

public:
  ZeroMQSink(const char *type, SUFLOAT audioSampRate);
  SUFLOAT getSampRate() const;

  virtual void opened(
      Suscan::Analyzer *,
      Suscan::Handle,
      ChannelDescription const &,
      Suscan::Config const &) override;
  virtual void samples(const SUCOMPLEX *, SUSCOUNT) override;
  virtual void closed() override;

  virtual ~ZeroMQSink();
};

#endif // ZEROMQSINK_H
