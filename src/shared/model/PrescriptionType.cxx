/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/PrescriptionType.hxx"
#include "shared/util/Expect.hxx"

namespace model
{

bool isPkv(PrescriptionType prescriptionType)
{
    switch (prescriptionType)
    {
        case PrescriptionType::apothekenpflichigeArzneimittel:
        case PrescriptionType::direkteZuweisung:
        case PrescriptionType::digitaleGesundheitsanwendungen:
            return false;
        case PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case PrescriptionType::direkteZuweisungPkv:
            return true;
    }
    Fail2("invalid PrescriptionType " + std::to_string(static_cast<uintmax_t>(prescriptionType)), std::logic_error);
    return false;
}

bool isDiga(PrescriptionType prescriptionType)
{
    switch (prescriptionType)
    {

        case PrescriptionType::digitaleGesundheitsanwendungen:
            return true;
        case PrescriptionType::apothekenpflichigeArzneimittel:
        case PrescriptionType::direkteZuweisung:
        case PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case PrescriptionType::direkteZuweisungPkv:
            return false;
    }
    Fail2("invalid PrescriptionType " + std::to_string(static_cast<uintmax_t>(prescriptionType)), std::logic_error);
    return false;
}

bool isDirectAssignment(PrescriptionType prescriptionType)
{
    switch (prescriptionType)
    {
        case PrescriptionType::direkteZuweisung:
        case PrescriptionType::direkteZuweisungPkv:
            return true;
        case PrescriptionType::digitaleGesundheitsanwendungen:
        case PrescriptionType::apothekenpflichigeArzneimittel:
        case PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            return false;
    }
    Fail2("invalid PrescriptionType " + std::to_string(static_cast<uintmax_t>(prescriptionType)), std::logic_error);
    return false;
}


}
