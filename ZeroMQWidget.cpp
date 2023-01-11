//
//    ZeroMQWidget.cpp: ZeroMQ widget implementation
//    Copyright (C) 2022 Gonzalo José Carracedo Carballal
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU Lesser General Public License as
//    published by the Free Software Foundation, either version 3 of the
//    License, or (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful, but
//    WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU Lesser General Public License for more details.
//
//    You should have received a copy of the GNU Lesser General Public
//    License along with this program.  If not, see
//    <http://www.gnu.org/licenses/>
//
#include "ZeroMQWidgetFactory.h"
#include "ZeroMQWidget.h"
#include "ui_ZeroMQWidget.h"

#include <MultiChannelForwarder.h>
#include <MultiChannelTreeModel.h>
#include <SuWidgetsHelpers.h>
#include <AddChanDialog.h>
#include <AddMasterDialog.h>
#include <QMessageBox>
#include <ZeroMQSink.h>
#include <SettingsManager.h>
#include <QFileDialog>
#include <QDir>
#include <UIMediator.h>
#include <MainSpectrum.h>

using namespace SigDigger;

#define STRINGFY(x) #x
#define STORE(field) obj.set(STRINGFY(field), this->field)
#define LOAD(field) this->field = conf.get(STRINGFY(field), this->field)


//////////////////////////// Widget config /////////////////////////////////////
void
ZeroMQWidgetConfig::deserialize(Suscan::Object const &conf)
{
  LOAD(collapsed);
  LOAD(trackTuner);
  LOAD(zmqURL);
  LOAD(startPublish);
}

Suscan::Object &&
ZeroMQWidgetConfig::serialize()
{
  Suscan::Object obj(SUSCAN_OBJECT_TYPE_OBJECT);

  obj.setClass("ZeroMQWidgetConfig");

  STORE(collapsed);
  STORE(trackTuner);
  STORE(zmqURL);
  STORE(startPublish);

  return persist(obj);
}

//////////////////////// Widget implementation /////////////////////////////////
ZeroMQWidget::ZeroMQWidget(
    ZeroMQWidgetFactory *factory,
    UIMediator *mediator,
    QWidget *parent) :
  ToolWidget(factory, mediator, parent),
  m_ui(new Ui::ZeroMQWidget)
{
  m_ui->setupUi(this);

  m_spectrum  = mediator->getMainSpectrum();
  m_forwarder = new MultiChannelForwarder();
  m_treeModel = new MultiChannelTreeModel(m_forwarder);
  m_smanager  = new SettingsManager(this);

  m_ui->treeView->setModel(m_treeModel);

  m_masterDialog = new AddMasterDialog(m_spectrum, m_forwarder, this);
  m_chanDialog   = new AddChanDialog(m_spectrum, m_forwarder, this);

  m_zmqSink = new ZeroMQSink();

  assertConfig();

  setProperty("collapsed", m_panelConfig->collapsed);

  refreshUi();

  connectAll();
}

ZeroMQWidget::~ZeroMQWidget()
{
  delete m_ui;
  delete m_forwarder;
  delete m_zmqSink;
}

// LO has changed. We have two choices here:
//
// 1. Track tuner is enabled. We call adjustLo() and update all
//    opened masters accordingly.
// 2. Track tuner is disabled. This is a slightly more complicated
//    case:
//    1. When the source starts, save the current frequency.
//    2. When the frequency changes, if and only if it is running,
//       update the named channels only, so they are in the right
//       shifted frequency.
//    3. When the source is stopped OR track tuner is enabled, shift
//       shift named channels back to their corresponding position.
void
ZeroMQWidget::checkRecentering()
{
  if (m_analyzer != nullptr) {
    Suscan::AnalyzerSourceInfo info = m_analyzer->getSourceInfo();
    if (SU_ABS(m_lastTunerFrequency - info.getFrequency()) < 1)
      return;

    m_lastTunerFrequency = info.getFrequency();
  }

  if (m_forwarder->isOpen()) {
    if (m_ui->trackTunerCheck->isChecked()) {
      // TRACK TUNER
      if (!m_forwarder->canOpen()) {
        m_ui->togglePublishingButton->setChecked(false);
        checkStartStop();

        QMessageBox::warning(
              this,
              "Channels out of limits",
              "Multichannel forwarder was automatically disabled, as the current source "
              "frequency and sample rate cannot keep all channels opened at the same time. "
              "You can open the channels again by recentering the source frequency to "
              "a value close to "
              + SuWidgetsHelpers::formatQuantity(m_forwarder->getCenter(), "Hz"));
      } else {
        m_forwarder->adjustLo();
      }
    } else {
      // NO TRACK TUNER
      lagNamedChannels();
    }
  }
}

