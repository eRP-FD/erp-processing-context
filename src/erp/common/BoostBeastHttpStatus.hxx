/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_COMMON_BOOSTBEASTHTTPSTATUS_HXX
#define E_LIBRARY_COMMON_BOOSTBEASTHTTPSTATUS_HXX

#include "erp/common/HttpStatus.hxx"

#include <boost/beast/http/status.hpp>
#include <iostream>


boost::beast::http::status toBoostBeastStatus (HttpStatus status);

HttpStatus fromBoostBeastStatus (boost::beast::http::status status);

#endif
