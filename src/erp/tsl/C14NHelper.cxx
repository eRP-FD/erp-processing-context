/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "C14NHelper.hxx"

#include <libxml/c14n.h>
#include <libxml/parser.h>
#include <libxml/xmlversion.h>
#include <numeric>
#include <string>

#include "erp/util/GLog.hxx"
#include "erp/util/Gsl.hxx"


namespace
{
    struct CanonicalizationUserData
    {
        xmlNodePtr include;
        xmlNodePtr exclude;
    };

    bool isNodeReferredBySegment(const xmlNodePtr node, const C14NHelper::PathSegment& segment)
    {
        Expects(node);
        Expects(node->name);
        if (segment.name != std::string(reinterpret_cast<const char*>(node->name)))
            return false;

        if (static_cast<bool>(node->ns) != segment.namespaceUrl.has_value())
            return false;

        if (node->ns)
        {
            Expects(node->ns->href);
            if (*segment.namespaceUrl != std::string(reinterpret_cast<const char*>(node->ns->href)))
                return false;
        }

        return true;
    }


    xmlNodePtr getChild (
        const C14NHelper::PathSegment& pathSegment, const xmlNode* containerNode)
    {
        xmlNodePtr found = nullptr;
        for (xmlNodePtr child = containerNode->children; child != nullptr && found == nullptr; child = child->next )
        {
            if (isNodeReferredBySegment(child, pathSegment))
                found = child;
        }
        return found;
    }


    xmlNodePtr getNodeByPath(
        const xmlDocPtr document,
        const std::vector<C14NHelper::PathSegment> path)
    {
        xmlNodePtr node = nullptr;
        if (!path.empty())
        {
            // first segment in the path is the root element
            xmlNodePtr currentNode = xmlDocGetRootElement(document);
            Expects(currentNode);
            Expects(isNodeReferredBySegment(currentNode, path[0]));

            // start iteration from the second segment
            for (size_t ind = 1; ind < path.size(); ind++)
            {
                Expects(currentNode->children);
                currentNode = getChild(path[ind], currentNode);
                Expects(currentNode);
            }

            node = currentNode;
        }

        return node;
    }


    int isNodeVisibleForC14N (void* userData, xmlNodePtr node, xmlNodePtr parent)
    {
        if (userData == nullptr)
            return 1;
        CanonicalizationUserData* data = reinterpret_cast<CanonicalizationUserData*>(userData);

        // Note that `node->parent` is not necessarily the same as `parent`. That seems to be the case for
        // synthesized nodes like namespace definitions. Therefore the separate handling of node and parent.
        if (node == data->exclude)
            return 0;
        else if (node == data->include)
            return 1;
        for (xmlNodePtr currentNode=parent; currentNode!=nullptr; currentNode=currentNode->parent)
        {
            if (currentNode == data->exclude)
                return 0;
            else if (currentNode == data->include)
                return 1;
        }

        if (data->include == nullptr)
            return 1;
        else
            return 0;
    }


    std::optional<std::string> generateC14N(
        const std::string& xml,
        const std::vector<C14NHelper::PathSegment>& path,
        const xmlC14NMode mode,
        const std::vector<std::string>& inclusiveNsPrefixes,
        bool withComments)
    {
        std::optional<std::string> result;

        xmlParserCtxtPtr parserContext = nullptr;
        xmlDocPtr document = nullptr;

        try
        {
            // parse xml document
            parserContext = xmlNewParserCtxt();
            document = xmlCtxtReadMemory(parserContext, xml.data(), gsl::narrow_cast<int>(xml.size()),
                                         nullptr, nullptr, XML_PARSE_DTDATTR);
            Expects(document);

            // retrieve node according to path if any
            xmlNodePtr nodeForC14N = getNodeByPath(document, path);

            result = C14NHelper::canonicalize(nodeForC14N, nullptr, document, mode,
                                              inclusiveNsPrefixes, withComments);
        }
        catch(const std::exception& e)
        {
            LOG(ERROR) << "Can not generate C14N, exception: " << e.what();
        }
        catch(...)
        {
            LOG(ERROR) << "Can not generate C14N";
        }

        // clean the libxml memory
        if (document)
            xmlFreeDoc(document);

        if (parserContext)
            xmlFreeParserCtxt(parserContext);

        return result;
    }
}


std::optional<std::string> C14NHelper::generateC14N(
    const std::string& xml,
    const std::vector<C14NHelper::PathSegment>& path,
    const std::string& algorithm,
    const std::vector<std::string>& inclusiveNsPrefixes)
{
    std::optional<std::string> result;

    // according to gemSpec_TBAuth_V1.2.0 only Exclusive XML Canonicalization 1.0 without comments
    // is allowed
    if (algorithm == "http://www.w3.org/2001/10/xml-exc-c14n#")
    {
        // Exclusive XML Canonicalization 1.0 (omit comments)
        result = ::generateC14N(xml, path, xmlC14NMode::XML_C14N_EXCLUSIVE_1_0,
                                inclusiveNsPrefixes, false);
    }

    VLOG(3) << "C14N original:\n" << xml
               << "\nC14N result:\n" << (result ? *result : "[empty]") << "\n";

    return result;
}


std::optional<std::string> C14NHelper::canonicalize(
    xmlNodePtr includeNode,
    xmlNodePtr excludeNode,
    xmlDocPtr document,
    const xmlC14NMode mode,
    const std::vector<std::string>& inclusiveNsPrefixes,
    const bool withComments)
{
    std::optional<std::string> result;

    std::vector<xmlChar*> prefixes;
    prefixes.reserve(inclusiveNsPrefixes.size()+1);

    for (const auto& inclusiveNsPrefix : inclusiveNsPrefixes)
    {
        prefixes.push_back(const_cast<xmlChar*>(reinterpret_cast<const xmlChar*>(inclusiveNsPrefix.c_str())));
    }
    prefixes.push_back(nullptr);

    // generate C14N
    auto buffer = std::shared_ptr<xmlOutputBuffer>(xmlAllocOutputBuffer(nullptr), &xmlOutputBufferClose);
    CanonicalizationUserData userData{};
    userData.include = includeNode;
    userData.exclude = excludeNode;
    int executionResult = xmlC14NExecute(
        document,
        isNodeVisibleForC14N,
        &userData,
        static_cast<int>(mode),
        prefixes.data(),
        withComments,
        buffer.get());

    VLOG(1) << "xmlC14NExecute returns " << executionResult;

    // Was canonicalization successful?
    if (executionResult >= 0)
    {
        // Yes.
        const size_t generatedSize = xmlBufUse(buffer->buffer);
        VLOG(2) << "    generated size is " << generatedSize;
        if (generatedSize > 0)
        {
            // get the result from the buffer
            result = std::string(reinterpret_cast<char*>(xmlBufContent(buffer->buffer)), generatedSize);
        }
    }
    else
    {
        // No.
        throw std::runtime_error("xmlC14NExecute returns " + std::to_string(executionResult));
    }

    return result;
}
