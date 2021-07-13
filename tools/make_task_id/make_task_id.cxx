#include <iostream>
#include <string>

#include "erp/model/PrescriptionId.hxx"

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: " << argv[0] << " <id>" << std::endl;
        exit(EXIT_FAILURE);
    }

    auto prescriptionId = model::PrescriptionId::fromDatabaseId(
        model::PrescriptionType::apothekenpflichigeArzneimittel, std::stoll(argv[1]));

    std::cout << prescriptionId.toString() << std::endl;

    return EXIT_SUCCESS;
}
