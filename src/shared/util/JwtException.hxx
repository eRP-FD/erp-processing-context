/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef E_LIBRARY_UTIL_JWTEXCEPTION_HXX
#define E_LIBRARY_UTIL_JWTEXCEPTION_HXX

#include <stdexcept>

class JwtException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class JwtInvalidRfcFormatException : public JwtException
{
public:
    using JwtException::JwtException;
};

class JwtExpiredException : public JwtException
{
public:
    using JwtException::JwtException;
};

class JwtInvalidFormatException : public JwtException
{
public:
    using JwtException::JwtException;
};

class JwtInvalidSignatureException : public JwtException
{
public:
    using JwtException::JwtException;
};

class JwtRequiredClaimException : public JwtException
{
public:
    using JwtException::JwtException;
};

class JwtInvalidAudClaimException : public JwtException
{
public:
    using JwtException::JwtException;
};

#endif//  E_LIBRARY_UTIL_JWTEXCEPTION_HXX
