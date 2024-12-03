/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_EXTENSIONS_DATAABSENTREASON_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_EXTENSIONS_DATAABSENTREASON_HXX

#include "shared/model/Extension.hxx"
#include "shared/model/ResourceNames.hxx"

namespace model
{

// NOLINTNEXTLINE(bugprone-exception-escape)
class DataAbsentReason : public Extension<DataAbsentReason>
{
public:
    static constexpr auto url = model::resource::structure_definition::dataAbsentReason;
    using Extension::Extension;
    DataAbsentReason();
    template<typename T>
    friend class Parameters;
};

extern template class Extension<DataAbsentReason>;
extern template class Resource<DataAbsentReason>;

}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_EXTENSIONS_DATAABSENTREASON_HXX
