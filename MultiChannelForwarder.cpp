#include "MultiChannelForwarder.h"
#include <memory>
#include <string>
#include <cstdio>

ChannelConsumer::~ChannelConsumer()
{
}

ChannelDescription::~ChannelDescription()
{
  delete consumer;
}

ChannelListIterator
MultiChannelForwarder::deleteChannel(ChannelListIterator it)
{
  ChannelDescription *channel = &*it;
  bool opened = channel->isOpen();

  // First: remove channel from the channel hash
  channelHash.erase(channel->name);

  // Second: If it is either in the pending or opened maps, remove from them
  if (opened)
    channelMap.erase(channel->handle);
  else if (channel->opening)
    pendingChannelMap.erase(channel->reqId);

  // Third: delete channel from the corresponding master. If opened, decrease counter
  // This automatically triggers the destructor
  if (opened)
    --channel->parent->open_count;

  auto next = channel->parent->channels.erase(it);

  // Done!

  return next;
}


MasterListIterator
MultiChannelForwarder::deleteMaster(MasterListIterator it)
{
  MasterChannel *master = *it;
  bool opened = master->isOpen();

  // First: remove from the masterList
  auto next = masterList.erase(it);

  // Second: remove form the master hash
  masterHash.erase(master->name);

  // Third: traverse all channels and delete them one by one
  while (!master->channels.empty()) {
    auto channel = master->channels.begin();
    deleteChannel(channel);
  }

  // Delete from the rest of maps
  if (opened)
    masterMap.erase(master->handle);
  else if (master->opening)
    pendingMasterMap.erase(master->reqId);

  // Also, deleting the master implies recalculating the frequency limits
  m_freqMin = +INFINITY;
  m_freqMax = -INFINITY;

  for (auto p : masterList) {
    if (p->frequency - p->bandwidth / 2 < m_freqMin)
      m_freqMin = p->frequency - p->bandwidth / 2;

    if (p->frequency + p->bandwidth / 2 > m_freqMax)
      m_freqMax = p->frequency + p->bandwidth / 2;

  }
  // Done!
  return next;
}

void
MultiChannelForwarder::reset()
{
  auto j = masterList.begin();

  while (j != masterList.end()) {
    auto p = *j;

    // This was deleted? Delete now.
    if (p->deleted) {
      j = deleteMaster(j);
    } else {
      auto i = p->channels.begin();

      p->handle     = SUSCAN_INVALID_HANDLE_VALUE;
      p->opening    = false;
      p->open_count = false;

      while (i != p->channels.end()) {
        // Channel was deleted? Delete now.
        i->handle    = SUSCAN_INVALID_HANDLE_VALUE;
        i->opening   = false;

        if (i->deleted)
          i = deleteChannel(i);
        else
          ++i;
      }

      ++j;
    }
  }

  masterMap.clear();
  pendingMasterMap.clear();

  m_opening = false;
  m_opened = false;
  clearErrors();
}

void
MultiChannelForwarder::setAnalyzer(Suscan::Analyzer *analyzer)
{
  // NO-OP
  if (analyzer == m_analyzer)
    return;

  if (m_analyzer != nullptr)
    closeAll();
  else
    reset();

  m_analyzer = analyzer;
}

bool
MultiChannelForwarder::canOpen() const
{
  if (m_analyzer != nullptr) {
    Suscan::AnalyzerSourceInfo info = m_analyzer->getSourceInfo();
    SUFREQ tunerFreq   = info.getFrequency();
    SUFREQ sampleRate  = info.getSampleRate();
    SUFREQ currFreqMin = tunerFreq - sampleRate / 2;
    SUFREQ currFreqMax = tunerFreq + sampleRate / 2;

    if (!canCenter())
      return false;

    return currFreqMin < m_freqMin && m_freqMax < currFreqMax;
  }

  return false;
}

SUFREQ
MultiChannelForwarder::span() const
{
  return m_freqMax - m_freqMin;
}

SUFREQ
MultiChannelForwarder::getCenter() const
{
  return .5 * (m_freqMax + m_freqMin);
}

bool
MultiChannelForwarder::canCenter() const
{
  if (m_analyzer == nullptr)
    return false;

  Suscan::AnalyzerSourceInfo info = m_analyzer->getSourceInfo();

  if (span() > info.getSampleRate())
    return false;

  return true;
}

