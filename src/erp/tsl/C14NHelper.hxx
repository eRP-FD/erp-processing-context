/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef SRC_BACKEND_UTIL_C14NHELPER_HXX
#define SRC_BACKEND_UTIL_C14NHELPER_HXX

#include <string>
#include <optional>
#include <vector>
#include <libxml/tree.h>
#include <libxml/c14n.h>


class C14NHelper
{
public:
    class PathSegment
    {
    public:
        std::string name;
        std::optional<std::string> namespaceUrl;
    };

    /**
     * Generates the canonicalized version of the specified node in provided xml.
     *
     * @param xml the xml document for canonicalisation
     * @param path the path to the node to canonicalise
     * @param algorithm the w3.org URL reference to the C14N algorithm
     */
    static std::optional<std::string> generateC14N(
        const std::string& xml,
        const std::vector<PathSegment>& path,
        const std::string& algorithm,
        const std::vector<std::string>& inclusiveNsPrefixes = {});

    /**
     * Canonicalize parts of the given `document`.
     * A subtree can be selected by providing `includeNode`. Part of that subtree can be excluded by providing `excludeNode`.
     *
     * @param includeNode  If NULL then the whole document is canonicalized.
     * @param excludeNode  If not NULL then this node is excluded from canonicalization.
     * @return The result of the canonicalization as string.
     */
    static std::optional<std::string> canonicalize(
        xmlNodePtr includeNode,
        xmlNodePtr excludeNode,
        xmlDocPtr document,
        const xmlC14NMode mode,
        const std::vector<std::string>& inclusiveNsPrefixes,
        const bool withComments);
};


#endif
