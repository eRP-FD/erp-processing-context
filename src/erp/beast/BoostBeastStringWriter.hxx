#ifndef ERP_PROCESSING_CONTEXT_BOOSTBEASTSTRINGWRITER_HXX
#define ERP_PROCESSING_CONTEXT_BOOSTBEASTSTRINGWRITER_HXX


#include <string>

class Header;


/**
 * Helper class for serializing requests and responses into a string.
 */
class BoostBeastStringWriter
{
public:
    static std::string serializeRequest (const Header& header, const std::string_view& body);
    static std::string serializeResponse (const Header& header, const std::string_view& body);

};


#endif
