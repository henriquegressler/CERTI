#include <RTI/LogicalTime.h>
#include <RTI/LogicalTimeFactory.h>
#include <RTI/LogicalTimeInterval.h>
#include <RTI/time/HLAfloat64TimeFactory.h>
#include <memory>

namespace rti1516e
{
    rti1516e::LogicalTime::~LogicalTime()
    {
    }

    rti1516e::LogicalTimeFactory::~LogicalTimeFactory()
    {
    }

    rti1516e::LogicalTimeInterval::~LogicalTimeInterval()
    {
    }

    std::auto_ptr<LogicalTimeFactory>
    HLAlogicalTimeFactoryFactory::makeLogicalTimeFactory(std::wstring const& implementationName)
    {
        if (implementationName.empty() || implementationName == HLAfloat64TimeName)
            return std::auto_ptr<LogicalTimeFactory>(new HLAfloat64TimeFactory());
        return std::auto_ptr<LogicalTimeFactory>();
    }
}
