/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_STRONGTYPE_HXX
#define EPA_LIBRARY_UTIL_STRONGTYPE_HXX

#include <utility>

namespace epa
{

template<typename WrappedT, typename TagT>
class StrongType
{
public:
    using Wrapee = WrappedT;
    using Tag = TagT;

    explicit StrongType(const WrappedT& wrapee)
      : mWrapee{wrapee}
    {
    }

    explicit StrongType(WrappedT&& wrapee)
      : mWrapee{std::move(wrapee)}
    {
    }

    const WrappedT& operator*() const
    {
        return mWrapee;
    }

    WrappedT& operator*()
    {
        return mWrapee;
    }

    const WrappedT* operator->() const
    {
        return &mWrapee;
    }

    WrappedT* operator->()
    {
        return &mWrapee;
    }

    bool operator==(const StrongType<Wrapee, Tag>& rhs) const
    {
        return mWrapee == rhs.mWrapee;
    }

private:
    WrappedT mWrapee;
};

} // namespace epa

#endif
