//
//    AddChanDialog.h: Add channel dialog
//    Copyright (C) 2023 Gonzalo José Carracedo Carballal
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

#ifndef ADDCHANDIALOG_H
#define ADDCHANDIALOG_H

#include <QDialog>
#include <Suscan/Analyzer.h>
#include <QVector>
#include <MainSpectrum.h>

class MultiChannelForwarder;

namespace Ui {
  class AddChanDialog;
}

namespace SigDigger {
  class AddChanDialog : public QDialog
  {
    Q_OBJECT

    unsigned int m_lastRate = 192000;
    MultiChannelForwarder *m_forwarder = nullptr;
    QVector<unsigned> m_sampleRates;
    MainSpectrum::Skewness m_savedSkewness;
    MainSpectrum *m_spectrum;

    void connectAll();
    void refreshUi();
    void populateRates();

  public:
    explicit AddChanDialog(
        MainSpectrum *spectrum,
        MultiChannelForwarder *,
        QWidget *parent = nullptr);

    void hideEvent(QHideEvent *) override;
    void showEvent(QShowEvent *) override;

    void setFrequency(SUFREQ);
    void setBandwidth(SUFLOAT);

    SUFREQ  getFrequency() const;
    SUFLOAT getBandwidth() const;

    SUFREQ getAdjustedFrequency() const;
    SUFLOAT getAdjustedBandwidth() const;

    QString getName() const;
    QString getDemodType() const;
    unsigned int getSampleRate() const;
    void suggestName();

    ~AddChanDialog();

  private:
    Ui::AddChanDialog *ui;

  public slots:
    void onBwChanged();
    void onChanEdited();
    void onSampleRateChanged();
  };
}

#endif // ADDCHANDIALOG_H
