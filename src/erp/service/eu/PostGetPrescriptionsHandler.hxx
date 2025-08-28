// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef ERP_PROCESSING_CONTEXT_EUPRESCRIPTIONS_GETHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_EUPRESCRIPTIONS_GETHANDLER_HXX
#include "erp/service/task/TaskHandler.hxx"

#include <iosfwd>
#include <tuple>
#include <vector>

namespace eu
{

class PostGetPrescriptionsHandler : public TaskHandlerBase
{
public:
    PostGetPrescriptionsHandler(const std::initializer_list<std::string_view>& allowedProfessionOIDs);

    void handleRequest(PcSessionContext& session) override;

private:
    static std::optional<size_t>
    extractCountParameter(std::vector<std::pair<std::string, std::string>>& queryParameters);
    static std::vector<std::tuple<model::Task, model::KbvBundle>>
    decodeAndSort(std::vector<std::tuple<model::Task, model::Binary>>& raw);
};

}// eu_prescriptions

#endif//ERP_PROCESSING_CONTEXT_EUPRESCRIPTIONS_GETHANDLER_HXX
