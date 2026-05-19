/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/PrescriptionType.hxx"
#include "shared/util/Expect.hxx"

namespace model
{

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
        case PrescriptionType::tRezept:
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
        case PrescriptionType::tRezept:
            return false;
    }
    Fail2("invalid PrescriptionType " + std::to_string(static_cast<uintmax_t>(prescriptionType)), std::logic_error);
    return false;
}

bool canBeGkv(PrescriptionType prescriptionType)
{
    switch (prescriptionType)
    {
        case PrescriptionType::apothekenpflichigeArzneimittel:
        case PrescriptionType::digitaleGesundheitsanwendungen:
        case PrescriptionType::direkteZuweisung:
        case PrescriptionType::tRezept:
            return true;
        case PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case PrescriptionType::direkteZuweisungPkv:
            break;
    }
    return false;
}

bool canBePkv(PrescriptionType prescriptionType)
{
    switch (prescriptionType)
    {
        case PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case PrescriptionType::direkteZuweisungPkv:
        case PrescriptionType::tRezept:
            return true;
        case PrescriptionType::digitaleGesundheitsanwendungen:
        case PrescriptionType::apothekenpflichigeArzneimittel:
        case PrescriptionType::direkteZuweisung:
            break;
    }
    return false;
}

bool isTRezept(PrescriptionType prescriptionType)
{
    switch (prescriptionType)
    {
        case PrescriptionType::tRezept:
            return true;
        case PrescriptionType::apothekenpflichigeArzneimittel:
        case PrescriptionType::digitaleGesundheitsanwendungen:
        case PrescriptionType::direkteZuweisung:
        case PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case PrescriptionType::direkteZuweisungPkv:
            break;
    }
    return false;
}

bool isEgkRedeemable(PrescriptionType prescriptionType)
{
    switch (prescriptionType)
    {
        case PrescriptionType::apothekenpflichigeArzneimittel:
        case PrescriptionType::tRezept:
            return true;
        case PrescriptionType::digitaleGesundheitsanwendungen:
        case PrescriptionType::direkteZuweisung:
        case PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case PrescriptionType::direkteZuweisungPkv:
            break;
    }
    return false;
}

}
