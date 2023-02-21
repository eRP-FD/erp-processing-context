/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_INCODEVALIDATOR_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_INCODEVALIDATOR_HXX

#include "erp/model/ResourceVersion.hxx"
#include "erp/validation/SchemaType.hxx"

#include <map>
#include <memory>
#include <set>

namespace model
{
class ResourceBase;
}
class XmlValidator;
class InCodeValidator;

class ResourceValidator
{
public:
    virtual ~ResourceValidator() = default;
    virtual void validate(const model::ResourceBase& resource, const XmlValidator& xmlValidator,
                          const InCodeValidator& inCodeValidator) const = 0;
};

class InCodeValidator
{
public:
    InCodeValidator();
    void validate(const model::ResourceBase& resource, SchemaType schemaType, model::ResourceVersion::KbvItaErp version,
                  const XmlValidator& xmlValidator) const;
    void validate(const model::ResourceBase& resource, SchemaType schemaType,
                  model::ResourceVersion::DeGematikErezeptWorkflowR4 version, const XmlValidator& xmlValidator) const;
    void validate(const model::ResourceBase& resource, SchemaType schemaType,
                  model::ResourceVersion::NotProfiled version, const XmlValidator& xmlValidator) const;
    void validate(const model::ResourceBase& resource, SchemaType schemaType,
                  model::ResourceVersion::Fhir version, const XmlValidator& xmlValidator) const;
    void validate(const model::ResourceBase& resource, SchemaType schemaType,
                  model::ResourceVersion::DeGematikErezeptPatientenrechnungR4 version, const XmlValidator& xmlValidator) const;
    void validate(const model::ResourceBase& resource, SchemaType schemaType,
                  model::ResourceVersion::WorkflowOrPatientenRechnungProfile version, const XmlValidator& xmlValidator) const;
    void validate(const model::ResourceBase& resource, SchemaType schemaType,
                  model::ResourceVersion::AbgabedatenPkv version, const XmlValidator& xmlValidator) const;

private:
    std::set<SchemaType> mMandatoryValidation;

    using KeyGem = std::pair<SchemaType, model::ResourceVersion::DeGematikErezeptWorkflowR4>;
    std::map<KeyGem, std::unique_ptr<ResourceValidator>> mGematikValidators;

    using KeyKbv = std::pair<SchemaType, model::ResourceVersion::KbvItaErp>;
    std::map<KeyKbv, std::unique_ptr<ResourceValidator>> mKbvValidators;
};


#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_VALIDATION_INCODEVALIDATOR_HXX
