/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2026
 *  (C) Copyright IBM Corp. 2021, 2026
 *  non-exclusively licensed to gematik GmbH
 */


#include "shared/util/BinaryView.hxx"
#include "shared/util/BinaryBuffer.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "shared/util/Expect.hxx"

#include <limits>


BinaryView::BinaryView(const BinaryBuffer& buffer)
  : BinaryView(buffer.data(), buffer.size())
{
}


BinaryView::BinaryView(const void* data, size_type size)
  : base_type(static_cast<const std::uint8_t*>(data), size)
{
    Expect3(data != nullptr || size == 0, "Tried to create a nullptr BinaryView with a non-zero size.",
            std::out_of_range);

    Expect3(size < std::numeric_limits<std::ptrdiff_t>::max(),
            "BinaryView tried to reference half of the available address space or more. "
            "This is most likely caused by passing a negative size.",
            std::out_of_range);
}


BinaryView::BinaryView(const std::string_view& str)
  : BinaryView(str.data(), str.size())
{
}


int BinaryView::sizeAsInt() const
{
    return gsl::narrow<int>(size());
}


long BinaryView::sizeAsLong() const
{
    return gsl::narrow<long>(size());
}
