/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_UTIL_SAFESTRING_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_SAFESTRING_HXX

#include "fhirtools/util/Gsl.hxx"

#include <memory>

/**
 * A std::string replacement that prevents proliferation of sensitive information like keys and certificates
 * and removes any trace from memory when deleted.
 */
class SafeString
{
public:
    class NoZeroFillTag{};
    static constexpr NoZeroFillTag no_zero_fill = {};
    // construct a SafeString of size 0, equivalent to "", the same as SafeString(0)
    SafeString (void);
    explicit SafeString (size_t size);
    explicit SafeString (const char* value);
    explicit SafeString(char* value, size_t size);
    explicit SafeString(unsigned char* value, size_t size);
    explicit SafeString(std::byte* value, size_t size);
    explicit SafeString (const NoZeroFillTag&, size_t size);

    SafeString (const SafeString& other);
    SafeString (SafeString&& other) noexcept;

    SafeString& operator= (const SafeString& other);
    SafeString& operator= (SafeString&& other) noexcept;

    /// @brief move construct from anything, that has a data and a size member (string, vector, array)
    template <typename InputT, decltype((void)std::declval<InputT>().data(), std::declval<InputT>().size())* = nullptr>
    explicit SafeString(InputT&& in); // NOLINT(bugprone-forwarding-reference-overload)

    /// @brief move from anything, that has a data and a size member (string, vector, array)
    template <typename InputT, decltype(std::declval<InputT>().data(), std::declval<InputT>().size())* = nullptr>
    SafeString& operator = (InputT&& in);

    ~SafeString (void);

    // Overwrite memory with random data and set size to 0
    // Do not use the instance after calling safeErase.
    void safeErase () noexcept;

    // The aim of the implicit conversion operators is to avoid unexpected conversions to std::string, where the sensitive
    // data would be copied into the string, as much as possible. Only conversions to view-ish types are provided.
    // Because the compiler will not allow two implicit conversions in one call, it is not possible to accidentally
    // convert for example SafeString->const char*->std::string

    /// @return const pointer to the managed memory, terminating \0 is guaranteed
    [[nodiscard]] operator const char* (void) const; //NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    /// @return const pointer to the managed memory, terminating \0 is guaranteed
    [[nodiscard]] operator const unsigned char* (void) const; //NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    /// @return const view to the managed memory, terminating \0 is guaranteed
    //NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    [[nodiscard]] operator std::string_view (void) const;

    /// @return const view to the managed memory, terminating \0 is guaranteed
    //NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    [[nodiscard]] operator std::basic_string_view<std::byte> (void) const;

    /// @return const view to the managed memory, terminating \0 is guaranteed
    [[nodiscard]] operator gsl::span<const char> (void) const; //NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    /// @return mutable pointer to the managed memory, terminating \0 is guaranteed
    [[nodiscard]] operator char* (void); //NOLINT(google-explicit-constructor,hicpp-explicit-conversions)

    /// @return mutable pointer to the managed memory, terminating \0 is guaranteed
    [[nodiscard]] explicit operator std::byte* (void); //NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
    operator std::string& () = delete;

    /// @return const pointer to the managed memory, terminating \0 is guaranteed
    [[nodiscard]] char* c_str();

    /// @return const pointer to the managed memory, terminating \0 is guaranteed
    [[nodiscard]] const char* c_str() const;

    /// @return the size of the string, excluding the terminating \0
    [[nodiscard]] size_t size (void) const;


    [[nodiscard]] bool operator == (const SafeString& other) const;
    [[nodiscard]] bool operator != (const SafeString& other) const;
    [[nodiscard]] bool operator > (const SafeString& other) const;
    [[nodiscard]] bool operator < (const SafeString& other) const;
    [[nodiscard]] bool operator >= (const SafeString& other) const;
    [[nodiscard]] bool operator <= (const SafeString& other) const;
private:
    void assignAndCleanse(char* value, size_t size);

    std::unique_ptr<char[]> mValue;
    size_t mStringLength = 0; // excluding \0

    void checkForTerminatingZero() const;

    // these two classes are needed to disambiguate the use of clear() templates below
    // if InputT has a member named `clear()` both functions are enabled
    // the one with special_ as parameter type is selected as it doesn't require
    // conversion when called with an argument of type special_
    struct general_ {};
    struct special_ : general_ {};

    template<typename InputT, decltype(std::declval<InputT>().clear())* = nullptr>
    static void clear(InputT&& in, special_);

    template<typename InputT>
    static auto clear(InputT&& in, general_)
        -> std::enable_if_t<std::is_default_constructible_v<InputT> && std::is_move_assignable_v<InputT>>;
};

template<typename InputT, decltype(std::declval<InputT>().data() , std::declval<InputT>().size())*>
SafeString& SafeString::operator=(InputT&& in)
{
    static_assert(!std::is_reference_v<InputT>);
    assignAndCleanse(in.data(), in.size());
    clear(std::forward<InputT>(in), special_{});
    return *this;
}

template<typename InputT, decltype((void)std::declval<InputT>().data() , std::declval<InputT>().size()) *>
SafeString::SafeString(InputT&& in) // NOLINT(bugprone-forwarding-reference-overload)
    : SafeString(in.data(), in.size())
{
    static_assert(!std::is_reference_v<InputT>);
    clear(std::forward<InputT>(in), special_{});
}

template<typename InputT, decltype(std::declval<InputT>().clear()) *>
void SafeString::clear(InputT && in, special_)
{
    std::forward<InputT>(in).clear();
}

template<typename InputT>
auto SafeString::clear(InputT && in, SafeString::general_)
    -> std::enable_if_t<std::is_default_constructible_v<InputT> && std::is_move_assignable_v<InputT>>
{
    std::forward<InputT>(in) = InputT{};
}


#endif
