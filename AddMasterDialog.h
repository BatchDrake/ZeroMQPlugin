#ifndef ADDMASTERDIALOG_H
#define ADDMASTERDIALOG_H

#include <QDialog>
#include <Suscan/Analyzer.h>

class MultiChannelForwarder;

namespace Ui {
  class AddMasterDialog;
}

namespace SigDigger{
  class AddMasterDialog : public QDialog
  {
    Q_OBJECT

    MultiChannelForwarder *m_forwarder = nullptr;

    void connectAll();
    void refreshUi();

  public:
    explicit AddMasterDialog(MultiChannelForwarder *, QWidget *parent = nullptr);
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
