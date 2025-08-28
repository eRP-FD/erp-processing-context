// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "shared/util/BdeUseCases.hxx"

#include <magic_enum/magic_enum.hpp>

namespace bde
{

std::string to_string(const UseCase& uc)
{
    using namespace std::string_literals;
    return "ERP."s.append(magic_enum::enum_name(uc.fdOperation));
}

std::string to_string(const NoUseCase&)
{
    return {};
}

}
