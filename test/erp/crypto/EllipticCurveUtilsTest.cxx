/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/crypto/EllipticCurveUtils.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/SafeString.hxx"
#include "erp/util/Base64.hxx"
#include "test_config.h"

#include <gtest/gtest.h>


class EllipticCurveUtilsTest : public testing::Test
{
};


TEST_F(EllipticCurveUtilsTest, derToPrivatePublicKey)
{
    // Key is taken from test configuration
    const SafeString privateKeyPem(R"(-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQClfUHcsU8zMDUM
rxzrGhN18Zw5HGUllJkKzvITpVx1H2i1yWSN88kzH9KnGoq3TCe7O5z6ANEEf1hE
cD7vGAhL1Bet+6dN28CTUHZhlYR0rnhMiSQNlCc+marOzG9URNRFsbxhYN5tGC2I
hRUqU6l4XHahbnpRlqONGGzq0/BPlNo8Dklf9fGzsWKriwIZ40jllytwVHCGh0sN
Zafu3inBa0W41FOnKHErIvImTjaC/bsGLL33NUdpCg0T986aAH7Y9sEJjwv1O37Y
57r61SqtHvxqErYUc0YVXuxHyLhWkygmPSsFvL9+HjjjuoJ/Lj38JDZAn8BXJrvc
J2C7CO51AgMBAAECggEBAJrtOr2LWSQI24E2ZxJKZTGjsaddx+t4xBX0S3jM9FPJ
xdN56SNjuVadUi6SkI6tQvzsADekkAlv1oirtJ9Nlma29jwxPh2LvyuqxkjxGwHJ
XGH6ecAklODsJ5ZWmVsA3OEqVbusXk8vtWD7hicMD7nYhk73CJhdOFwdI8psA6vb
ozDsVv5MoRjf6LLEP6VmK4RDg+84eBbCRK8qVUxJFyZ7Ls4df/tmtj+z3MNQuaOW
rjWew0kW440CzqdJIyZj9udAgSPtMjVd+sXsz251fxMr0GZ8WJk6aa2aSsQs2Kmh
96I1/j5YiIu6B0ShsbkGmulgss8VhipyR0TBA72oOUECgYEA07XwMzo1dhvzCqLl
1uVqyQilGXRlBftp1yHlq4BRTfsMh/WWmCdzJCrq3j3JkdY8k9e8qffM1ZbHMs4v
oO7wp3gGniy9RsCHEzNid2DN2Nmde/u+vAgykJIdwPOd3gDOzi1jynlrh2ynKyTz
wVlWUk/AO0WekIdNywB2ZMol/x0CgYEAyBv0R0FVb3pU3Vv3WUd9YKJb+dsJmO2g
Xyjug4sa8Wxzf4/VpyqpSRGRYDXNruVqcv6d57g7jdpHeieB0du706Bt6z/l+zzM
dBvaxzJ/GPooANFYy69LHq0wOjKY1i+Aon1PpAqKa51CIkEnpZ96n6RdQOd+Gz3X
WR/zzxeQ1TkCgYEApCmWAgMG5XiysvKxijsG3K/pZZ2NoF/dKEZOkvfDE9axVtOm
XIFqlQb6bC45GO6otnM5BgryOETcXZbn88CTtygo6YoDktNDai4UEkFsHNRRe4wv
0BoDK3tBuxasuTKjKdikYqJYMQCdd6UFpk2h092nT86iL6vbNKg9JdZiNDkCgYAT
r2RNaijsaX1VtUlU2AqGahJgNuLvz1h6Y/1qpVGGNGP8RXsAEdtLW9YQP9q2/MyG
+XMxK1d4ceOcKazEpzgH7n1BqiyGlYmLVn4kIFyOaXVr9ywkBV9/agwXfYi5cTzX
PzqJaZwKUBMEaaJr0Y9viuy9iMhIB8JaeyEx2yCdSQKBgG1EBM5iBAznFPFdOicZ
gA857nTIP37ytKpuBXr2GYA95EvJkL7JoKUG8TxMd+oZJyIKwuC2ytuDopBt4l4R
J4yLNNrbKUjfStiv3QSA+YUA0wPpNFWEw5RVIHrqF3zMiZO4usEhhL56NN/akox8
dMkw1lJ+k3MQ1BMakygnHcRu
-----END PRIVATE KEY-----
)");
    const auto keyPair = EllipticCurveUtils::pemToPrivatePublicKeyPair(privateKeyPem);

    ASSERT_TRUE(keyPair.isSet());
}
