/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/compression/ZStdDictionaryRepository.hxx"

#include "erp/util/Expect.hxx"

using namespace std::string_literals;

bool ZStdDictionaryRepository::DictionaryIdOrder::operator()(const UniqueDictPtr& lhs, const UniqueDictPtr& rhs) const
{
    return lhs->id() < rhs->id();
}

bool ZStdDictionaryRepository::DictionaryIdOrder::operator()(const UniqueDictPtr& lhs, const ZStdDictionary::IdType& rhs) const
{
    return lhs->id() < rhs;
}

bool ZStdDictionaryRepository::DictionaryIdOrder::operator()(const ZStdDictionary::IdType& lhs, const UniqueDictPtr& rhs) const
{
    return lhs < rhs->id();
}

void ZStdDictionaryRepository::addDictionary(UniqueDictPtr&& dictionary)
{
    using namespace std::string_literals;
    using std::to_string;
    auto dictIter = mDictionaries.upper_bound(dictionary);
    Expect3(dictIter == mDictionaries.end() || (*dictIter)->id() != dictionary->id(),
        R"(Duplicate dictionary Id: )"s.append(magic_enum::enum_name(dictionary->id().use()))
            + " version: " + to_string(dictionary->id().version()),
        std::logic_error);
    auto [dictByUse, inserted] = mDictionariesByUse.emplace(dictionary->id().use(), dictionary.get());
    if (!inserted && dictByUse->second->id().version() < dictionary->id().version())
    { // dictionary with that use already in map, this one is newer
        dictByUse->second = dictionary.get();
    }
    mDictionaries.emplace_hint(dictIter, std::move(dictionary));
}


const ZStdDictionary& ZStdDictionaryRepository::getDictionary(ZStdDictionary::IdType dictId) const
{
    using std::to_string;
    const auto& dict = mDictionaries.find(dictId);
    Expect3(dict != mDictionaries.end(),
            "No dictionary for: "s.append(magic_enum::enum_name(dictId.use())) + " version: " + to_string(dictId.version()),
            std::logic_error);
    return **dict;
}

const ZStdDictionary&
ZStdDictionaryRepository::getDictionaryForUse(Compression::DictionaryUse dictUse) const
{
    Expect3(dictUse != Compression::DictionaryUse::Undefined, "DictionaryUse not defined.", std::logic_error);
    using std::to_string;
    const auto& dict = mDictionariesByUse.find(dictUse);
    Expect3(dict != mDictionariesByUse.end(),
            "No dictionary for: "s.append(magic_enum::enum_name(dictUse)),
            std::logic_error);
    return *dict->second;
}

