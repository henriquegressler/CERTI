#include <RTI/LogicalTimeFactory.h>
#include <memory>

namespace rti1516e
{
    std::auto_ptr<LogicalTimeFactory>
    LogicalTimeFactoryFactory::makeLogicalTimeFactory(std::wstring const& implementationName)
    {
        return HLAlogicalTimeFactoryFactory::makeLogicalTimeFactory(implementationName);
    }
}
