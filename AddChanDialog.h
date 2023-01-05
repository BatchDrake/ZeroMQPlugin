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
  };
}

#endif // ADDCHANDIALOG_H
