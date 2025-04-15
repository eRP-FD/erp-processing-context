/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_MODEL_MUTABLEELEMENT_HXX
#define ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_MODEL_MUTABLEELEMENT_HXX

#include "fhirtools/model/Element.hxx"

namespace fhirtools
{

class MutableElement : public Element
{
public:
    using Element::Element;

    virtual const std::string& elementName() const = 0;
    virtual void setString(std::string_view stringValue) const = 0;
    virtual void setDataAbsentExtension(const std::string_view& missingElementName) const = 0;
    virtual void removeFromParent() const = 0;
};

}// fhirtools

#endif//ERP_PROCESSING_CONTEXT_SRC_FHIRTOOLS_MODEL_MUTABLEELEMENT_HXX