void
ZeroMQWidget::applySpectrumState()
{
  auto bandwidth  = m_spectrum->getBandwidth();
  auto loFreq     = m_spectrum->getLoFreq();
  auto centerFreq = m_spectrum->getCenterFreq();
  auto freq       = centerFreq + loFreq;

  m_masterDialog->setFrequency(freq);
  m_masterDialog->setBandwidth(bandwidth);

  m_chanDialog->setFrequency(freq);
  m_chanDialog->setBandwidth(bandwidth);

  m_ui->frequencyLabel->setText(
        SuWidgetsHelpers::formatQuantity(freq, "Hz"));
  m_ui->bandwidthLabel->setText(
        SuWidgetsHelpers::formatQuantity(bandwidth, "Hz"));
}

void
ZeroMQWidget::connectAll()
{
  connect(
        m_ui->urlEdit,
        SIGNAL(textChanged(QString)),
        this,
        SLOT(onURLChanged()));

  connect(
        m_ui->addAsMaster,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onAddMaster()));

  connect(
        m_ui->addVFOButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onAddChannel()));

  connect(
        m_ui->removeVFOButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onRemove()));

  connect(
        m_ui->togglePublishingButton,
        SIGNAL(toggled(bool)),
        this,
        SLOT(onTogglePublishing()));

  connect(
        m_masterDialog,
        SIGNAL(accepted()),
        this,
        SLOT(onAddMasterConfirm()));

  connect(
        m_chanDialog,
        SIGNAL(accepted()),
        this,
        SLOT(onAddChannelConfirm()));

  connect(
        m_spectrum,
        SIGNAL(bandwidthChanged()),
        this,
        SLOT(onSpectrumBandwidthChanged()));

  connect(
        m_spectrum,
        SIGNAL(loChanged(qint64)),
        this,
        SLOT(onSpectrumLoChanged(qint64)));

  connect(
        m_spectrum,
        SIGNAL(frequencyChanged(qint64)),
        this,
        SLOT(onSpectrumFrequencyChanged(qint64)));

  connect(
        m_ui->treeView->selectionModel(),
        SIGNAL(currentChanged(QModelIndex,QModelIndex)),
        this,
        SLOT(onChangeCurrent()));

  connect(
        m_smanager,
        SIGNAL(loadError(QString)),
        this,
        SLOT(onLoadSettingsFailed(QString)));

  connect(
        m_smanager,
        SIGNAL(createMaster(QString,double,float)),
        this,
        SLOT(onFileMakeMaster(QString,double,float)));

  connect(
        m_smanager,
        SIGNAL(createVFO(QString,double,float,QString,qint64)),
        this,
        SLOT(onFileMakeChannel(QString,double,float,QString,qint64)));

  connect(
        m_ui->openButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onOpenSettings()));

  connect(
        m_ui->saveButton,
        SIGNAL(clicked(bool)),
        this,
        SLOT(onSaveSettings()));

  connect(
        m_ui->trackTunerCheck,
        SIGNAL(toggled(bool)),
        this,
        SLOT(onToggleTrackTuner()));

  connect(
        m_treeModel,
        SIGNAL(dataChanged(QModelIndex,QModelIndex,QList<int>)),
        this,
        SLOT(onDataChanged(QModelIndex,QModelIndex,QList<int>)));
}

void
ZeroMQWidget::colorizeMaster(
    std::string const &name,
    NamedChannelSetIterator &it)
{
  // Verify state of this master:
  MasterChannel *master = m_forwarder->findMaster(name.c_str());
  NamedChannel *namCh   = it.value();
  QColor color;

  if (master != nullptr) {
    bool opened = master->handle != SUSCAN_INVALID_HANDLE_VALUE;

    if (!master->enabled) {
      color = opened ? QColor(0, 127, 0) : QColor(127, 127, 127);
    } else {
      color = opened ? QColor(0, 255, 0) : QColor(255, 255, 255);
    }
  } else {
    color = QColor(127, 127, 127);
  }

  namCh->cutOffColor = namCh->markerColor = namCh->boxColor = color;

  m_spectrum->refreshChannel(it);
}

