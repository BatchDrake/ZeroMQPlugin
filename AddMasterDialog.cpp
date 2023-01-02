#include "AddMasterDialog.h"
#include "ui_AddMasterDialog.h"

AddMasterDialog::AddMasterDialog(
    MultiChannelForwarder *forwarder,
    QWidget *parent) :
  QDialog(parent),
  ui(new Ui::AddMasterDialog)
{
  m_forwarder = forwarder;

  ui->setupUi(this);
}

AddMasterDialog::~AddMasterDialog()
{
  delete ui;
}

void
AddMasterDialog::connectAll()
{
  connect(
        ui->buttonBox,
        SIGNAL(accepted()),
        this,
        SIGNAL(accepted()));

  connect(
        ui->buttonBox,
        SIGNAL(rejected()),
        this,
        SIGNAL(rejected()));
}

void
AddMasterDialog::setFrequency(SUFREQ freq)
{
  ui->frequencySpinBox->setValue(freq);
}

void
AddMasterDialog::setBandwidth(SUFLOAT bw)
{
  ui->bandwidthSpinBox->setValue(bw);
}

SUFREQ
AddMasterDialog::getFrequency() const
{
  return ui->frequencySpinBox->value();
}

SUFLOAT
AddMasterDialog::getBandwidth() const
{
  return ui->bandwidthSpinBox->value();
}

