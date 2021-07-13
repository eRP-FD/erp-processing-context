#ifndef ERP_PROCESSING_CONTEXT_RAPIDJSONDOCUMENT_HXX
#define ERP_PROCESSING_CONTEXT_RAPIDJSONDOCUMENT_HXX

#include <rapidjson/document.h>
#include "erp/model/NumberAsStringParserDocument.hxx"

/**
 * This is a workaround for the fact that rapidjson::GenericDocument uses a static int value in its constructor.
 * That makes its values undefined when static GenericDocument variables are used. The order in which the static int
 * and the static variable is defined is undefined.
 *
 * This class offers a solution by encapsulating the creation of new GenericDocument instances with a simple
 * factory method that holds a local static instance of GenericDocument. With this well known trick the time of which
 * the instance is created is well defined, i.e. when the method is entered for the first time. At this time all
 * global static initialization has finished and the relative initialization order of the two values is well defined.
 *
 * Note that the implementation is header-file-only so that the compiler can inline the small methods.
 *
 * How to use:
 * 1. create an empty struct, call it, say, Mark
 * 2. replace a static member or variable of type rapidjson::Document with one of RapidjsonDocument<Mark>
 * 3. everywhere the document is used replace the . with a ->.
 *
 * The Mark struct is not used. It merley prevents from multiple instances of RapidjsonDocument to use the same
 * instance of the inner rapidjson::Document variable.
 * Example:
 *
 * struct SomeName;
 * RapidjsonDocument<SomeName> document;
 * ...{...
 *     document->Parse(...);
 *     doSomething(*document);
 * ...}...
 *
 */
template<typename Mark>
class RapidjsonDocument
{
public:
    rapidjson::Document& instance (void)
    {
        static rapidjson::Document instance;
        return instance;
    }

    rapidjson::Document* operator-> (void)
    {
        return &instance();
    }

    inline rapidjson::Document& operator* (void)
    {
        return instance();
    }
};

/**
 * For the resource model classes, in which prefixes for strings and numerical values are inserted in the
 * json document to maintain the number precision the same workaround as described above must be used.
 */
template<typename Mark>
class RapidjsonNumberAsStringParserDocument
{
public:
    model::NumberAsStringParserDocument& instance(void)
    {
        static model::NumberAsStringParserDocument instance;
        return instance;
    }

    model::NumberAsStringParserDocument* operator-> (void)
    {
        return &instance();
    }

    inline model::NumberAsStringParserDocument& operator* (void)
    {
        return instance();
    }
};

#endif