void
ZeroMQWidget::colorizeChannel(
    std::string const &name,
    NamedChannelSetIterator &it)
{
  // Verify state of this master:
  ChannelDescription *chan = m_forwarder->findChannel(name.c_str());
  NamedChannel *namCh   = it.value();
  QColor color;

  if (chan != nullptr) {
    bool opened = chan->handle != SUSCAN_INVALID_HANDLE_VALUE;

    if (!chan->consumer->isEnabled() || !chan->parent->enabled)
      color = opened ? QColor(127, 82, 0) : QColor(100, 100, 100);
    else
      color = opened ? QColor(255, 165, 0) : QColor(200, 200, 200);
  } else {
    color = QColor(127, 127, 127);
  }

  namCh->cutOffColor = namCh->markerColor = namCh->boxColor = color;

  m_spectrum->refreshChannel(it);
}


void
ZeroMQWidget::refreshUi()
{
  bool hasMasters = !m_forwarder->empty();
  bool hasCurrent = m_ui->treeView->currentIndex().isValid();
  bool publishing = m_ui->togglePublishingButton->isChecked();
  QString style, text;

  m_ui->addVFOButton->setEnabled(hasMasters);
  m_ui->removeVFOButton->setEnabled(hasCurrent);

  if (publishing) {
    style =
          "background-color: #7f0000;\n"
          "color: white;\n"
          "font-weight: bold";
    text = "Stop publishing";
  } else {
    style =
          "background-color: #007f00;\n"
          "color: white;\n"
          "font-weight: bold";
    text = "Start publishing";
  }

  m_ui->togglePublishingButton->setText(text);
  m_ui->togglePublishingButton->setStyleSheet(style);

  for (auto p = m_masterMarkers.begin(); p != m_masterMarkers.end(); ++p)
    colorizeMaster(p->first, p->second);

  for (auto p = m_channelMarkers.begin(); p != m_channelMarkers.end(); ++p)
    colorizeChannel(p->first, p->second);
}

void
ZeroMQWidget::doRemoveMaster(MasterChannel *master)
{
  // Remove from markers
  auto iter = m_masterMarkers.find(master->name);

  if (iter != m_masterMarkers.end()) {
    m_spectrum->removeChannel(iter->second);
    m_masterMarkers.erase(master->name);
  }

  // We also need to traverse all children belonging to this master
  // and remove them accordingly
  for (auto i = master->channels.begin(); i != master->channels.end(); ++i) {
    auto c = m_channelMarkers.find(i->name);
    if (c != m_channelMarkers.end()) {
      m_spectrum->removeChannel(c->second);
      m_channelMarkers.erase(c->first);
    }
  }

  // Remove from forwarder
  m_forwarder->removeMaster(master);

  // Update view
  m_treeModel->rebuildStructure();
  m_ui->treeView->expandAll();
  refreshUi();
}

void
ZeroMQWidget::doRemoveChannel(ChannelDescription *channel)
{
  // Remove from markers
  auto iter = m_channelMarkers.find(channel->name);

  if (iter != m_channelMarkers.end()) {
    m_spectrum->removeChannel(iter->second);
    m_channelMarkers.erase(channel->name);
  }

  // Remove from forwarder
  m_forwarder->removeChannel(channel);

  // Update view
  m_treeModel->rebuildStructure();
  m_ui->treeView->expandAll();
  refreshUi();
}

