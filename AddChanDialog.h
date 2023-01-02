#ifndef ADDCHANDIALOG_H
#define ADDCHANDIALOG_H

#include <QDialog>
#include <Suscan/Analyzer.h>
#include <QVector>

class MultiChannelForwarder;

namespace Ui {
  class AddChanDialog;
}

class AddChanDialog : public QDialog
{
  Q_OBJECT

  MultiChannelForwarder *m_forwarder = nullptr;
  QVector<unsigned> m_sampleRates;

  void connectAll();
  void refreshUi();
  void populateRates();

public:
  explicit AddChanDialog(MultiChannelForwarder *, QWidget *parent = nullptr);
  void setFrequency(SUFREQ);
  void setBandwidth(SUFLOAT);

  SUFREQ  getFrequency() const;
  SUFLOAT getBandwidth() const;
  QString getDemodType() const;
  unsigned int getSampleRate() const;

  ~AddChanDialog();

private:
  Ui::AddChanDialog *ui;

public slots:
  void onChanEdited();
};

#endif // ADDCHANDIALOG_H