bool
MultiChannelForwarder::center()
{
  SUFREQ tunerFreq;

  if (!canCenter())
    return false;

  tunerFreq = .5 * (m_freqMax + m_freqMin);

  m_analyzer->setFrequency(tunerFreq);

  return true;
}

bool
MultiChannelForwarder::isOpen() const
{
  return m_opened;
}


MasterChannel *
MultiChannelForwarder::getMasterFromRequest(Suscan::RequestId reqId) const
{
  auto it = pendingMasterMap.find(reqId);

  if (it == pendingMasterMap.cend())
    return nullptr;

  return it->second;
}

ChannelDescription *
MultiChannelForwarder::getChannelFromRequest(Suscan::RequestId reqId) const
{
  auto it = pendingChannelMap.find(reqId);

  if (it == pendingChannelMap.cend())
    return nullptr;

  return it->second;
}

ChannelDescription *
MultiChannelForwarder::getChannelFromHandle(Suscan::Handle reqId) const
{
  auto it = channelMap.find(reqId);

  if (it == channelMap.cend())
    return nullptr;

  return it->second;
}
bool
MultiChannelForwarder::promoteMaster(Suscan::RequestId reqId, Suscan::Handle hnd)
{
  MasterChannel *master;

  auto it = pendingMasterMap.find(reqId);

  if (it == pendingMasterMap.cend())
    return false;

  master = it->second;

  pendingMasterMap.erase(it);

  if (master->deleted) {
    // If master was deleted, we silently close it and return false. And also
    // remote it from the list
    m_analyzer->closeInspector(hnd);

    master->deleted = false;
    master->handle = SUSCAN_INVALID_HANDLE_VALUE;
    master->opening = false;

    removeMaster(master->iter);

    return false;
  }

  master->handle  = hnd;
  master->opening = false;

  masterMap[hnd]  = master;

  return true;
}

bool
MultiChannelForwarder::promoteChannel(Suscan::RequestId reqId, Suscan::Handle hnd)
{
  ChannelDescription *channel;

  auto it = pendingChannelMap.find(reqId);

  if (it == pendingChannelMap.cend())
    return false;

  channel = it->second;

  pendingChannelMap.erase(it);

  if (channel->deleted) {
    // If channel was deleted, we silently close it and return false. And also
    // remote it from the list
    m_analyzer->closeInspector(hnd);

    channel->deleted = false;
    channel->handle = SUSCAN_INVALID_HANDLE_VALUE;
    channel->opening = false;

    removeChannel(channel->iter);

    return false;
  }

  channel->handle  = hnd;
  channel->opening = false;

  ++channel->parent->open_count;

  channelMap[hnd]  = channel;

  return true;
}

void
MultiChannelForwarder::keepOpening()
{
  if (!m_opened) {
    // For all masterLists
    //  1. If master set is not opened nor opening: open
    //  2. If master set is opened: check if there are
    //     pending open requests

    for (auto p : masterList) {
      bool opened = p->isOpen();
      bool fullyOpened = opened && p->open_count == p->channels.size();

      // Neither opened nor opening: open master.
      if (!opened && !p->opening) {
        Suscan::AnalyzerSourceInfo info = m_analyzer->getSourceInfo();
        Suscan::Channel channel;

        p->reqId = m_analyzer->allocateRequestId();
        channel.fc = p->frequency - info.getFrequency();
        channel.fHigh = channel.fc + p->bandwidth / 2;
        channel.fLow  = channel.fc - p->bandwidth / 2;

        // Open master (no precision)
        m_analyzer->open("multicarrier", channel, p->reqId);
        pendingMasterMap[p->reqId] = p;

        p->opening = true;
      }

      // Opened: open all subchannels
      if (opened && !fullyOpened) {
        for (auto c = p->channels.begin();
             c != p->channels.end();
             ++c) {
          bool chan_opened = c->isOpen();
          if (!chan_opened && !c->opening) {
            Suscan::Channel channel;
            SUFLOAT extraRoom = 2 * c->bandwidth;
            if (extraRoom > p->bandwidth)
              extraRoom = p->bandwidth;

            c->reqId = m_analyzer->allocateRequestId();
            channel.fc    = c->offset;
            channel.fHigh = + .5 * extraRoom;
            channel.fLow  = - .5 * extraRoom;
            channel.bw    = extraRoom; // Give some extra room at allocation
            channel.ft    = 0;

            m_analyzer->openEx(
                  c->inspClass,
                  channel,
                  true,
                  p->handle,
                  c->reqId);

            pendingChannelMap[c->reqId] = &*c;
            c->opening = true;
          }
        }
      }
    }
  }
}