void
ZeroMQWidget::checkStartStop()
{
  bool tryOpen = m_ui->togglePublishingButton->isChecked();
  bool isOpen  = m_forwarder->isPartiallyOpen();

  if (isOpen != tryOpen && m_analyzer != nullptr) {
    if (!tryOpen) {
      // Close all
      m_forwarder->closeAll();
      recenterNamedChannels();
      refreshUi();
    } else {
      if (!m_forwarder->canOpen()) {
        m_ui->togglePublishingButton->setChecked(false);
        if (m_forwarder->canCenter()) {
          auto freq = m_forwarder->getCenter();
          auto answer = QMessageBox::question(
                this,
                "ZeroMQ forwarder",
                "ZeroMQ forwarder was disabled because some of the channels fall outside of the current portion of the spectrum. "
                "Do you want to attempt to shift the current spectrum to the optimal center frequency "
                  + SuWidgetsHelpers::formatQuantity(freq, "Hz")
                  + " before trying again?",
                QMessageBox::Yes | QMessageBox::No);
          if (answer == QMessageBox::Yes) {
            try {
              m_spectrum->setFreqs(
                    static_cast<qint64>(freq),
                    static_cast<qint64>(m_analyzer->getLnbFrequency()));
            } catch (Suscan::Exception &) {
              QMessageBox::critical(
                    this,
                    "ZeroMQ forwarder",
                    "Failed to change analyzer frequency.");
            }
          }
        } else {
          QMessageBox::warning(
                this,
                "ZeroMQ forwarder",
                "ZeroMQ forwarder was disabled because the sample rate is too low "
                "to keep all channels opened at the same time. Note that the current channel "
                "configuration requires a a sample rate of at least "
                + SuWidgetsHelpers::formatQuantity(m_forwarder->span(), "sps")
                + ".");
        }
      } else {
        // Open All: use this as reference frequency
        Suscan::AnalyzerSourceInfo info = m_analyzer->getSourceInfo();
        m_lastRefFrequency = info.getFrequency();
        m_forwarder->openAll();
      }
    }
  }
}


void
ZeroMQWidget::lagNamedChannels()
{
  // Lag Named Channels: move all master channels to the frequency
  // they are due to changes in the LO frequency

  if (m_analyzer != nullptr) {
    Suscan::AnalyzerSourceInfo info = m_analyzer->getSourceInfo();
    SUFREQ diffFreq = info.getFrequency() - m_lastRefFrequency;
    for (auto i = m_forwarder->begin(); i != m_forwarder->end(); ++i) {
      auto master = *i;
      auto marker = m_masterMarkers.find(master->name);

      if (marker != m_masterMarkers.end()) {
        auto it = marker->second;
        it.value()->frequency = master->frequency + diffFreq;
        m_spectrum->refreshChannel(it);
        m_masterMarkers[master->name] = it;
      }

      for (auto j = master->channels.begin(); j != master->channels.end(); ++j) {
        auto marker = m_channelMarkers.find(j->name);
        auto channel = &*j;

        if (marker != m_channelMarkers.end()) {
          auto it = marker->second;
          it.value()->frequency = master->frequency + channel->offset + diffFreq;
          m_spectrum->refreshChannel(it);
          m_channelMarkers[channel->name] = it;
        }
      }
    }
  }
}

void
ZeroMQWidget::recenterNamedChannels()
{
  for (auto i = m_forwarder->begin(); i != m_forwarder->end(); ++i) {
    auto master = *i;
    auto marker = m_masterMarkers.find(master->name);

    if (marker != m_masterMarkers.end()) {
      auto it = marker->second;
      it.value()->frequency = master->frequency;
      m_spectrum->refreshChannel(it);
      m_masterMarkers[master->name] = it;
    }

    for (auto j = master->channels.begin(); j != master->channels.end(); ++j) {
      auto marker = m_channelMarkers.find(j->name);
      auto channel = &*j;

      if (marker != m_channelMarkers.end()) {
        auto it = marker->second;
        it.value()->frequency = master->frequency + channel->offset;
        m_spectrum->refreshChannel(it);
        m_channelMarkers[channel->name] = it;
      }
    }
  }
}

bool
ZeroMQWidget::doAddMaster(QString qName, SUFREQ frequency, SUFLOAT bandwidth, bool refresh)
{
  std::string name = qName.toStdString();
  MasterChannel *master = m_forwarder->makeMaster(
        name.c_str(),
        frequency,
        bandwidth);

  if (master == nullptr) {
    QString error = QString::fromStdString(m_forwarder->getErrors());

    QMessageBox::warning(
          this,
          "Failed to create master",
          "Master channel creation failed: " + error);

    return false;
  }

  NamedChannelSetIterator it =
      m_spectrum->addChannel(
        qName,
        frequency,
        - bandwidth / 2,
        + bandwidth / 2,
        QColor(127, 127, 127),
        QColor(127, 127, 127),
        QColor(127, 127, 127));
  it.value()->bandLike = true;
  m_masterMarkers[name] = it;
  m_spectrum->refreshChannel(it);

  if (refresh) {
    m_treeModel->rebuildStructure();
    m_ui->treeView->expandAll();
    refreshUi();
  }

  return true;
}

