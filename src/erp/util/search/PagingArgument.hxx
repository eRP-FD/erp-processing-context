/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_PAGINGARGUMENT_HXX
#define ERP_PROCESSING_CONTEXT_PAGINGARGUMENT_HXX

#include "shared/model/Timestamp.hxx"

#include <optional>
#include <string>
#include <utility>
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
    static constexpr std::string_view idKey = "_id";

    explicit PagingArgument ();

    /**
     * Value is accepted as string so that input validation is offloaded from the caller to this method.
     * @throw ErpException if the input can not be interpreted as unsigned number
     */
    void setCount (const std::string& countString);
    bool hasDefaultCount () const;
    size_t getCount () const;
    static size_t getDefaultCount ();

    void setEntryTimestampRange(const model::Timestamp& firstEntry, const model::Timestamp& lastEntry);
    std::optional<std::pair<model::Timestamp, model::Timestamp>> getEntryTimestampRange() const;

    /**
     * Value is accepted as string so that input validation is offloaded from the caller to this method.
     * @throw ErpException if the input can not be interpreted as unsigned number
     */
    void setOffset (const std::string& offset);
    size_t getOffset () const;

    /**
     * Return whether either or both of offset and count have non-default values.
     */
    bool isSet () const;
    /**
     * Return whether there is a 'prev' page.
     * That is always the case when offset > 0.
     */
    bool hasPreviousPage () const;
    /**
     * Return whether there is a 'next' page.
     */
    bool hasNextPage (const std::size_t totalSearchMatches) const;
    /**
     * totalSearchMatches from Handler
     */
    void setTotalSearchMatches(const std::size_t totalSearchMatches);
    size_t getTotalSearchMatches () const;

    size_t getOffsetLastPage () const;


private:
    size_t mCount;
    size_t mOffset;
    size_t mTotalSearchMatches;
    std::optional<std::pair<model::Timestamp, model::Timestamp>> mEntryTimestampRange;
};


#endif
