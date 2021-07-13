#include "erp/tpm/Tpm.hxx"

#include "erp/util/Expect.hxx"


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
