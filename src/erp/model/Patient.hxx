#ifndef ERP_PROCESSING_CONTEXT_PATIENT_HXX
#define ERP_PROCESSING_CONTEXT_PATIENT_HXX

#include "erp/model/Resource.hxx"

namespace model
{

class Patient : public Resource<Patient>
{
public:
    std::string getKvnr() const;

private:
    friend Resource<Patient>;
    explicit Patient (NumberAsStringParserDocument&& document);

};

}

#endif //ERP_PROCESSING_CONTEXT_PATIENT_HXX
