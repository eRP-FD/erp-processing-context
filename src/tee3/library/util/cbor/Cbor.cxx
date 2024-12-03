/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/cbor/Cbor.hxx"
#include "library/stream/ReadableByteStream.hxx"

namespace epa
{

uint8_t Cbor::makeItemHeadByte(const MajorType type, const Info info)
{
    return static_cast<uint8_t>(
        (static_cast<uint8_t>(type) << CborMajorTypeShift) | static_cast<uint8_t>(info));
}


Cbor::MajorType Cbor::getMajorType(const uint8_t value)
{
    return static_cast<MajorType>((value & CborMajorTypeMask) >> CborMajorTypeShift);
}


Cbor::Type Cbor::getType(const uint8_t value)
{
    switch (getMajorType(value))
    {
        case MajorType::UnsignedInteger:
            return Type::UnsignedInteger;
        case MajorType::NegativeInteger:
            return Type::NegativeInteger;
        case MajorType::ByteString:
            return Type::ByteString;
        case MajorType::TextString:
            return Type::TextString;
        case MajorType::Array:
            return Type::Array;
        case MajorType::Map:
            return Type::Map;
        case MajorType::Tag:
            return Type::Tag;
        case MajorType::EOS:
            return Type::EOS;

        case MajorType::BreakSimpleFloat:
            switch (getSimple(value))
            {
                case Simple::True:
                    return Type::True;
                case Simple::False:
                    return Type::False;
                case Simple::Null:
                    return Type::Null;
                case Simple::HalfPrecisionFloat:
                    return Type::HalfFloat;
                case Simple::SinglePrecisionFLoat:
                    return Type::Float;
                case Simple::DoublePrecisionFloat:
                    return Type::Double;
                case Simple::Break:
                    return Type::Break;
                default:
                    break;
            }
            break;
    }
    Failure() << "unsupported CBOR type " << std::hex << static_cast<uintmax_t>(value);
}


Cbor::Info Cbor::getInfo(const uint8_t value)
{
    return static_cast<Cbor::Info>(value & CborInfoMask);
}


Cbor::Simple Cbor::getSimple(const uint8_t value)
{
    return static_cast<Cbor::Simple>(value & CborInfoMask);
}


std::string Cbor::toString(const Cbor::Info info)
{
#define Case(type)                                                                                 \
    case Cbor::Info::type:                                                                         \
        return #type
    switch (info)
    {
        Case(SmallestDirectValue); // 0
        Case(LargestDirectValue);  // 23
        Case(OneByteArgument);
        Case(TwoByteArgument);
        Case(FourByteArgument);
        Case(EightByteArgument);
        Case(Indefinite);
        default:
            return std::to_string(static_cast<uint8_t>(info));
    }
    Failure() << "invalid Cbor::Info value";
#undef Case
}
} // namespace epa
