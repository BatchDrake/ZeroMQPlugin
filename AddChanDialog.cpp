#include "AddChanDialog.h"
#include "ui_AddChanDialog.h"
#include "MultiChannelForwarder.h"
#include <QPushButton>

using namespace SigDigger;

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

  refreshUi();
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

  connect(
        ui->nameEdit,
        SIGNAL(textEdited(QString)),
        this,
        SLOT(onChanEdited()));
}

void
AddChanDialog::refreshUi()
{
  SUFLOAT frequency = getFrequency();
  SUFLOAT bandwidth = getBandwidth();
  std::string name = getName().toStdString();
  bool okToGo = false;
  QString nameStyleSheet = "";
  MasterListConstIterator it;

  it = m_forwarder->findMaster(frequency, bandwidth);

  if (it == m_forwarder->cend()) {
    ui->masterLabel->setText("Invalid");
    ui->masterLabel->setStyleSheet("color: red");
  } else if (
             name.size() == 0
             || m_forwarder->findChannel(name.c_str()) != nullptr) {
    nameStyleSheet = "background-color: #ff7f7f; color: black;";
  } else {
    MasterChannel *master = *it;
    ui->masterLabel->setText(QString::fromStdString(master->name));
    ui->masterLabel->setStyleSheet("");
    okToGo = true;
  }

  ui->nameEdit->setStyleSheet(nameStyleSheet);
  ui->buttonBox->button(
        QDialogButtonBox::StandardButton::Ok)->setEnabled(okToGo);
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

QString
AddChanDialog::getName() const
{
  return ui->nameEdit->text();
}

void
AddChanDialog::suggestName()
{
  std::string name = getName().toStdString();
  std::string lastChannel, suggestion;
  int index = 1;

  if (name.size() == 0) {
    lastChannel = "VFO (1)";
  } else {
    lastChannel = name;
  }

  QString asQString = QString::fromStdString(lastChannel);
  QString qSuggestion;
  QRegularExpression rx("[0-9]+");
  QRegularExpressionMatch match = rx.match(asQString);

  int lastIndex = match.lastCapturedIndex();
  int offset, size;

  if (lastIndex != -1) {
    std::string as_string;

    offset    = match.capturedStart(lastIndex);
    size      = match.capturedLength(lastIndex);

    as_string = match.captured(lastIndex).toStdString();
    if (sscanf(as_string.c_str(), "%d", &index) != 1)
      index = 1;
  }

  qSuggestion = asQString;
  suggestion = qSuggestion.toStdString();

  while (m_forwarder->findChannel(suggestion.c_str()) != nullptr) {
    ++index;

    if (lastIndex != -1) {
      qSuggestion = asQString;
      qSuggestion.replace(offset, size, QString::number(index));
    } else {
      qSuggestion = asQString + " (" + QString::number(index) + ")";
    }

    suggestion = qSuggestion.toStdString();
  }

  ui->nameEdit->setText(qSuggestion);
  refreshUi();
}

////////////////////////// Slots //////////////////////////////////////////////
void
AddChanDialog::onChanEdited()
{
  refreshUi();
}
