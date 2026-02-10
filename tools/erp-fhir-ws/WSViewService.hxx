/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */
#pragma once

#include "shared/model/Timestamp.hxx"

#include <optional>
#include <set>
#include <shared_mutex>
#include <boost/asio/execution_context.hpp>

namespace fhirtools {
class FhirStructureRepositoryView;
};

namespace erp_fhir_ws
{

class WSViewService : public boost::asio::execution_context::service
{
public:
    struct View
    {
        std::string id;
        std::optional<model::Timestamp> start;
        std::optional<model::Timestamp> end;
        std::shared_ptr<const fhirtools::FhirStructureRepositoryView> view;

        struct Cmp
        {
            using is_transparent = void;
            template <typename RhsT, typename LhsT>
            bool operator()(const LhsT& lhs, const RhsT& rhs) const;
        private:
            static std::string_view id(const View& view);
            template <typename T>
            static std::string_view id(const T& value);
        };
    };


    explicit WSViewService(boost::asio::execution_context& context);

    const View& getView(std::string_view viewParam);


    static boost::asio::execution_context::id id;
protected:
    void shutdown() override;

private:
    const View& createView(std::string_view viewParam);

    std::shared_mutex mViewsMutex;
    std::set<View, View::Cmp> mViews;
};

}
