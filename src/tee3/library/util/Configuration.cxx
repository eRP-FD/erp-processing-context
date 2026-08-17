/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2026
 *  (C) Copyright IBM Corp. 2021, 2026
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/Configuration.hxx"

namespace epa
{


const Configuration& epa::Configuration::getInstance()
{
    static Configuration instance;
    return instance;
}

}
