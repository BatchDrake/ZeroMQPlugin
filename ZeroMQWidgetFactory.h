#ifndef ZEROMQWIDGETFACTORY_H
#define ZEROMQWIDGETFACTORY_H

#include <ToolWidgetFactory.h>

namespace SigDigger {
  class ZeroMQWidgetFactory : public ToolWidgetFactory
  {
  public:
    // FeatureFactory overrides
    const char *name() const override;

    // ToolWidgetFactory overrides
    ToolWidget *make(UIMediator *) override;
    std::string getTitle() const override;

    ZeroMQWidgetFactory(Suscan::Plugin *);
  };
}

#endif // ZEROMQWIDGETFACTORY_H
