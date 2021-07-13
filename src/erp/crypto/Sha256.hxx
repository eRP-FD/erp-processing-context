#ifndef ERP_PROCESSING_CONTEXT_SHA256_HXX
#define ERP_PROCESSING_CONTEXT_SHA256_HXX

#include <string>
#include <string_view>

class Sha256
{
public:
    static std::string fromBin(const std::string_view& data);
};

#endif// ERP_PROCESSING_CONTEXT_SHA256_HXX
