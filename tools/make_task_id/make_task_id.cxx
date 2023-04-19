/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include <iostream>
#include <string>

#include "erp/model/PrescriptionId.hxx"
#include "fhirtools/util/Gsl.hxx"

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "usage: " << argv[0] << " 160|169 <id>" << std::endl;
        return EXIT_FAILURE;
    }

    auto prescriptionId = model::PrescriptionId::fromDatabaseId(
        magic_enum::enum_cast<model::PrescriptionType>(gsl::narrow<uint8_t>(std::stoll(argv[1]))).value(),
        std::stoll(argv[2]));

    std::cout << prescriptionId.toString() << std::endl;

    return EXIT_SUCCESS;
}
