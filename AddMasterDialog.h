//
//    AddMasterDialog.h: Add master dialog
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

#ifndef ADDMASTERDIALOG_H
#define ADDMASTERDIALOG_H

#include <QDialog>
#include <Suscan/Analyzer.h>
#include <MainSpectrum.h>

class MultiChannelForwarder;

namespace Ui {
  class AddMasterDialog;
}

namespace SigDigger{
  class AddMasterDialog : public QDialog
  {
    Q_OBJECT

    MultiChannelForwarder *m_forwarder = nullptr;
    MainSpectrum::Skewness m_savedSkewness;
    MainSpectrum *m_spectrum;

    void connectAll();
    void refreshUi();

  public:
    explicit AddMasterDialog(
        MainSpectrum *,
        MultiChannelForwarder *,
        QWidget *parent = nullptr);

    void hideEvent(QHideEvent *) override;
    void showEvent(QShowEvent *) override;

    void setFrequency(SUFREQ);
    void setBandwidth(SUFLOAT);

    SUFREQ  getFrequency() const;
    SUFLOAT getBandwidth() const;
    QString getName() const;
    void suggestName();

    ~AddMasterDialog();

  public slots:
    void onNameEdited();

  private:
    Ui::AddMasterDialog *ui;
  };
}

#endif // ADDMASTERDIALOG_H
