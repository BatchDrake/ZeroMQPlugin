//
//    AddMasterDialog.cpp: Add master channel dialog
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

#include "AddMasterDialog.h"
#include "ui_AddMasterDialog.h"
#include "MultiChannelForwarder.h"

#include <QRegularExpression>
#include <QPushButton>

using namespace SigDigger;

AddMasterDialog::AddMasterDialog(
    MainSpectrum *spectrum,
    MultiChannelForwarder *forwarder,
    QWidget *parent) :
  QDialog(parent),
  ui(new Ui::AddMasterDialog)
{
  m_spectrum  = spectrum;
  m_forwarder = forwarder;

  ui->setupUi(this);

  refreshUi();
  connectAll();
}

AddMasterDialog::~AddMasterDialog()
{
  delete ui;
}

void
AddMasterDialog::connectAll()
{
  connect(
        ui->nameEdit,
        SIGNAL(textEdited(QString)),
        this,
        SLOT(onNameEdited()));
}

void
AddMasterDialog::refreshUi()
{
  std::string name = getName().toStdString();
  bool okToGo = false;
  QString nameStyleSheet = "";

  if (name.size() == 0 || m_forwarder->findMaster(name.c_str()) != nullptr) {
    nameStyleSheet = "background-color: #ff7f7f; color: black";
  } else {
    okToGo = true;
  }

  ui->nameEdit->setStyleSheet(nameStyleSheet);
  ui->buttonBox->button(
        QDialogButtonBox::StandardButton::Ok)->setEnabled(okToGo);
}

void
AddMasterDialog::hideEvent(QHideEvent *)
{
  m_spectrum->setFilterSkewness(m_savedSkewness);
}

void
AddMasterDialog::showEvent(QShowEvent *)
{
  m_savedSkewness = m_spectrum->getFilterSkewness();
  m_spectrum->setFilterSkewness(MainSpectrum::Skewness::SYMMETRIC);
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

QString
AddMasterDialog::getName() const
{
  return ui->nameEdit->text();
}

void
AddMasterDialog::suggestName()
{
  std::string name = getName().toStdString();
  std::string lastMaster, suggestion;
  MasterListIterator first = m_forwarder->begin();
  MasterListIterator last  = m_forwarder->end();
  int index = 1;

  if (name.size() == 0) {
    if (first == last) {
      lastMaster = "MASTER_1";
    } else {
      MasterChannel *master = *--last;
      lastMaster = master->name;
    }
  } else {
    lastMaster = name;
  }

  QString asQString = QString::fromStdString(lastMaster);
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

  while (m_forwarder->findMaster(suggestion.c_str()) != nullptr) {
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

///////////////////////////////// Slots ////////////////////////////////////////
void
AddMasterDialog::onNameEdited()
{
  refreshUi();
}
