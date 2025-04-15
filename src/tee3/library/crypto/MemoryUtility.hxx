/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_CRYPTO_MEMORYUTILITY_HXX
#define EPA_LIBRARY_CRYPTO_MEMORYUTILITY_HXX

#include "library/wrappers/OpenSsl.hxx"

#include <optional>
#include <string>
#include <type_traits>

namespace epa
{

class MemoryUtility
{
public:
    /**
     * Utility function to cleanse an area of memory by overwriting it completely with 0's using
     * OPENSSL_cleanse.
     *
     * @warning This function must be used with care, especially when used with class objects, as it
     * could bring the object into an invalid state. Using a cleansed object can cause undefined
     * behavior!
     */
    template<typename T>
    static void cleanse(T& object)
    {
        // Rationale for using is_trivially_assignable: We want to overwrite the object without
        // causing memory leaks.
        static_assert(
            std::is_trivially_assignable_v<T&, T>,
            "Please define a new overload of the cleanse() function for this type");
        //Avoid memory leaks by accidentally overwriting pointers.
        static_assert(! std::is_pointer_v<T>, "Please provide a valid object by reference");

        OPENSSL_cleanse(&object, sizeof object);

        if constexpr (std::is_nothrow_default_constructible_v<T>)
        {
            // reassign a default constructed object to avoid an object in invalid state
            object = T{};
        }
    }


    /**
     * Utility function to cleanse a string by overwriting its content completely with 0's.
     */
    static void cleanse(std::string& string)
    {
        OPENSSL_cleanse(string.data(), string.size());
    }


    /**
     * Utility function to cleanse the value of an optional object, if it has any.
     */
    template<typename T>
    static void cleanse(std::optional<T>& optional)
    {
        if (optional)
            cleanse(*optional);
    }
};

} // namespace epa

#endif