bool
ZeroMQWidget::doRemoveAll()
{
  if (!m_forwarder->removeAll()) {
    QMessageBox::critical(
          this,
          "Cannot clear channel list",
          "Failed to remove all entries from the current tree. Some channels are still being opened.");
    return false;
  }

  for (auto p : m_masterMarkers)
    m_spectrum->removeChannel(p.second);

  for (auto p : m_channelMarkers)
    m_spectrum->removeChannel(p.second);

  m_masterMarkers.clear();
  m_channelMarkers.clear();

  return true;
}

bool
ZeroMQWidget::doAddChannel(
    QString qName,
    SUFREQ frequency,
    SUFLOAT bandwidth,
    QString qChanType,
    qint64 sampleRate,
    bool refresh)
{
  std::string chanType = qChanType.toStdString();
  std::string inspClass = chanType == "raw" ? "raw" : "audio";
  unsigned int sampRate = static_cast<unsigned>(sampleRate);
  std::string name = qName.toStdString();

  m_forwarder->clearErrors();
  ChannelDescription *channel = m_forwarder->makeChannel(
        name.c_str(),
        frequency,
        bandwidth,
        inspClass.c_str(),
        new ZeroMQConsumer(m_zmqSink, chanType.c_str(), sampRate));

  if (channel == nullptr) {
    QString error = QString::fromStdString(m_forwarder->getErrors());

    QMessageBox::warning(
          this,
          "Failed to create channel",
          "Channel creation failed: " + error);
    return false;
  }

  NamedChannelSetIterator it =
      m_spectrum->addChannel(
        qName,
        frequency,
        - bandwidth / 2,
        + bandwidth / 2,
        QColor(127, 127, 127),
        QColor(127, 127, 127),
        QColor(127, 127, 127));
  it.value()->bandLike = false;
  it.value()->nestLevel = 1;

  m_channelMarkers[name] = it;
  m_spectrum->refreshChannel(it);

  if (refresh) {
    m_treeModel->rebuildStructure();
    m_ui->treeView->expandAll();
    refreshUi();
  }

  return true;
}


void
ZeroMQWidget::fwdAddMaster()
{
  QString qName = m_masterDialog->getName();
  SUFREQ frequency = m_masterDialog->getFrequency();
  SUFLOAT bandwidth = m_masterDialog->getBandwidth();

  doAddMaster(qName, frequency, bandwidth, true);
}

void
ZeroMQWidget::fwdAddChannel()
{
  QString qName = m_chanDialog->getName();
  QString qChanType = m_chanDialog->getDemodType();
  unsigned int sampRate = m_chanDialog->getSampleRate();
  SUFREQ frequency = m_chanDialog->getAdjustedFrequency();
  SUFLOAT bandwidth = m_chanDialog->getAdjustedBandwidth();

  doAddChannel(qName, frequency, bandwidth, qChanType, sampRate, true);
}


// Overriden methods
Suscan::Serializable *
ZeroMQWidget::allocConfig()
{
  return m_panelConfig = new ZeroMQWidgetConfig();
}

void
ZeroMQWidget::applyConfig()
{
  setProperty("collapsed", m_panelConfig->collapsed);
  m_ui->urlEdit->setText(QString::fromStdString(m_panelConfig->zmqURL));
  m_ui->togglePublishingButton->setChecked(m_panelConfig->startPublish);
  m_ui->trackTunerCheck->setChecked(m_panelConfig->trackTuner);

  refreshUi();
}

bool
ZeroMQWidget::event(QEvent *event)
{
  if (event->type() == QEvent::DynamicPropertyChange) {
    QDynamicPropertyChangeEvent *const propEvent =
        static_cast<QDynamicPropertyChangeEvent*>(event);
    QString propName = propEvent->propertyName();
    if (propName == "collapsed")
      m_panelConfig->collapsed = property("collapsed").value<bool>();
  }

  return QWidget::event(event);
}

