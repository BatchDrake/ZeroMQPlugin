//
//    ZeroMQWidget.cpp: description
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
}

Suscan::Object &&
ZeroMQWidgetConfig::serialize()
{
  Suscan::Object obj(SUSCAN_OBJECT_TYPE_OBJECT);

  obj.setClass("ZeroMQWidgetConfig");

  STORE(collapsed);

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

  m_ui->treeView->setModel(m_treeModel);

  m_masterDialog = new AddMasterDialog(m_forwarder, this);
  m_chanDialog   = new AddChanDialog(m_forwarder, this);

  assertConfig();

  setProperty("collapsed", m_panelConfig->collapsed);

  refreshUi();

  connectAll();
}

ZeroMQWidget::~ZeroMQWidget()
{
  delete m_ui;
  delete m_forwarder;
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
}

void
ZeroMQWidget::refreshUi()
{
  bool hasMasters = !m_forwarder->empty();
  bool hasCurrent = m_ui->treeView->currentIndex().isValid();

  m_ui->addVFOButton->setEnabled(hasMasters);
  m_ui->removeVFOButton->setEnabled(hasCurrent);

  for (auto p = m_masterMarkers.begin(); p != m_masterMarkers.end(); ++p) {
    // Verify state of this master:
    MasterChannel *master = m_forwarder->findMaster(p.key().c_str());
    NamedChannelSetIterator it = p.value();
    NamedChannel *namCh   = it.value();
    QColor color;

    if (master != nullptr) {
      bool opened = master->handle != SUSCAN_INVALID_HANDLE_VALUE;

      color = opened ? QColor(0, 255, 0) : QColor(255, 255, 255);
    } else {
      color = QColor(127, 127, 127);
    }

    namCh->cutOffColor = namCh->markerColor = namCh->boxColor = color;

    m_spectrum->refreshChannel(it);
  }

  for (auto p = m_channelMarkers.begin(); p != m_channelMarkers.end(); ++p) {
    // Verify state of this master:
    ChannelDescription *chan = m_forwarder->findChannel(p.key().c_str());
    NamedChannelSetIterator it = p.value();
    NamedChannel *namCh   = it.value();
    QColor color;

    if (chan != nullptr) {
      bool opened = chan->handle != SUSCAN_INVALID_HANDLE_VALUE;

      color = opened ? QColor(255, 165, 0) : QColor(200, 200, 200);
    } else {
      color = QColor(127, 127, 127);
    }

    namCh->cutOffColor = namCh->markerColor = namCh->boxColor = color;

    m_spectrum->refreshChannel(it);
  }
}

void
ZeroMQWidget::doRemoveMaster(MasterChannel *master)
{
  // Remove from markers
  auto iter = m_masterMarkers.find(master->name);

  if (iter != m_masterMarkers.end()) {
    m_spectrum->removeChannel(iter.value());
    m_masterMarkers.remove(master->name);
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
    m_spectrum->removeChannel(iter.value());
    m_masterMarkers.remove(channel->name);
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
  bool isOpen  = m_forwarder->isOpen();

  if (isOpen != tryOpen && m_analyzer != nullptr) {
    if (!tryOpen) {
      // Close all
      m_forwarder->closeAll();
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
        m_forwarder->openAll();
      }
    }
  }
}

void
ZeroMQWidget::fwdAddMaster()
{
  QString qName = m_masterDialog->getName();
  SUFREQ frequency = m_masterDialog->getFrequency();
  SUFLOAT bandwidth = m_masterDialog->getBandwidth();

  std::string name = qName.toStdString();
  MasterChannel *master = m_forwarder->makeMaster(
        name.c_str(),
        frequency,
        bandwidth);

  if (master != nullptr) {
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

    m_treeModel->rebuildStructure();
    m_ui->treeView->expandAll();
    refreshUi();
  } else {
    QString error = QString::fromStdString(m_forwarder->getErrors());

    QMessageBox::warning(
          this,
          "Failed to create master",
          "Master channel creation failed: " + error);
  }
}

void
ZeroMQWidget::fwdAddChannel()
{
  QString qName = m_chanDialog->getName();
  QString qChanType = m_chanDialog->getDemodType();
  unsigned int sampRate = m_chanDialog->getSampleRate();
  SUFREQ frequency = m_chanDialog->getFrequency();
  SUFLOAT bandwidth = m_chanDialog->getBandwidth();
  std::string chanType = qChanType.toStdString();
  std::string inspClass = chanType == "raw" ? "raw" : "audio";

  std::string name = qName.toStdString();

  m_forwarder->clearErrors();
  ChannelDescription *channel = m_forwarder->makeChannel(
        name.c_str(),
        frequency,
        bandwidth,
        inspClass.c_str(),
        new ZeroMQSink(chanType.c_str(), sampRate));

  if (channel != nullptr) {
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

    m_treeModel->rebuildStructure();
    m_ui->treeView->expandAll();
    refreshUi();
  } else {
    QString error = QString::fromStdString(m_forwarder->getErrors());

    QMessageBox::warning(
          this,
          "Failed to create channel",
          "Channel creation failed: " + error);
  }
}

void
ZeroMQWidget::fwdOpenChannels()
{
  m_forwarder->openAll();
}

void
ZeroMQWidget::fwdCloseChannels()
{
  m_forwarder->closeAll();;
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

  m_analyzer = analyzer;

  if (state != m_state)
    m_state = state;

  m_forwarder->setAnalyzer(analyzer);
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
ZeroMQWidget::setProfile(Suscan::Source::Config &)
{
  // TODO
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
  applySpectrumState();
}

void
ZeroMQWidget::onSourceInfoMessage(Suscan::SourceInfoMessage const &)
{
  if (!m_haveSourceInfo) {
   // TODO

    m_haveSourceInfo = true;
    refreshUi();
  }
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
            "Multi-channel forwarder error: "
            + QString::fromStdString(m_forwarder->getErrors()));
      m_forwarder->closeAll();
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
  checkStartStop();
}
