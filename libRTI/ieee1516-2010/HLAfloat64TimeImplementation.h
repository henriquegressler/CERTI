#ifndef RTI_HLAfloat64TimeImplementation_h
#define RTI_HLAfloat64TimeImplementation_h

namespace rti1516e
{
    class HLAfloat64TimeImpl
    {
    private:
        friend class HLAfloat64Time;
        friend class HLAfloat64Interval;
        double _value;
    };

    class HLAfloat64IntervalImpl
    {
    private:
        friend class HLAfloat64Interval;
        friend class HLAfloat64Time;
        double _value;
    };
}

#endif
