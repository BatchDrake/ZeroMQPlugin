//
//    ZeroMQWidget.cpp: description
//    Copyright (C) 2022 Gonzalo José Carracedo Carballal
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
#include "ZeroMQWidgetFactory.h"
#include "ZeroMQWidget.h"
#include "ui_ZeroMQWidget.h"

#include <MultiChannelForwarder.h>
#include <MultiChannelTreeModel.h>

using namespace SigDigger;

#define STRINGFY(x) #x
#define STORE(field) obj.set(STRINGFY(field), this->field)
#define LOAD(field) this->field = conf.get(STRINGFY(field), this->field)


//////////////////////////// Widget config /////////////////////////////////////
void
ZeroMQWidgetConfig::deserialize(Suscan::Object const &conf)
{
  LOAD(collapsed);
}

Suscan::Object &&
ZeroMQWidgetConfig::serialize()
{
  Suscan::Object obj(SUSCAN_OBJECT_TYPE_OBJECT);

  obj.setClass("ZeroMQWidgetConfig");

  STORE(collapsed);

  return persist(obj);
}

//////////////////////// Widget implementation /////////////////////////////////
ZeroMQWidget::ZeroMQWidget(
    ZeroMQWidgetFactory *factory,
    UIMediator *mediator,
    QWidget *parent) :
  ToolWidget(factory, mediator, parent),
  m_ui(new Ui::ZeroMQWidget)
{
  m_ui->setupUi(this);

  m_forwarder = new MultiChannelForwarder();
  m_treeModel = new MultiChannelTreeModel(m_forwarder);

  m_ui->treeView->setModel(m_treeModel);

  assertConfig();

  setProperty("collapsed", m_panelConfig->collapsed);
}

ZeroMQWidget::~ZeroMQWidget()
{
  delete m_ui;
  delete m_forwarder;
}

void
ZeroMQWidget::refreshUi()
{

}

// Overriden methods
Suscan::Serializable *
ZeroMQWidget::allocConfig()
{
  return m_panelConfig = new ZeroMQWidgetConfig();
}

void
ZeroMQWidget::applyConfig()
{
  setProperty("collapsed", m_panelConfig->collapsed);
}

bool
ZeroMQWidget::event(QEvent *event)
{
  if (event->type() == QEvent::DynamicPropertyChange) {
    QDynamicPropertyChangeEvent *const propEvent =
        static_cast<QDynamicPropertyChangeEvent*>(event);
    QString propName = propEvent->propertyName();
    if (propName == "collapsed")
      m_panelConfig->collapsed = property("collapsed").value<bool>();
  }

  return QWidget::event(event);
}

void
ZeroMQWidget::setState(int state, Suscan::Analyzer *analyzer)
{
  if (m_analyzer == nullptr && analyzer != nullptr) {
    m_haveSourceInfo = false;

    connect(
          analyzer,
          SIGNAL(source_info_message(const Suscan::SourceInfoMessage &)),
          this,
          SLOT(onSourceInfoMessage(const Suscan::SourceInfoMessage &)));

    refreshUi();
  }

  m_analyzer = analyzer;

  if (state != m_state)
    m_state = state;
}

void
ZeroMQWidget::setQth(Suscan::Location const &)
{
  // TODO
}

void
ZeroMQWidget::setColorConfig(ColorConfig const &)
{
  // TODO
}

void
ZeroMQWidget::setTimeStamp(struct timeval const &)
{
  // TODO
}

void
ZeroMQWidget::setProfile(Suscan::Source::Config &)
{
  // TODO
}

//////////////////////////////// Slots ////////////////////////////////////////
void
ZeroMQWidget::onSourceInfoMessage(Suscan::SourceInfoMessage const &)
{
  if (!m_haveSourceInfo) {
   // TODO

    m_haveSourceInfo = true;
    refreshUi();
  }
}

