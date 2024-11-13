#include "HLAfloat64TimeImplementation.h"
#include <RTI/time/HLAfloat64Time.h>
#include <RTI/time/HLAfloat64TimeFactory.h>
#include <RTI/time/HLAfloat64Interval.h>
#include <memory>

namespace rti1516e {

    static constexpr double INITIAL = 0.0;
    static constexpr double FINAL = std::numeric_limits<double>::max();
    static constexpr double ZERO = 0.0;
    static constexpr double EPSILON = std::numeric_limits<double>::epsilon();

    static inline void encodeDouble(double value, void* buffer, size_t bufferSize)
    {
        if (bufferSize < sizeof(double)) {
            throw CouldNotEncode(L"Not enough space");
        }
        uint64_t bytes = 0;
        std::memcpy(&bytes, &value, sizeof(double));
        unsigned char* byteBuffer = static_cast<unsigned char*>(buffer);
        byteBuffer[0] = static_cast<unsigned char>(bytes >> 56);
        byteBuffer[1] = static_cast<unsigned char>(bytes >> 48);
        byteBuffer[2] = static_cast<unsigned char>(bytes >> 40);
        byteBuffer[3] = static_cast<unsigned char>(bytes >> 32);
        byteBuffer[4] = static_cast<unsigned char>(bytes >> 24);
        byteBuffer[5] = static_cast<unsigned char>(bytes >> 16);
        byteBuffer[6] = static_cast<unsigned char>(bytes >> 8);
        byteBuffer[7] = static_cast<unsigned char>(bytes >> 0);
    }

    static inline double decodeDouble(void* buffer, size_t bufferSize)
    {
        if (bufferSize < sizeof(double)) {
            throw CouldNotEncode(L"Not enough space");
        }
        unsigned char* byteBuffer = static_cast<unsigned char*>(buffer);
        uint64_t bytes = (static_cast<uint64_t>(byteBuffer[0]) << 56) | (static_cast<uint64_t>(byteBuffer[1]) << 48)
            | (static_cast<uint64_t>(byteBuffer[2]) << 40) | (static_cast<uint64_t>(byteBuffer[3]) << 32)
            | (static_cast<uint64_t>(byteBuffer[4]) << 24) | (static_cast<uint64_t>(byteBuffer[5]) << 16)
            | (static_cast<uint64_t>(byteBuffer[6]) << 8) | (static_cast<uint64_t>(byteBuffer[7]) << 0);
        double value = 0.0;
        std::memcpy(&value, &bytes, sizeof(double));
        return value;
    }

    HLAfloat64Time::HLAfloat64Time() : _impl(new HLAfloat64TimeImpl())
    {
    }

    HLAfloat64Time::HLAfloat64Time(double const& value) : HLAfloat64Time()
    {
        setTime(value);
    }

    HLAfloat64Time::HLAfloat64Time(LogicalTime const& value) : HLAfloat64Time()
    {
        const HLAfloat64Time* other = dynamic_cast<const HLAfloat64Time*>(&value);
        if (other != nullptr) {
            setTime(other->getTime());
        }
    }

    HLAfloat64Time::HLAfloat64Time(HLAfloat64Time const& value) : HLAfloat64Time(value.getTime())
    {
    }

    HLAfloat64Time::~HLAfloat64Time()
    {
        delete _impl;
    }

    void HLAfloat64Time::setInitial()
    {
        setTime(INITIAL);
    }

    bool HLAfloat64Time::isInitial() const
    {
        return getTime() == INITIAL;
    }

    void HLAfloat64Time::setFinal()
    {
        setTime(FINAL);
    }

    bool HLAfloat64Time::isFinal() const
    {
        return getTime() == FINAL;
    }

    LogicalTime& HLAfloat64Time::operator=(LogicalTime const& value) throw(InvalidLogicalTime)
    {
        if (this != &value) {
            const HLAfloat64Time* other = dynamic_cast<const HLAfloat64Time*>(&value);
            if (other == nullptr) {
                throw InvalidLogicalTime(L"Different LogicalTime implementation");
            }
            setTime(other->getTime());
        }
        return *this;
    }

    LogicalTime& HLAfloat64Time::operator+=(LogicalTimeInterval const& addend) throw(IllegalTimeArithmetic, InvalidLogicalTimeInterval)
    {
        const HLAfloat64Interval* other = dynamic_cast<const HLAfloat64Interval*>(&addend);
        if (other == nullptr) {
            throw InvalidLogicalTime(L"Different LogicalTimeInterval implementation");
        }
        setTime(getTime() + other->getInterval());
        return *this;
    }