void
ZeroMQWidget::setState(int state, Suscan::Analyzer *analyzer)
{
  if (m_analyzer == nullptr && analyzer != nullptr) {
    m_haveSourceInfo = false;

    connect(
          analyzer,
          SIGNAL(source_info_message(Suscan::SourceInfoMessage)),
          this,
          SLOT(onSourceInfoMessage(Suscan::SourceInfoMessage)));

    connect(
          analyzer,
          SIGNAL(inspector_message(Suscan::InspectorMessage)),
          this,
          SLOT(onInspectorMessage(Suscan::InspectorMessage)));

    connect(
          analyzer,
          SIGNAL(samples_message(Suscan::SamplesMessage)),
          this,
          SLOT(onSamplesMessage(Suscan::SamplesMessage)));

    refreshUi();
    applySpectrumState();
  }

  if (state != m_state)
    m_state = state;

  if (m_analyzer != analyzer) {
    m_analyzer = analyzer;
    m_lastTunerFrequency = INFINITY;

    if (m_analyzer == nullptr)
      recenterNamedChannels();

    m_forwarder->setAnalyzer(analyzer);
  }

  refreshUi();
  checkStartStop();
}

void
ZeroMQWidget::setQth(Suscan::Location const &)
{
  // TODO
}

void
ZeroMQWidget::setColorConfig(ColorConfig const &)
{
  // TODO
}

void
ZeroMQWidget::setTimeStamp(struct timeval const &)
{
  // TODO
}

void
ZeroMQWidget::setProfile(Suscan::Source::Config &profile)
{
  m_chanDialog->setNativeRate(profile.getSampleRate());
}

//////////////////////////////// Slots ////////////////////////////////////////
void
ZeroMQWidget::onSpectrumBandwidthChanged()
{
  applySpectrumState();
}

void
ZeroMQWidget::onSpectrumLoChanged(qint64)
{
  applySpectrumState();
}


void
ZeroMQWidget::onSpectrumFrequencyChanged(qint64)
{
  checkRecentering();

  applySpectrumState();
}

void
ZeroMQWidget::onSourceInfoMessage(Suscan::SourceInfoMessage const &info)
{
  if (!m_haveSourceInfo) {
   // TODO

    m_haveSourceInfo = true;
    m_chanDialog->setNativeRate(info.info()->getSampleRate());
    refreshUi();
  }

  checkRecentering();
}

void
ZeroMQWidget::onInspectorMessage(const Suscan::InspectorMessage &msg)
{
  m_forwarder->clearErrors();

  if (m_forwarder->processMessage(msg)) {
    if (m_forwarder->failed()) {
      QMessageBox::warning(
            this,
            "ZeroMQ forwarder",
            "Multi-channel forwarder disabled due to errors: "
            + QString::fromStdString(m_forwarder->getErrors()));
      m_forwarder->closeAll();
      m_ui->togglePublishingButton->setChecked(false);
      recenterNamedChannels();
    }

    refreshUi();
  }
}

void
ZeroMQWidget::onSamplesMessage(const Suscan::SamplesMessage &msg)
{
  (void) m_forwarder->feedSamplesMessage(msg);
}

void
ZeroMQWidget::onAddMaster()
{
  m_masterDialog->suggestName();
  m_masterDialog->show();
}

void
ZeroMQWidget::onAddMasterConfirm()
{
  fwdAddMaster();
}

void
ZeroMQWidget::onAddChannel()
{
  m_chanDialog->suggestName();
  m_chanDialog->show();
}

void
ZeroMQWidget::onAddChannelConfirm()
{
  fwdAddChannel();
}

void
ZeroMQWidget::onChangeCurrent()
{
  refreshUi();
}

void
ZeroMQWidget::onRemove()
{
  auto index = m_ui->treeView->currentIndex();
  auto item  = MultiChannelTreeModel::indexData(index);

  if (item != nullptr) {
    switch (item->type) {
      case MULTI_CHANNEL_TREE_ITEM_MASTER:
        doRemoveMaster(item->master);
        break;

      case MULTI_CHANNEL_TREE_ITEM_CHANNEL:
        doRemoveChannel(item->channel);
        break;

      default:
        break;
    }
  }
}

