/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef E_LIBRARY_COMMON_BOOSTBEASTMETHOD_HXX
#define E_LIBRARY_COMMON_BOOSTBEASTMETHOD_HXX

#include "erp/common/HttpMethod.hxx"

#include <boost/beast/http/verb.hpp>
#include <iostream>

boost::beast::http::verb toBoostBeastVerb (HttpMethod method);

HttpMethod fromBoostBeastVerb (boost::beast::http::verb verb);

#endif
