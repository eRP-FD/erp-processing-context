/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_COMMON_BOOSTBEASTMETHOD_HXX
#define E_LIBRARY_COMMON_BOOSTBEASTMETHOD_HXX

#include "shared/network/message/HttpMethod.hxx"

#include <boost/beast/http/verb.hpp>
#include <iostream>

boost::beast::http::verb toBoostBeastVerb (HttpMethod method);

HttpMethod fromBoostBeastVerb (boost::beast::http::verb verb);

#endif
