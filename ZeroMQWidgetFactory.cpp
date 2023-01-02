#include "ZeroMQWidgetFactory.h"
#include "ZeroMQWidget.h"

using namespace SigDigger;

const char *
ZeroMQWidgetFactory::name() const
{
  return "ZeroMQWidget";
}

ToolWidget *
ZeroMQWidgetFactory::make(UIMediator *mediator)
{
  return new ZeroMQWidget(this, mediator);
}

std::string
ZeroMQWidgetFactory::getTitle() const
{
  return "ZeroMQ forwarding";
}

ZeroMQWidgetFactory::ZeroMQWidgetFactory(Suscan::Plugin *plugin) :
  ToolWidgetFactory(plugin)
{

}
