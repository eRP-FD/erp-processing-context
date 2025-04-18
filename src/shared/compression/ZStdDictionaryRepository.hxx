/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ZSTDDICTIONARYREPOSITORY_HXX
#define ERP_PROCESSING_CONTEXT_ZSTDDICTIONARYREPOSITORY_HXX

#include "shared/compression/ZStdDictionary.hxx"

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