void
ZeroMQWidget::onTogglePublishing()
{
  if (m_ui->togglePublishingButton->isChecked()) {
    std::string asString = m_ui->urlEdit->text().toStdString();
    try {
      m_zmqSink->bind(asString.c_str());
    } catch (zmq::error_t &e) {
      QMessageBox::warning(
            this,
            "Cannot bind to ZeroMQ address",
            "Publishing disabled due to ZeroMQ errors: " + QString(e.what()));
      m_ui->togglePublishingButton->setChecked(false);
    }
  } else {
    m_zmqSink->disconnect();
  }

  m_panelConfig->startPublish = m_ui->togglePublishingButton->isChecked();
  refreshUi();
  checkStartStop();
}

void
ZeroMQWidget::onLoadSettingsFailed(QString error)
{
  QMessageBox::critical(
        this,
        "Cannot load settings file",
        "Failed to load settings from file: " + error);
}

void
ZeroMQWidget::onFileMakeMaster(QString masterName, SUFREQ freq, SUFLOAT bw)
{
  if (!doAddMaster(masterName, freq, bw))
    m_smanager->abortLoad();
}

void
ZeroMQWidget::onFileMakeChannel(
    QString channelName,
    SUFREQ freq,
    SUFLOAT bw,
    QString chanType,
    qint64 rate)
{
  doAddChannel(channelName, freq, bw, chanType, rate);
}

void
ZeroMQWidget::onOpenSettings()
{
  if (m_forwarder->begin() != m_forwarder->end()) {
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
          this,
          "Load channels from file",
          "The current list of channels will be cleared. Are you sure?",
          QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No);

    if (reply == QMessageBox::StandardButton::No) {
      return;
    }
  }

  QString fileName = QFileDialog::getOpenFileName(
        this,
        "Load channels form file",
        QDir().absolutePath(),
        "SDRReceiver INI settings (*.ini);;All files (*)");

  if (fileName.size() > 0) {
    std::string asStdString = fileName.toStdString();

    if (!doRemoveAll())
      return;

    if (!m_smanager->loadSettings(asStdString.c_str()))
      m_forwarder->removeAll();

    m_treeModel->rebuildStructure();
    m_ui->treeView->expandAll();
    refreshUi();
  }
}

void
ZeroMQWidget::onSaveSettings()
{
  QString fileName = QFileDialog::getSaveFileName(
        this,
        "Save channels to file",
        QDir().absolutePath(),
        "SDRReceiver INI settings (*.ini);;All files (*)");

  m_smanager->setZmqAddress(m_ui->urlEdit->text());
  m_smanager->setTunerFreq(m_spectrum->getCenterFreq());
  m_smanager->setLNBFreq(m_spectrum->getLnbFreq());

  if (fileName.size() > 0) {
    if (!m_smanager->saveSettings(fileName.toStdString().c_str(), m_forwarder)) {
      QMessageBox::critical(
            this,
            "Cannot save settings to file",
            "Failed to load channel list to file. Please verify directory permissions and try again.");

    }
  }
}

void
ZeroMQWidget::onToggleTrackTuner()
{
  if (m_ui->trackTunerCheck->isChecked()) {
    recenterNamedChannels();
  } else {
    Suscan::AnalyzerSourceInfo info = m_analyzer->getSourceInfo();
    m_lastRefFrequency = info.getFrequency();
    lagNamedChannels();
  }

  m_panelConfig->trackTuner = m_ui->trackTunerCheck->isChecked();
}

void
ZeroMQWidget::onURLChanged()
{
  m_panelConfig->zmqURL = m_ui->urlEdit->text().toStdString();
}

void
ZeroMQWidget::onDataChanged(
    const QModelIndex &topLeft,
    const QModelIndex &,
    const QList<int> &)
{
  if (topLeft.isValid()) {
    MultiChannelTreeItem *item = MultiChannelTreeModel::indexData(topLeft);

    if (item->type == MULTI_CHANNEL_TREE_ITEM_MASTER) {
      auto it = m_masterMarkers.find(item->master->name);
      if (it != m_masterMarkers.end()) {
        colorizeMaster(it->first, it->second);

        for (auto p = item->master->channels.begin();
             p != item->master->channels.end();
             ++p) {
          auto it = m_channelMarkers.find(p->name);
          if (it != m_channelMarkers.end())
            colorizeChannel(it->first, it->second);
        }
      }
    } else if (item->type == MULTI_CHANNEL_TREE_ITEM_CHANNEL) {
      auto it = m_channelMarkers.find(item->channel->name);
      if (it != m_channelMarkers.end())
        colorizeChannel(it->first, it->second);
    }
  }
}

