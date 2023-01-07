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