    LogicalTime& HLAfloat64Time::operator-=(LogicalTimeInterval const& subtrahend) throw(IllegalTimeArithmetic, InvalidLogicalTimeInterval)
    {
        const HLAfloat64Interval* other = dynamic_cast<const HLAfloat64Interval*>(&subtrahend);
        if (other == nullptr) {
            throw InvalidLogicalTime(L"Different LogicalTimeInterval implementation");
        }
        setTime(getTime() - other->getInterval());
        return *this;
    }

    bool HLAfloat64Time::operator>(LogicalTime const& value) const throw(InvalidLogicalTime)
    {
        const HLAfloat64Time* other = dynamic_cast<const HLAfloat64Time*>(&value);
        if (other == nullptr) {
            throw InvalidLogicalTime(L"Different LogicalTime implementation");
        }
        return getTime() > other->getTime();
    }

    bool HLAfloat64Time::operator<(LogicalTime const& value) const throw(InvalidLogicalTime)
    {
        return !(*this >= value);
    }

    bool HLAfloat64Time::operator==(LogicalTime const& value) const throw(InvalidLogicalTime)
    {
        const HLAfloat64Time* other = dynamic_cast<const HLAfloat64Time*>(&value);
        if (other == nullptr) {
            throw InvalidLogicalTime(L"Different LogicalTime implementation");
        }
        return getTime() == other->getTime();
    }

    bool HLAfloat64Time::operator>=(LogicalTime const& value) const throw(InvalidLogicalTime)
    {
        return !(*this < value);
    }

    bool HLAfloat64Time::operator<=(LogicalTime const& value) const throw(InvalidLogicalTime)
    {
        return !(*this > value);
    }

    VariableLengthData HLAfloat64Time::encode() const
    {
        unsigned char buffer[sizeof(double)] = {};
        encode(buffer, sizeof(double));
        return VariableLengthData(buffer, sizeof(double));
    }

    size_t HLAfloat64Time::encode(void* buffer, size_t bufferSize) const throw(CouldNotEncode)
    {
        encodeDouble(getTime(), buffer, bufferSize);
        return sizeof(double);
    }

    size_t HLAfloat64Time::encodedLength() const
    {
        return sizeof(double);
    }

    void HLAfloat64Time::decode(VariableLengthData const& VariableLengthData) throw(InternalError, CouldNotDecode)
    {
        decode(const_cast<void*>(VariableLengthData.data()), VariableLengthData.size());
    }

    void HLAfloat64Time::decode(void* buffer, size_t bufferSize) throw(InternalError, CouldNotDecode)
    {
        setTime(decodeDouble(buffer, bufferSize));
    }

    std::wstring HLAfloat64Time::toString() const
    {
        return std::to_wstring(getTime());
    }

    std::wstring HLAfloat64Time::implementationName() const
    {
        return HLAfloat64TimeName;
    }

    double HLAfloat64Time::getTime() const
    {
        return _impl->_value;
    }

    void HLAfloat64Time::setTime(double value)
    {
        _impl->_value = value;
    }

    HLAfloat64Time& HLAfloat64Time::operator=(const HLAfloat64Time& value) throw(rti1516e::InvalidLogicalTimeInterval)
    {
        if (this != &value) {
            setTime(value.getTime());
        }
        return *this;
    }

    HLAfloat64Time::operator double() const
    {
        return getTime();
    }

    HLAfloat64Interval::HLAfloat64Interval() : _impl(new HLAfloat64IntervalImpl())
    {
    }

    HLAfloat64Interval::HLAfloat64Interval(double value) : HLAfloat64Interval()
    {
        setInterval(value);
    }

    HLAfloat64Interval::HLAfloat64Interval(LogicalTimeInterval const& value) : HLAfloat64Interval()
    {
        const HLAfloat64Interval* other = dynamic_cast<const HLAfloat64Interval*>(&value);
        if (other != nullptr) {
            setInterval(other->getInterval());
        }
    }

    HLAfloat64Interval::HLAfloat64Interval(HLAfloat64Interval const& value) : HLAfloat64Interval(value.getInterval())
    {
    }

    HLAfloat64Interval::~HLAfloat64Interval()
    {
        delete _impl;
    }

