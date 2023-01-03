//
//    ZeroMQWidget.h: description
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
#ifndef ZEROMQWIDGET_H
#define ZEROMQWIDGET_H

#include <ToolWidgetFactory.h>
#include <QWidget>
#include <QHash>
#include <MainSpectrum.h>

namespace Ui {
  class ZeroMQWidget;
}

class MultiChannelForwarder;
class MultiChannelTreeModel;

class MasterChannel;
class ChannelDescription;

namespace SigDigger {
  class AddChanDialog;
  class AddMasterDialog;
  class AudioProcessor;
  class ZeroMQWidgetFactory;
  class FrequencyCorrectionDialog;
  class MainSpectrum;

  class ZeroMQWidgetConfig : public Suscan::Serializable {
  public:
    bool collapsed      = false;

    // Overriden methods
    void deserialize(Suscan::Object const &conf) override;
    Suscan::Object &&serialize() override;
  };

  class ZeroMQWidget : public ToolWidget
  {
    Q_OBJECT

    ZeroMQWidgetConfig *m_panelConfig = nullptr;

    // Processing members
    Suscan::Analyzer *m_analyzer = nullptr; // Borrowed
    bool m_haveSourceInfo = false;
    MultiChannelForwarder *m_forwarder = nullptr;
    MultiChannelTreeModel *m_treeModel = nullptr;

    // UI members
    int m_state = 0;
    MainSpectrum *m_spectrum = nullptr;
    Ui::ZeroMQWidget *m_ui = nullptr;
    AddChanDialog *m_chanDialog = nullptr;
    AddMasterDialog *m_masterDialog = nullptr;

    // Master channel markers
    QHash<std::string, NamedChannelSetIterator> m_masterMarkers;
    QHash<std::string, NamedChannelSetIterator> m_channelMarkers;

    void refreshUi();

    void doRemoveMaster(MasterChannel *);
    void doRemoveChannel(ChannelDescription *);

    // High-level logic
    void fwdAddMaster();
    void fwdAddChannel();

    void fwdOpenChannels();
    void fwdCloseChannels();

    void applySpectrumState();
    void connectAll();

  public:
    ZeroMQWidget(ZeroMQWidgetFactory *, UIMediator *, QWidget *parent = nullptr);
    ~ZeroMQWidget() override;

    // Configuration methods
    Suscan::Serializable *allocConfig() override;
    void applyConfig() override;
    bool event(QEvent *) override;

    // Overriden methods
    void setState(int, Suscan::Analyzer *) override;
    void setQth(Suscan::Location const &) override;
    void setColorConfig(ColorConfig const &) override;
    void setTimeStamp(struct timeval const &) override;
    void setProfile(Suscan::Source::Config &) override;

  public slots:
    void onSpectrumBandwidthChanged();
    void onSpectrumLoChanged(qint64);
    void onSpectrumFrequencyChanged(qint64 freq);

    void onSourceInfoMessage(Suscan::SourceInfoMessage const &);
    void onInspectorMessage(Suscan::InspectorMessage const &);
    void onSamplesMessage(Suscan::SamplesMessage const &);

    void onAddMaster();
    void onAddMasterConfirm();

    void onAddChannel();
    void onAddChannelConfirm();

    void onChangeCurrent();
    void onRemove();
  };
}

#endif // ZEROMQWIDGET_H
