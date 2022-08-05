/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef FHIR_TOOLS_SRC_FHIR_PATH_MODEL_COLLECTION_HXX
#define FHIR_TOOLS_SRC_FHIR_PATH_MODEL_COLLECTION_HXX

#include <cstddef>
#include <memory>
#include <vector>

namespace fhirtools
{
class Element;

class Collection : public std::vector<std::shared_ptr<const Element>>
{
public:
    using std::vector<value_type>::vector;
    Collection(std::vector<value_type>&& v);
    [[nodiscard]] value_type singleOrEmpty() const;
    [[nodiscard]] value_type single() const;
    [[nodiscard]] value_type boolean() const;
    [[nodiscard]] bool contains(const value_type& element) const;
    void append(std::vector<value_type>&& other);

    std::ostream& json(std::ostream&);

    [[nodiscard]] bool operator==(const Collection& rhs) const;
    [[nodiscard]] bool operator!=(const Collection& rhs) const;
};

std::ostream& operator<<(std::ostream& os, const Collection& collection);
}

#endif//FHIR_TOOLS_SRC_FHIR_PATH_MODEL_COLLECTION_HXX
