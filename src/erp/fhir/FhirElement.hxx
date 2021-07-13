#ifndef ERP_PROCESSING_CONTEXT_FHIRELEMENT_HXX
#define ERP_PROCESSING_CONTEXT_FHIRELEMENT_HXX

#include <iosfwd>
#include <memory>
#include <string>

/// @brief stores information of an Element in the StructureDefinition
///
/// XPath: StructureDefinition/snapshot/element
class FhirElement
{
public:
    class Builder;
    enum class Representation
    {
        xmlAttr,
        xmlText,
        typeAttr,
        cdaText,
        xhtml,
        element,
    };

    FhirElement();
    virtual ~FhirElement();


    const std::string& name() const { return mName; }
    const std::string& typeId() const { return isRoot()?mName: mTypeId; }
    const std::string& contentReference() const { return mContentReference; }
    Representation representation() const { return mRepresentation; }
    bool isArray() const { return mIsArray; }
    /// element is a root element
    /// root elements have no \<type\>; element and carry the name of their Structure's typeId (StructureDefinition/type\@value)
    bool isRoot() const
    { return mTypeId.empty() && (mName.find('.') == std::string::npos); }

    // immutable:
    FhirElement(FhirElement&&) = default;
    FhirElement(const FhirElement&) = default;
    FhirElement& operator = (FhirElement&&) = default;
    FhirElement& operator = (const FhirElement&) = default;
private:
    std::string mName;
    std::string mTypeId;
    std::string mContentReference;
    Representation mRepresentation = Representation::element;
    bool mIsArray = false;
};

class FhirElement::Builder
{
public:
    Builder();

    explicit Builder(const FhirElement& elementTemplate);

    Builder& name(std::string name_);

    Builder& typeId(std::string type_);

    Builder& contentReference(std::string contentReference_);

    Builder& representation(Representation representation_);

    Builder& isArray(bool isArray_);

    FhirElement getAndReset();
private:
    std::unique_ptr<FhirElement> mFhirElement;
};

std::ostream& operator << (std::ostream&, const FhirElement&);

FhirElement::Representation stringToRepresentation(const std::string_view& str);
const std::string& to_string(FhirElement::Representation);
std::ostream& operator << (std::ostream&, FhirElement::Representation);


#endif// ERP_PROCESSING_CONTEXT_FHIRELEMENT_HXX
