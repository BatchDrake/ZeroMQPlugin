#include "AddChanDialog.h"
#include "ui_AddChanDialog.h"
#include "MultiChannelForwarder.h"

AddChanDialog::AddChanDialog(MultiChannelForwarder *fw, QWidget *parent) :
  QDialog(parent),
  ui(new Ui::AddChanDialog)
{
  ui->setupUi(this);

  m_forwarder = fw;

  ui->sampleRateCombo->clear();
  ui->demodTypeCombo->clear();

  ui->demodTypeCombo->addItem("Raw IQ", QVariant::fromValue<QString>("raw"));
  ui->demodTypeCombo->addItem("Audio (FM)", QVariant::fromValue<QString>("audio:fm"));
  ui->demodTypeCombo->addItem("Audio (AM)", QVariant::fromValue<QString>("audio:am"));
  ui->demodTypeCombo->addItem("Audio (USB)", QVariant::fromValue<QString>("audio:usb"));
  ui->demodTypeCombo->addItem("Audio (LSB)", QVariant::fromValue<QString>("audio:lsb"));

#define ADD_SAMP_RATE(x) m_sampleRates.push_back(x)

  ADD_SAMP_RATE(8000);
  ADD_SAMP_RATE(16000);
  ADD_SAMP_RATE(32000);
  ADD_SAMP_RATE(44100);
  ADD_SAMP_RATE(48000);
  ADD_SAMP_RATE(96000);
  ADD_SAMP_RATE(192000);

#undef ADD_SAMP_RATE

  connectAll();
}

AddChanDialog::~AddChanDialog()
{
  delete ui;
}

void
AddChanDialog::populateRates()
{
  SUFLOAT bandwidth = getBandwidth();

  if (bandwidth > 0) {
    ui->sampleRateCombo->clear();

    for (auto rate : m_sampleRates) {
      if (rate < bandwidth) {
        ui->sampleRateCombo->addItem(
              QString::number(rate),
              QVariant::fromValue<unsigned>(rate));
      }
    }
  }
}

void
AddChanDialog::connectAll()
{
  connect(
        ui->frequencySpinBox,
        SIGNAL(valueChanged(double)),
        this,
        SLOT(onChanEdited()));

  connect(
        ui->bandwidthSpinBox,
        SIGNAL(valueChanged(double)),
        this,
        SLOT(onChanEdited()));

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
AddChanDialog::refreshUi()
{
  SUFLOAT frequency = getFrequency();
  SUFLOAT bandwidth = getBandwidth();
  MasterListConstIterator it;

  it = m_forwarder->findMaster(frequency, bandwidth);

  if (it == m_forwarder->cend()) {
    ui->masterLabel->setText("Invalid");
    ui->masterLabel->setStyleSheet("color: red");
  } else {
    MasterChannel *master = *it;
    ui->masterLabel->setText(QString::fromStdString(master->name));
    ui->masterLabel->setStyleSheet("");
  }
}

void
AddChanDialog::setFrequency(SUFREQ freq)
{
  ui->frequencySpinBox->setValue(freq);

  refreshUi();
}

void
AddChanDialog::setBandwidth(SUFLOAT bw)
{
  ui->bandwidthSpinBox->setValue(bw);

  populateRates();
  refreshUi();
}

SUFREQ
AddChanDialog::getFrequency() const
{
  return ui->frequencySpinBox->value();
}

SUFLOAT
AddChanDialog::getBandwidth() const
{
  return ui->bandwidthSpinBox->value();
}

QString
AddChanDialog::getDemodType() const
{
  if (ui->demodTypeCombo->currentIndex() == -1)
    return QString();

  return ui->demodTypeCombo->currentData().value<QString>();
}

unsigned
AddChanDialog::getSampleRate() const
{
  if (ui->sampleRateCombo->currentIndex() == -1)
    return 0;

  return ui->sampleRateCombo->currentData().value<unsigned>();
}
