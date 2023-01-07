//
//    AddChanDialog.cpp: Add channel dialog
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

#include "AddChanDialog.h"
#include "ui_AddChanDialog.h"
#include "MultiChannelForwarder.h"
#include <QPushButton>

using namespace SigDigger;

AddChanDialog::AddChanDialog(
    MainSpectrum *spectrum,
    MultiChannelForwarder *fw,
    QWidget *parent) :
  QDialog(parent),
  ui(new Ui::AddChanDialog)
{
  ui->setupUi(this);

  m_forwarder = fw;
  m_spectrum = spectrum;

  ui->sampleRateCombo->clear();
  ui->demodTypeCombo->clear();

  ui->demodTypeCombo->addItem("Raw IQ", QVariant::fromValue<QString>("raw"));
  ui->demodTypeCombo->addItem("Audio (FM)", QVariant::fromValue<QString>("audio:fm"));
  ui->demodTypeCombo->addItem("Audio (AM)", QVariant::fromValue<QString>("audio:am"));
  ui->demodTypeCombo->addItem("Audio (USB)", QVariant::fromValue<QString>("audio:usb"));
  ui->demodTypeCombo->addItem("Audio (LSB)", QVariant::fromValue<QString>("audio:lsb"));

  ui->demodTypeCombo->setCurrentIndex(3);

#define ADD_SAMP_RATE(x) m_sampleRates.push_back(x)

  ADD_SAMP_RATE(8000);
  ADD_SAMP_RATE(16000);
  ADD_SAMP_RATE(24000);
  ADD_SAMP_RATE(25000);
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
AddChanDialog::hideEvent(QHideEvent *)
{
  m_spectrum->setFilterSkewness(m_savedSkewness);
}

void
AddChanDialog::showEvent(QShowEvent *)
{
  m_savedSkewness = m_spectrum->getFilterSkewness();
  refreshUi();
}

SUFREQ
AddChanDialog::getAdjustedFrequency() const
{
  QString demod = getDemodType();
  SUFLOAT adjustedBandwidth = getAdjustedBandwidth();
  SUFREQ freq = getFrequency();

  if (demod == "audio:usb")
    freq += .5 * adjustedBandwidth;
  else if (demod == "audio:lsb")
    freq -= .5 * adjustedBandwidth;

  return freq;
}

SUFLOAT
AddChanDialog::getAdjustedBandwidth() const
{
  QString demod = getDemodType();
  SUFLOAT multiplier = 1;

  if (demod == "audio:usb" || demod == "audio:lsb")
    multiplier = .5;

  return getBandwidth() * multiplier;
}

void
AddChanDialog::populateRates()
{
  SUFLOAT bandwidth = getBandwidth();

  if (bandwidth > 0) {
    unsigned savedRate = m_lastRate;
    int index = -1;

    ui->sampleRateCombo->clear();

    for (auto rate : m_sampleRates) {
      if (rate < bandwidth) {
        if (rate <= savedRate)
          index = ui->sampleRateCombo->count();

        ui->sampleRateCombo->addItem(
              QString::number(rate),
              QVariant::fromValue<unsigned>(rate));
      }
    }

    if (index != -1)
      ui->sampleRateCombo->setCurrentIndex(index);
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
        SLOT(onBwChanged()));

  connect(
        ui->demodTypeCombo,
        SIGNAL(activated(int)),
        this,
        SLOT(onSampleRateChanged()));

  connect(
        ui->nameEdit,
        SIGNAL(textEdited(QString)),
        this,
        SLOT(onChanEdited()));
}

void
AddChanDialog::refreshUi()
{
  SUFLOAT frequency = getAdjustedFrequency();
  SUFLOAT bandwidth = getAdjustedBandwidth();
  std::string name = getName().toStdString();
  QString demod = getDemodType();
  bool okToGo = false;
  QString nameStyleSheet = "";
  MasterListConstIterator it;
  bool isAudio = getDemodType() != "raw";

  it = m_forwarder->findMaster(frequency, bandwidth);

  ui->sampleRateCombo->setEnabled(isAudio);

  if (isVisible()) {
    MainSpectrum::Skewness skewness = MainSpectrum::Skewness::SYMMETRIC;
    if (demod == "audio:usb")
      skewness = MainSpectrum::Skewness::UPPER;
    else if (demod == "audio:lsb")
      skewness = MainSpectrum::Skewness::LOWER;

    m_spectrum->setFilterSkewness(skewness);
  }

  if (it == m_forwarder->cend()) {
    ui->masterLabel->setText("Invalid (outside master limits)");
    ui->masterLabel->setStyleSheet("color: red");
  } else if (ui->sampleRateCombo->count() == 0 && isAudio) {
    ui->masterLabel->setText("Invalid (bandwidth below 8 kHz)");
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
    lastChannel = "VFO_1";
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

void
AddChanDialog::onBwChanged()
{
  populateRates();
  refreshUi();
}

void
AddChanDialog::onSampleRateChanged()
{
  int rate = getSampleRate();

  if (rate != 0)
    m_lastRate = rate;

  refreshUi();
}
