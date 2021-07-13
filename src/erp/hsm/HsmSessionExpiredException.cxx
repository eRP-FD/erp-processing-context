#include "erp/hsm/HsmSessionExpiredException.hxx"

#include "erp/hsm/production/HsmProductionClient.hxx"


HsmSessionExpiredException::HsmSessionExpiredException (const std::string& what, const int errorCode)
    : HsmException(what,errorCode)
{
}