void
MultiChannelForwarder::openAll()
{
  if (m_analyzer != nullptr && !m_opening && !m_opened) {
    m_opening = true;
    keepOpening();
  }
}

void
MultiChannelForwarder::closeAll()
{
  if (m_opened) {
    if (m_analyzer != nullptr) {
      for (auto p : masterList) {
        if (p->isOpen()) {
          m_analyzer->closeInspector(p->handle);
          p->handle = SUSCAN_INVALID_HANDLE_VALUE;
          p->open_count = 0;
          p->opening = false;
          p->deleted = false;

          for (auto c = p->channels.begin();
               c != p->channels.end();
               ++c) {
            if (c->isOpen()) {
              c->consumer->closed();
              c->handle = SUSCAN_INVALID_HANDLE_VALUE;
              c->opening = false;
              c->deleted = false;
            }
          }
        }
      }
    }
  }

  masterMap.clear();
  channelMap.clear();

  pendingMasterMap.clear();
  pendingChannelMap.clear();

  m_opened = false;
  m_opening = false;
}

template<typename ... arg>
void
MultiChannelForwarder::error(const char *fmt, arg ... args)
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

  m_errors += buffer;
  m_failed = true;
}

bool
MultiChannelForwarder::failed() const
{
  return m_failed;
}

void
MultiChannelForwarder::clearErrors()
{
  m_errors = "";
  m_failed = false;
}

std::string
MultiChannelForwarder::getErrors() const
{
  return m_errors;
}

bool
MultiChannelForwarder::processMessage(Suscan::InspectorMessage const &msg)
{
  ChannelDescription *ch;
  bool changes = false;

  if (m_opening) {
    // This is where we inspect the result of the opening process. In order
    // Changes here, time to keep opening

    switch (msg.getKind()) {
      case SUSCAN_ANALYZER_INSPECTOR_MSGKIND_OPEN:
        // Determine whether it is a master, a slave or something else
        // If it is ours, make sure its id equals to its handle.

        // 1. Find master
        // 2. If found, promote
        // 3. If not, find channel
        // 4. If found, promote
        // 5. If anything was opened, check if we must transit to opened
        if (!promoteMaster(msg.getRequestId(), msg.getHandle())) {
          ch = getChannelFromRequest(msg.getRequestId());
          if (ch != nullptr) {
            if (promoteChannel(msg.getRequestId(), msg.getHandle())) {
              m_analyzer->setInspectorId(msg.getHandle(), msg.getHandle());
              m_analyzer->setInspectorBandwidth(msg.getHandle(), ch->bandwidth);
              ch->sampRate = msg.getEquivSampleRate();
              ch->consumer->opened(
                    m_analyzer,
                    msg.getHandle(),
                    *ch,
                    Suscan::Config(msg.getCConfig()));
              m_analyzer->setInspectorWatermark(msg.getHandle(), 12000);
              changes = true;
            }
          }
        } else {
          changes = true;
        }

        if (changes)
          keepOpening();

        m_opened = !(pendingMasterMap.size() > 0 || pendingChannelMap.size() > 0);
        m_opening = !m_opened;
        break;

      case SUSCAN_ANALYZER_INSPECTOR_MSGKIND_WRONG_HANDLE:
        // Failed to open one of the subcarrier inspectors
        if (pendingChannelMap.find(msg.getRequestId()) != pendingChannelMap.end()) {
          closeAll();
          error("Failed to open subcarrier inspector (wrong handle)\n");

          changes = true;
        }
        break;

      case SUSCAN_ANALYZER_INSPECTOR_MSGKIND_INVALID_CHANNEL:
        // Failed to open a channel
        if (pendingChannelMap.find(msg.getRequestId()) != pendingChannelMap.end()
            || pendingMasterMap.find(msg.getRequestId()) != pendingMasterMap.end()) {
          closeAll();
          error("Failed to open a channel (invalid limits?)\n");

          changes = true;
        }
        break;

      default:
        // Anything else goes in here.
        break;
    }
  }

  return changes;
}

bool
MultiChannelForwarder::feedSamplesMessage(Suscan::SamplesMessage const &msg)
{
  ChannelDescription *ch;

  ch = getChannelFromHandle(msg.getInspectorId());
  if (ch != nullptr) {
    ch->consumer->samples(msg.getSamples(), msg.getCount());
    return true;
  }

  return false;
}

