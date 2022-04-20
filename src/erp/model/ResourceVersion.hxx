/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_RESOURCEVERSION_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_RESOURCEVERSION_HXX


#include <string>
#include <string_view>
#include <tuple>

namespace model
{
namespace ResourceVersion
{
enum class NotProfiled {};

enum class DeGematikErezeptWorkflowR4
{
    v1_0_3_1,// https://simplifier.net/packages/de.gematik.erezept-workflow.r4/1.0.3-1
    v1_1_1,// https://simplifier.net/packages/de.gematik.erezept-workflow.r4/1.1.1
};

std::string_view v_str(const DeGematikErezeptWorkflowR4& v);
DeGematikErezeptWorkflowR4 str_vGematik(const std::string_view& versionString);

enum class KbvItaErp
{
    v1_0_1,// https://simplifier.net/packages/kbv.ita.erp/1.0.1
    v1_0_2,// https://simplifier.net/packages/kbv.ita.erp/1.0.2
};

std::string_view v_str(const KbvItaErp& v);
KbvItaErp str_vKbv(const std::string_view& versionString);


[[nodiscard]] ::std::tuple<DeGematikErezeptWorkflowR4, KbvItaErp> current();
template<typename VersionType>
[[nodiscard]] VersionType current()
{
    return ::std::get<VersionType>(current());
}

[[nodiscard]] ::std::string versionizeProfile(::std::string_view profile);

std::string_view v_str(const NotProfiled&);

}
}

#endif//ERP_PROCESSING_CONTEXT_SRC_ERP_MODEL_RESOURCEVERSION_HXX
