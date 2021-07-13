#ifndef ERP_PROCESSING_CONTEXT_TEST_MOCK_MOCKHSM_HXX
#define ERP_PROCESSING_CONTEXT_TEST_MOCK_MOCKHSM_HXX

#include "erp/crypto/RandomSource.hxx"
#include "erp/util/SafeString.hxx"

class MockRandom
    : public RandomSource
{
public:
    MockRandom (void);

    SafeString randomBytes (size_t count) override;

    void SetRandomBytesOffset(uint8_t offset);

    SafeString operator()(size_t count) const;

private:
    uint8_t mRandomOffset = 0;
};


#endif
