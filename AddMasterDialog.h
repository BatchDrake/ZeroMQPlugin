#ifndef ADDMASTERDIALOG_H
#define ADDMASTERDIALOG_H

#include <QDialog>
#include <Suscan/Analyzer.h>

class MultiChannelForwarder;

namespace Ui {
  class AddMasterDialog;
}

class AddMasterDialog : public QDialog
{
  Q_OBJECT

  MultiChannelForwarder *m_forwarder = nullptr;

  void connectAll();

public:
  explicit AddMasterDialog(MultiChannelForwarder *, QWidget *parent = nullptr);
  void setFrequency(SUFREQ);
  void setBandwidth(SUFLOAT);

  SUFREQ  getFrequency() const;
  SUFLOAT getBandwidth() const;
  ~AddMasterDialog();

private:
  Ui::AddMasterDialog *ui;
};

#endif // ADDMASTERDIALOG_H
