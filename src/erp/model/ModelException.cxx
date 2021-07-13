#include "erp/model/ModelException.hxx"

namespace model {

ModelException::ModelException (const std::string& message)
    : std::runtime_error(message)
{
}

}