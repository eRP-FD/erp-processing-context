/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/tpm/Tpm.hxx"

#include "shared/util/Expect.hxx"


std::ostream& operator<< (std::ostream& out, const Tpm::PcrRegisterList& pcrSet)
{
    out << '[';
    bool first = true;
    for (const auto value : pcrSet)
    {
        if (first)
            first = false;
        else
            out << ',';
        out << value;
    }
    out << ']';
    return out;
}
