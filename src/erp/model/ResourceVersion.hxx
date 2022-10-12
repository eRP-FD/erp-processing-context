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
    v1_1_1,// https://simplifier.net/packages/de.gematik.erezept-workflow.r4/1.1.1
};

std::string_view v_str(const DeGematikErezeptWorkflowR4& v);
DeGematikErezeptWorkflowR4 str_vGematik(const std::string_view& versionString);

enum class KbvItaErp
{
    v1_0_2,// https://simplifier.net/packages/kbv.ita.erp/1.0.2
};

std::string_view v_str(const KbvItaErp& v);
KbvItaErp str_vKbv(const std::string_view& versionString);


// Bundled profile versions of Gematik, KBV, DAV, PKV, GKV profiles.
// See https://github.com/gematik/api-erp/blob/master/docs/erp_fhirversion.adoc
enum class FhirProfileBundleVersion
{
    /**
     * KBV: 1.0.2
     * gematik: 1.1.1
     * DAV: Package 1.1.2 Profile 1.1.0
     * GKV: 1.1.0
     * PKV: 1.1.0-rc6
     */
    v_2022_01_01,

    /**
     * KBV: 1.0.2
     * gematik: 1.1.1
     * DAV: Package 1.2.0 Profile 1.2
     * GKV: Package 1.2.0 Profile 1.2
     * PKV: 1.1.0-rc9
     */
    v_2022_07_01,

    /**
     * KBV: tdb
     * gematik: Package 1.2.0 Profile 1.2
     * DAV: Package 1.2.0 Profile 1.2
     * GKV: Package 1.2.0 Profile 1.2
     * PKV: 1.1.0-rc9
     */
    v_2023_01_01
};
std::string_view v_str(const FhirProfileBundleVersion& v);
FhirProfileBundleVersion str_vBundled(const std::string_view& versionString);


[[nodiscard]] ::std::tuple<DeGematikErezeptWorkflowR4, KbvItaErp, FhirProfileBundleVersion> current();
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