MasterChannel *
MultiChannelForwarder::makeMaster(const char *name, SUFREQ frequency, SUFLOAT bandwidth)
{
  MasterChannel *master = new MasterChannel;

  if (findMaster(name) != nullptr) {
    error("Master channel `%s' already exists.\n", name);
    return nullptr;
  }

  master->name      = name;
  master->frequency = frequency;
  master->bandwidth = bandwidth;

  master->iter = masterList.insert(masterList.cend(), master);
  masterHash[name] = master;

  if (frequency - bandwidth / 2 < m_freqMin)
    m_freqMin = frequency - bandwidth / 2;

  if (frequency + bandwidth / 2 > m_freqMax)
    m_freqMax = frequency + bandwidth / 2;

  if (m_opened) {
    m_opening = true;
    m_opened = false;
  }

  if (m_opening)
    keepOpening();

  return master;
}

bool
MultiChannelForwarder::removeMaster(MasterListIterator it)
{
  bool delayed = false;

  // The removal of a master may occur immediately or be delayed

  if (m_opened) {
    MasterChannel *master = *it;
    bool master_opened = master->isOpen();

    if (master->opening) {
      // This refers to a lazy closure. Mark as deleted and delete later.
      master->deleted = true;
      delayed = true;
    } else if (master_opened) {
      // We do not need to traverse the subchannels here. The closure
      // of the master triggers the close of the children
      m_analyzer->closeInspector(master->handle);
      master->handle = SUSCAN_INVALID_HANDLE_VALUE;
    }
  }

  // Nothing delayed. We can delete now.
  if (!delayed)
    deleteMaster(it);

  return !delayed;
}

bool
MultiChannelForwarder::removeMaster(MasterChannel *master)
{
  return removeMaster(master->iter);
}


MasterListConstIterator
MultiChannelForwarder::findMaster(SUFREQ frequency, SUFLOAT bandwidth) const
{
  for (auto i = cbegin(); i != cend(); ++i) {
    auto p = *i;

    if (
        p->frequency - p->bandwidth / 2 <= frequency - bandwidth / 2
        && frequency + bandwidth / 2 <= p->frequency + p->bandwidth / 2
        && !p->deleted)
      return i;
  }

  return cend();
}

ChannelDescription *
MultiChannelForwarder::makeChannel(
    const char *name,
    SUFREQ freq,
    SUFLOAT bw,
    const char *inspClass,
    ChannelConsumer *co)
{
  auto it = findMaster(freq, bw);

  if (it == cend()) {
    error("Provided channel is outside any master channel.\n");
    return nullptr;
  }

  if (findChannel(name) != nullptr) {
    error("Channel `%s' already exists\n", name);
    return nullptr;
  }

  MasterChannel *master = *it;
  ChannelDescription *channel;

  master->channels.push_front(ChannelDescription());
  channel = &*master->channels.begin();
  channel->name      = name;
  channel->offset    = freq - master->frequency;
  channel->bandwidth = bw;
  channel->consumer  = co;
  channel->parent    = master;
  channel->inspClass = inspClass;
  channel->iter = master->channels.begin();

  channelHash[name] = channel;

  if (m_opened) {
    m_opening = true;
    m_opened = false;
  }

  if (m_opening)
    keepOpening();

  return channel;
}

bool
MultiChannelForwarder::removeChannel(ChannelListIterator it)
{
  bool delayed = false;

  // The removal of a channel may occur immediately or be delayed
  if (m_opened) {
    ChannelDescription *channel = &*it;
    bool channel_opened = channel->isOpen();

    if (channel->opening) {
      // This refers to a lazy closure. Mark as deleted and delete later.
      channel->deleted = true;
      delayed = true;
    } else if (channel_opened) {
      // We do not need to traverse the subchannels here. The closure
      // of the master triggers the close of the children
      m_analyzer->closeInspector(channel->handle);
      channel->handle = SUSCAN_INVALID_HANDLE_VALUE;
    }
  }

  // Nothing delayed. We can delete now.
  if (!delayed)
    deleteChannel(it);

  return !delayed;
}

bool
MultiChannelForwarder::removeChannel(ChannelDescription *channel)
{
  return removeChannel(channel->iter);
}

MultiChannelForwarder::MultiChannelForwarder()
{

}

MultiChannelForwarder::~MultiChannelForwarder()
{
  for (auto p : masterList)
    delete p;
}
