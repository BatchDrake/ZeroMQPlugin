#ifndef MULTICHANNELFORWARDER_H
#define MULTICHANNELFORWARDER_H

#include <Suscan/Analyzer.h>
#include <Suscan/Messages/InspectorMessage.h>
#include <map>
#include <list>
#include <unordered_map>

struct ChannelDescription;

class ChannelConsumer {
public:
  virtual void opened(
      Suscan::Analyzer *,
      Suscan::Handle,
      ChannelDescription const &,
      Suscan::Config const &) = 0;
  virtual void samples(const SUCOMPLEX *, SUSCOUNT) = 0;
  virtual void closed() = 0;
  virtual ~ChannelConsumer();
};

struct MasterChannel;

typedef std::list<ChannelDescription>::iterator ChannelListIterator;
typedef std::list<MasterChannel *>::iterator MasterListIterator;
typedef std::list<MasterChannel *>::const_iterator MasterListConstIterator;

struct ChannelDescription {
  MasterChannel *parent;
  std::string    name;
  SUFREQ         offset;
  SUFLOAT        bandwidth;

  SUFLOAT        sampRate;
  std::string    inspClass;
  Suscan::Config inspConfig;

  ChannelConsumer   *consumer = 0;
  ChannelListIterator iter;
  Suscan::Handle     handle  = SUSCAN_INVALID_HANDLE_VALUE;
  Suscan::RequestId  reqId;
  bool               opening = false;
  bool               deleted = false;

  inline bool
  isOpen() const
  {
    return handle != SUSCAN_INVALID_HANDLE_VALUE;
  }

  ~ChannelDescription();
};

struct MasterChannel {
  std::string    name;
  SUFREQ         frequency;
  SUFLOAT        bandwidth;

  std::list<ChannelDescription> channels;
  MasterListIterator  iter;
  Suscan::Handle     handle  = SUSCAN_INVALID_HANDLE_VALUE;
  Suscan::RequestId  reqId;
  bool               opening = false;
  unsigned int       open_count = 0;
  bool               deleted = false;

  inline bool
  isOpen() const
  {
    return handle != SUSCAN_INVALID_HANDLE_VALUE;
  }

  inline bool
  isEmpty() const
  {
    return channels.empty();
  }
};

class MultiChannelForwarder
{
  Suscan::Analyzer *m_analyzer = nullptr;
  bool m_opening = false;
  bool m_opened = false;
  SUFREQ m_freqMin = INFINITY;
  SUFREQ m_freqMax = -INFINITY;
  std::string m_errors;
  bool m_failed = false;

  // Owner: This holds the structure of the channels to open
  std::list<MasterChannel *> masterList;

  // Fast lookup of master and channels
  std::unordered_map<std::string, MasterChannel *> masterHash;
  std::unordered_map<std::string, ChannelDescription *> channelHash;

  // This is a map that enumerates opened masters
  std::map<Suscan::Handle, MasterChannel *> masterMap;
  std::map<Suscan::RequestId, MasterChannel *> pendingMasterMap;
  bool promoteMaster(Suscan::RequestId, Suscan::Handle);

  // This is a map that relates opened channels with consumers
  std::map<Suscan::Handle, ChannelDescription *> channelMap;
  std::map<Suscan::RequestId, ChannelDescription *> pendingChannelMap;
  bool promoteChannel(Suscan::RequestId, Suscan::Handle);

  void keepOpening();
  MasterChannel *getMasterFromRequest(Suscan::RequestId) const;
  ChannelDescription *getChannelFromRequest(Suscan::RequestId) const;
  ChannelDescription *getChannelFromHandle(Suscan::Handle) const;


  MasterListIterator deleteMaster(MasterListIterator);
  ChannelListIterator deleteChannel(ChannelListIterator);

  // This makes sure that all channels get back to a sane state
  void reset();

  // Hahaha oh my God the syntax
  template<typename ... arg> void error(const char *fmt, arg ...);

public:
  MasterListIterator
  begin()
  {
    return masterList.begin();
  }

  MasterListIterator
  end()
  {
    return masterList.end();
  }

  MasterListConstIterator
  cbegin() const
  {
    return masterList.cbegin();
  }

  MasterListConstIterator
  cend() const
  {
    return masterList.cend();
  }

  MasterChannel *
  findMaster(const char *name) const
  {
    auto it = masterHash.find(name);

    if (it == masterHash.cend())
      return nullptr;

    if (it->second->deleted)
      return nullptr;

    return it->second;
  }

  ChannelDescription *
  findChannel(const char *name) const
  {
    auto it = channelHash.find(name);

    if (it == channelHash.cend())
      return nullptr;

    if (it->second->deleted)
      return nullptr;

    return it->second;
  }

  bool
  removeMaster(const char *name)
  {
    auto it = masterHash.find(name);

    if (it == masterHash.cend())
      return false;

    return removeMaster(it->second->iter);
  }

  bool
  empty() const
  {
    return masterList.empty();
  }

  bool failed() const; // Something went wrong
  std::string getErrors() const;
  void clearErrors();
  bool canOpen() const; // Returns if channels can be opened
  bool canCenter() const; // Returns if all masters fit
  bool center(); // Center masters
  SUFREQ span() const;
  SUFREQ getCenter() const;
  bool isOpen() const;

  void setAnalyzer(Suscan::Analyzer *); // Used to update changes
  void openAll(); // Used to open all masters and channels
  void closeAll(); // Used to close all masters and channels

  bool processMessage(Suscan::InspectorMessage const &);
  bool feedSamplesMessage(Suscan::SamplesMessage const &);

  MasterChannel *makeMaster(const char *, SUFREQ freq, SUFLOAT bw);
  bool removeMaster(MasterListIterator);
  bool removeMaster(MasterChannel *);

  MasterListConstIterator findMaster(SUFREQ freq, SUFLOAT bw) const;

  ChannelDescription *makeChannel(
      const char *,
      SUFREQ freq,
      SUFLOAT bw,
      const char *inspClass,
      ChannelConsumer *);
  bool removeChannel(ChannelListIterator);
  bool removeChannel(ChannelDescription *);

  MultiChannelForwarder();
  ~MultiChannelForwarder();
};

#endif // MULTICHANNELFORWARDER_H
