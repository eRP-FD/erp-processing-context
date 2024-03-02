/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_XMLMEMORY_HXX
#define ERP_PROCESSING_CONTEXT_XMLMEMORY_HXX

#include <libxml/tree.h>
#include <memory>

template <auto freeFunction>
class FreeXml;

template <auto freeFunction>
class FreeXml final
{
private:
    template<typename T>
    static auto arg1Type(void(*)(T*)) -> T;
public:
    using Type = decltype(arg1Type(freeFunction));

    void operator() (Type* doc) {freeFunction(doc); }
};

template <auto freeFunction>
using UniqueXmlPtr = std::unique_ptr<typename FreeXml<freeFunction>::Type, FreeXml<freeFunction>>;

using UniqueXmlDocumentPtr = UniqueXmlPtr<xmlFreeDoc>;
using UniqueXmlNodePtr = UniqueXmlPtr<xmlFreeNode>;
using UniqueXmlNsPtr = UniqueXmlPtr<xmlFreeNs>;

extern template class std::unique_ptr<FreeXml<xmlFreeDoc>::Type, FreeXml<xmlFreeDoc>>;
extern template class std::unique_ptr<FreeXml<xmlFreeNode>::Type, FreeXml<xmlFreeNode>>;
extern template class std::unique_ptr<FreeXml<xmlFreeNs>::Type, FreeXml<xmlFreeNs>>;


#endif // ERP_PROCESSING_CONTEXT_XMLMEMORY_HXX
