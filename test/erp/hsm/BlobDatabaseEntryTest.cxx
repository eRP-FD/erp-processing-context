/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/hsm/BlobDatabase.hxx"

#include <gtest/gtest.h>

TEST(BlobDatabaseEntryTest, isBlobValid)
{
    ::BlobDatabase::Entry entry;
    entry.type = ::BlobType::EciesKeypair;
    entry.name = ::ErpVector::create("name is usually binary data");
    entry.blob.data = ::SafeString("this is the blob data");
    entry.blob.generation = 1;
    entry.id = 0;
    entry.expiryDateTime = ::std::chrono::system_clock::now() + ::std::chrono::minutes(1);

    EXPECT_TRUE(entry.isBlobValid());

    entry.expiryDateTime = ::std::chrono::system_clock::now() - ::std::chrono::minutes(1);

    EXPECT_FALSE(entry.isBlobValid());

    entry.expiryDateTime = {};
    entry.startDateTime = ::std::chrono::system_clock::now() - ::std::chrono::minutes(1);
    entry.endDateTime = ::std::chrono::system_clock::now() + ::std::chrono::minutes(1);

    EXPECT_TRUE(entry.isBlobValid());

    entry.startDateTime = ::std::chrono::system_clock::now();

    EXPECT_TRUE(entry.isBlobValid());

    entry.startDateTime = ::std::chrono::system_clock::now() + ::std::chrono::minutes(1);

    EXPECT_FALSE(entry.isBlobValid());

    entry.startDateTime = ::std::chrono::system_clock::now() - ::std::chrono::minutes(2);
    entry.endDateTime = ::std::chrono::system_clock::now() - ::std::chrono::minutes(1);

    EXPECT_FALSE(entry.isBlobValid());

    entry.type = ::BlobType::Quote;
    entry.endDateTime = ::std::chrono::system_clock::now() + ::std::chrono::minutes(1);
    entry.pcrHash = ::ErpVector::create("123456");
    EXPECT_TRUE(entry.isBlobValid());
    EXPECT_TRUE(entry.isBlobValid(std::chrono::system_clock::now(), entry.pcrHash.value()));
    EXPECT_FALSE(entry.isBlobValid(std::chrono::system_clock::now(), ::ErpVector::create("abcdef")));
}