    void HLAfloat64Interval::setZero()
    {
        setInterval(ZERO);
    }

    bool HLAfloat64Interval::isZero() const
    {
        return getInterval() == ZERO;
    }

    void HLAfloat64Interval::setEpsilon()
    {
        setInterval(EPSILON);
    }

    bool HLAfloat64Interval::isEpsilon() const
    {
        return getInterval() == EPSILON;
    }

    LogicalTimeInterval& HLAfloat64Interval::operator=(LogicalTimeInterval const& value) throw(InvalidLogicalTime)
    {
        if (this != &value) {
            const HLAfloat64Interval* other = dynamic_cast<const HLAfloat64Interval*>(&value);
            if (other == nullptr) {
                throw InvalidLogicalTime(L"Different LogicalTimeInterval implementation");
            }
            setInterval(other->getInterval());
        }
        return *this;
    }

    LogicalTimeInterval& HLAfloat64Interval::operator+=(LogicalTimeInterval const& addend) throw(IllegalTimeArithmetic, InvalidLogicalTimeInterval)
    {
        const HLAfloat64Interval* other = dynamic_cast<const HLAfloat64Interval*>(&addend);
        if (other == nullptr) {
            throw InvalidLogicalTime(L"Different LogicalTimeInterval implementation");
        }
        setInterval(getInterval() + other->getInterval());
        return *this;
    }

    LogicalTimeInterval& HLAfloat64Interval::operator-=(LogicalTimeInterval const& subtrahend) throw(IllegalTimeArithmetic, InvalidLogicalTimeInterval)
    {
        const HLAfloat64Interval* other = dynamic_cast<const HLAfloat64Interval*>(&subtrahend);
        if (other == nullptr) {
            throw InvalidLogicalTime(L"Different LogicalTimeInterval implementation");
        }
        setInterval(getInterval() - other->getInterval());
        return *this;
    }

    bool HLAfloat64Interval::operator>(LogicalTimeInterval const& value) const throw(InvalidLogicalTime)
    {
        const HLAfloat64Interval* other = dynamic_cast<const HLAfloat64Interval*>(&value);
        if (other == nullptr) {
            throw InvalidLogicalTime(L"Different LogicalTime implementation");
        }
        return getInterval() > other->getInterval();
    }

    bool HLAfloat64Interval::operator<(LogicalTimeInterval const& value) const throw(InvalidLogicalTime)
    {
        return !(*this >= value);
    }

    bool HLAfloat64Interval::operator==(LogicalTimeInterval const& value) const throw(InvalidLogicalTime)
    {
        const HLAfloat64Interval* other = dynamic_cast<const HLAfloat64Interval*>(&value);
        if (other == nullptr) {
            throw InvalidLogicalTime(L"Different LogicalTime implementation");
        }
        return getInterval() == other->getInterval();
    }

    bool HLAfloat64Interval::operator>=(LogicalTimeInterval const& value) const throw(InvalidLogicalTime)
    {
        return !(*this < value);
    }

    bool HLAfloat64Interval::operator<=(LogicalTimeInterval const& value) const throw(InvalidLogicalTime)
    {
        return !(*this > value);
    }

    void HLAfloat64Interval::setToDifference(rti1516e::LogicalTime const& minuend, rti1516e::LogicalTime const& subtrahend) throw(rti1516e::IllegalTimeArithmetic, rti1516e::InvalidLogicalTime)
    {
        const HLAfloat64Time* lhs = dynamic_cast<const HLAfloat64Time*>(&minuend);
        if (lhs == nullptr) {
            throw InvalidLogicalTime(L"Different LogicalTime implementation");
        }
        const HLAfloat64Time* rhs = dynamic_cast<const HLAfloat64Time*>(&subtrahend);
        if (rhs == nullptr) {
            throw InvalidLogicalTime(L"Different LogicalTime implementation");
        }
        setInterval(lhs->getTime() - rhs->getTime());
    }

    VariableLengthData HLAfloat64Interval::encode() const
    {
        unsigned char buffer[sizeof(double)] = {};
        encode(buffer, sizeof(double));
        return VariableLengthData(buffer, sizeof(double));
    }

    size_t HLAfloat64Interval::encode(void* buffer, size_t bufferSize) const throw(CouldNotEncode)
    {
        encodeDouble(getInterval(), buffer, bufferSize);
        return sizeof(double);
    }

    size_t HLAfloat64Interval::encodedLength() const
    {
        return sizeof(double);
    }

