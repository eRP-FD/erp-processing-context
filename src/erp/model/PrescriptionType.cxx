/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/model/PrescriptionType.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/Expect.hxx"

namespace model {

void checkFeatureWf200(model::PrescriptionType prescriptionType)
{
    const auto& config = Configuration::instance();
    bool featureWf200 = config.featureWf200Enabled();
    ErpExpect(featureWf200 || prescriptionType != model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
           HttpStatus::BadRequest, "Unsupported Workflow");
}

}
