#ifndef ERP_PROCESSING_CONTEXT_ZSTDDICTIONARYREPOSITORY_HXX
#define ERP_PROCESSING_CONTEXT_ZSTDDICTIONARYREPOSITORY_HXX

#include "erp/compression/ZStdDictionary.hxx"

#include <map>
#include <set>

class ZStdDictionaryRepository
{
public:
    using UniqueDictPtr = std::unique_ptr<ZStdDictionary>;

    void addDictionary(UniqueDictPtr&& dictionary);
    const ZStdDictionary& getDictionary(ZStdDictionary::IdType id) const;
    const ZStdDictionary& getDictionaryForUse(Compression::DictionaryUse dictUse) const;

private:
    class DictionaryIdOrder
    {
    public:
        using is_transparent = ZStdDictionary::IdType;
        bool operator() (const UniqueDictPtr& lhs, const UniqueDictPtr& rhs) const;
        bool operator() (const ZStdDictionary::IdType& lhs, const UniqueDictPtr& rhs) const;
        bool operator() (const UniqueDictPtr& lhs, const ZStdDictionary::IdType& rhs) const;
    };

    std::set<UniqueDictPtr, DictionaryIdOrder> mDictionaries;
    std::map<Compression::DictionaryUse, const ZStdDictionary*> mDictionariesByUse;
};


#endif // ERP_PROCESSING_CONTEXT_ZSTDDICTIONARYREPOSITORY_HXX