    void HLAfloat64Interval::decode(VariableLengthData const& VariableLengthData) throw(InternalError, CouldNotDecode)
    {
        decode(const_cast<void*>(VariableLengthData.data()), VariableLengthData.size());
    }

    void HLAfloat64Interval::decode(void* buffer, size_t bufferSize) throw(InternalError, CouldNotDecode)
    {
        setInterval(decodeDouble(buffer, bufferSize));
    }

    std::wstring HLAfloat64Interval::toString() const
    {
        return std::to_wstring(getInterval());
    }

    std::wstring HLAfloat64Interval::implementationName() const
    {
        return HLAfloat64TimeName;
    }

    double HLAfloat64Interval::getInterval() const
    {
        return _impl->_value;
    }

    void HLAfloat64Interval::setInterval(double value)
    {
        _impl->_value = value;
    }

    HLAfloat64Interval& HLAfloat64Interval::operator=(const HLAfloat64Interval& value) throw(rti1516e::InvalidLogicalTimeInterval)
    {
        if (this != &value) {
            setInterval(value.getInterval());
        }
        return *this;
    }

    HLAfloat64Interval::operator double() const
    {
        return getInterval();
    }

    HLAfloat64TimeFactory::HLAfloat64TimeFactory()
    {
    }

    HLAfloat64TimeFactory::~HLAfloat64TimeFactory() throw()
    {
    }

    std::auto_ptr<HLAfloat64Time> HLAfloat64TimeFactory::makeLogicalTime(double value) throw(InternalError)
    {
        return std::auto_ptr<HLAfloat64Time>(new HLAfloat64Time(value));
    }

    std::auto_ptr<LogicalTime> HLAfloat64TimeFactory::makeInitial() throw(InternalError)
    {
        return std::auto_ptr<HLAfloat64Time>(new HLAfloat64Time(INITIAL));
    }

    std::auto_ptr<LogicalTime> HLAfloat64TimeFactory::makeFinal() throw(InternalError)
    {
        return std::auto_ptr<HLAfloat64Time>(new HLAfloat64Time(FINAL));
    }

    std::auto_ptr<HLAfloat64Interval>
    HLAfloat64TimeFactory::makeLogicalTimeInterval(double value) throw(InternalError)
    {
        return std::auto_ptr<HLAfloat64Interval>(new HLAfloat64Interval(value));
    }

    std::auto_ptr<LogicalTimeInterval> HLAfloat64TimeFactory::makeZero() throw(InternalError)
    {
        return std::auto_ptr<HLAfloat64Interval>(new HLAfloat64Interval(ZERO));
    }

    std::auto_ptr<LogicalTimeInterval> HLAfloat64TimeFactory::makeEpsilon() throw(InternalError)
    {
        return std::auto_ptr<HLAfloat64Interval>(new HLAfloat64Interval(EPSILON));
    }

    std::auto_ptr<LogicalTime>
    HLAfloat64TimeFactory::decodeLogicalTime(VariableLengthData const& encodedLogicalTime) throw(InternalError, CouldNotDecode)
    {
        std::auto_ptr<HLAfloat64Time> time(new HLAfloat64Time());
        time->decode(encodedLogicalTime);
        return time;
    }

    std::auto_ptr<LogicalTime>
    HLAfloat64TimeFactory::decodeLogicalTime(void* buffer, size_t bufferSize) throw(InternalError, CouldNotDecode)
    {
        std::auto_ptr<HLAfloat64Time> time(new HLAfloat64Time());
        time->decode(buffer, bufferSize);
        return time;
    }

    std::auto_ptr<LogicalTimeInterval>
    HLAfloat64TimeFactory::decodeLogicalTimeInterval(VariableLengthData const& encodedValue) throw(InternalError, CouldNotDecode)
    {
        std::auto_ptr<HLAfloat64Interval> time(new HLAfloat64Interval());
        time->decode(encodedValue);
        return time;
    }

    std::auto_ptr<LogicalTimeInterval>
    HLAfloat64TimeFactory::decodeLogicalTimeInterval(void* buffer, size_t bufferSize) throw(InternalError, CouldNotDecode)
    {
        std::auto_ptr<HLAfloat64Interval> time(new HLAfloat64Interval());
        time->decode(buffer, bufferSize);
        return time;
    }

    std::wstring HLAfloat64TimeFactory::getName() const
    {
        return HLAfloat64TimeName;
    };

}
