/*
 * (C) Copyright IBM Deutschland GmbH 2023
 * (C) Copyright IBM Corp. 2023
 */

#include "erp/model/PrescriptionType.hxx"
#include "erp/util/Expect.hxx"

namespace model
{

bool IsPkv(PrescriptionType prescriptionType)
{
    switch (prescriptionType)
    {
        case PrescriptionType::apothekenpflichigeArzneimittel:
        case PrescriptionType::direkteZuweisung:
            return false;
        case PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case PrescriptionType::direkteZuweisungPkv:
            return true;
    }
    Fail2("invalid PrescriptionType " + std::to_string(static_cast<uintmax_t>(prescriptionType)), std::logic_error);
    return false;
}

}
