/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_PAGINGARGUMENT_HXX
#define ERP_PROCESSING_CONTEXT_PAGINGARGUMENT_HXX

#include <optional>
#include <string>
#include <vector>


/**
 * Collection of data and functionality around paging of the result of a REST request.
 *
 * The SQL implementation for paging, currently located in UrlArguments, but based on data held in this class,
 * is rather simple. It uses SQL's LIMIT and OFFSET.
 * The alternative keyset paging does not work with complex ORDER clauses (or even simple ones, rather anything that
 * does not sort on the primary id).
 */
class PagingArgument
{
public:
    static constexpr std::string_view countKey = "_count";
    static constexpr std::string_view offsetKey = "__offset";

    explicit PagingArgument (void);

    /**
     * Value is accepted as string so that input validation is offloaded from the caller to this method.
     * @throw ErpException if the input can not be interpreted as unsigned number
     */
    void setCount (const std::string& countString);
    bool hasDefaultCount (void) const;
    size_t getCount (void) const;
    static size_t getDefaultCount (void);

    /**
     * Value is accepted as string so that input validation is offloaded from the caller to this method.
     * @throw ErpException if the input can not be interpreted as unsigned number
     */
    void setOffset (const std::string& offset);
    size_t getOffset (void) const;

    /**
     * Return whether either or both of offset and count have non-default values.
     */
    bool isSet (void) const;
    /**
     * Return whether there is a 'prev' page.
     * That is always the case when offset > 0.
     */
    bool hasPreviousPage (void) const;
    /**
     * Return whether there is a 'next' page.
     * At the moment the result is hard coded to `true`.  The reason is that for finding out whether or not there
     * is a next page, we need additional information from the database. This is somewhat expensive to obtain.
     * The cost does not seem to be worth the effect at the moment.
     */
    bool hasNextPage (void) const;

private:
    size_t mCount;
    size_t mOffset;
};


#endif
