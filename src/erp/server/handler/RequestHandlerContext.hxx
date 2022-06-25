/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_SERVER_HANDLER_REQUESTHANDLERCONTEXT_HXX
#define ERP_PROCESSING_CONTEXT_SERVER_HANDLER_REQUESTHANDLERCONTEXT_HXX

#include "erp/common/HttpMethod.hxx"

#include "erp/util/Expect.hxx"

#include <functional>
#include <unordered_map>
#include <string>
#include <vector>
#include <regex>


class SessionContext;

class RequestHandlerInterface;


class RequestHandlerContext
{
public:
    using HandlerType = RequestHandlerInterface;

    const HttpMethod method;
    const std::string path;
    const std::regex pathRegex;
    const std::vector<std::string> pathParameterNames;
    std::unique_ptr<HandlerType> handler;

    RequestHandlerContext (
        HttpMethod method,
        const std::string& path,
        std::unique_ptr<HandlerType>&& handler);
    RequestHandlerContext (RequestHandlerContext&& other) noexcept = default;
    RequestHandlerContext (void) = delete;
    RequestHandlerContext (const RequestHandlerContext& other) = delete;
    RequestHandlerContext& operator= (const RequestHandlerContext& other) = delete;
    ~RequestHandlerContext (void);

    /** Match a concrete path against the path regex.
     *
     * @param method Method of an incoming request.
     * @param requestPath Path of an incoming request.
     * @return <true,list of path values> if the path matches the regex or
     *         <false, empty list> if it doesn't.
     *         The returned list of path values corresponds to the list of parameter names `pathParameterNames` in the
     *         path regex `pathRegex`.
     */
    std::tuple<bool,std::vector<std::string>> matches (
        HttpMethod method,
        const std::string& requestPath) const;
};


class RequestHandlerContainer
{
public:
    RequestHandlerContainer(void) = default;
    RequestHandlerContainer(const RequestHandlerContainer& other) = delete;
    RequestHandlerContainer(RequestHandlerContainer&& other) noexcept = default;
    RequestHandlerContainer& operator=(const RequestHandlerContainer& other) = delete;
    RequestHandlerContainer& operator=(RequestHandlerContainer&& other) noexcept = delete;

    using ItemType = RequestHandlerContext;
    using ContainerType = std::unordered_map<std::string, std::unique_ptr<ItemType>>;

    /**
     * Create and add a new entry to the container. The call signature follows that of emplace, i.e. the first
     * item of arguments will be the key, the rest will be passed to a constructor of ItemType.
     * Returns a reference to the newly created item
     * or throws an exception of item creation failed.
     */
    template<typename ... Arguments>
        ItemType& addHandler (std::string_view key, Arguments&&... arguments);

    typename ContainerType::const_iterator find (const std::string& key) const;

    typename ContainerType::const_iterator begin (void) const;
    typename ContainerType::const_iterator end (void) const;

private:
    ContainerType mHandlers;
};


template<typename ... Arguments>
typename RequestHandlerContainer::ItemType& RequestHandlerContainer::addHandler (
    std::string_view key,
    Arguments&&... arguments)
{
    const auto [iterator,success] = mHandlers.emplace(
        key,
        std::make_unique<ItemType>(std::forward<Arguments>(arguments)...));
    Expect(success && iterator!=mHandlers.end(), "creation of RequestHandlerContext failed");
    Expect(iterator->second!=nullptr, "creation of RequestHandlerContext failed");
    return *iterator->second;
}


#endif
