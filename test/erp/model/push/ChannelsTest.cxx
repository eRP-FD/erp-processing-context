/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/push/Channels.hxx"
#include "shared/model/ModelException.hxx"

#include <gtest/gtest.h>

class ChannelsTest : public testing::Test
{
};

TEST_F(ChannelsTest, processUpdate)
{
    const auto json = R"({
  "channels": [
    {
      "id": "erp.task.accept",
      "status": "enabled"
    },
    {
      "id": "erp.task.vertreter",
      "status": "disabled"
    }
  ]
})";

    model::Channels channels;
    channels.add(model::ChannelId::erp_task_vertreter);
    ASSERT_NO_THROW(channels.processUpdate(json));
    ASSERT_EQ(channels.activeChannels().size(), 1);
    EXPECT_EQ(*channels.activeChannels().begin(), model::ChannelId::erp_task_accept);
}

TEST_F(ChannelsTest, pqxxString)
{
    model::Channels channels;
    EXPECT_EQ(channels.pqxxArrayStr(), "{}");

    channels.add(model::ChannelId::erp_task_accept);
    EXPECT_EQ(channels.pqxxArrayStr(), "{erp.task.accept}");

    channels.add(model::ChannelId::erp_task_vertreter);
    EXPECT_EQ(channels.pqxxArrayStr(), "{erp.task.accept,erp.task.vertreter}");

    channels.add(model::ChannelId::erp_task_activate);
    EXPECT_EQ(channels.pqxxArrayStr(), "{erp.task.accept,erp.task.activate,erp.task.vertreter}");
}
#include "shared/util/Expect.hxx"

TEST_F(ChannelsTest, jsonString)
{
    model::Channels channels;
    EXPECT_EQ(channels.serializeToJsonString(),
              "{\"channels\":[{\"id\":\"erp.task.activate\",\"status\":\"disabled\"},{\"id\":\"erp.task.accept\","
              "\"status\":\"disabled\"},{\"id\":\"erp.task.reject\",\"status\":\"disabled\"},{\"id\":\"erp.task."
              "close\",\"status\":\"disabled\"},{\"id\":\"erp.task.dispense\",\"status\":\"disabled\"},{\"id\":\"erp."
              "task.abort\",\"status\":\"disabled\"},{\"id\":\"erp.communication.new\",\"status\":\"disabled\"},{"
              "\"id\":\"erp.task.vertreter\",\"status\":\"disabled\"},{\"id\":\"erp.chargeitem.create\",\"status\":"
              "\"disabled\"},{\"id\":\"erp.chargeitem.update\",\"status\":\"disabled\"},{\"id\":\"erp.eu.prescription."
              "get\",\"status\":\"disabled\"},{\"id\":\"erp.eu.prescription.redeem\",\"status\":\"disabled\"},{\"id\":"
              "\"erp.eu.prescription.close\",\"status\":\"disabled\"}]}");

    channels.add(model::ChannelId::erp_chargeitem_create);
    EXPECT_EQ(channels.serializeToJsonString(),
              "{\"channels\":[{\"id\":\"erp.chargeitem.create\",\"status\":\"enabled\"},{\"id\":\"erp.task.activate\","
              "\"status\":\"disabled\"},{\"id\":\"erp.task.accept\",\"status\":\"disabled\"},{\"id\":\"erp.task."
              "reject\",\"status\":\"disabled\"},{\"id\":\"erp.task.close\",\"status\":\"disabled\"},{\"id\":\"erp."
              "task.dispense\",\"status\":\"disabled\"},{\"id\":\"erp.task.abort\",\"status\":\"disabled\"},{\"id\":"
              "\"erp.communication.new\",\"status\":\"disabled\"},{\"id\":\"erp.task.vertreter\",\"status\":"
              "\"disabled\"},{\"id\":\"erp.chargeitem.update\",\"status\":\"disabled\"},{\"id\":\"erp.eu.prescription."
              "get\",\"status\":\"disabled\"},{\"id\":\"erp.eu.prescription.redeem\",\"status\":\"disabled\"},{\"id\":"
              "\"erp.eu.prescription.close\",\"status\":\"disabled\"}]}");

    channels.add(model::ChannelId::erp_eu_prescription_get);
    EXPECT_EQ(channels.serializeToJsonString(),
              "{\"channels\":[{\"id\":\"erp.chargeitem.create\",\"status\":\"enabled\"},{\"id\":\"erp.eu.prescription."
              "get\",\"status\":\"enabled\"},{\"id\":\"erp.task.activate\",\"status\":\"disabled\"},{\"id\":\"erp.task."
              "accept\",\"status\":\"disabled\"},{\"id\":\"erp.task.reject\",\"status\":\"disabled\"},{\"id\":\"erp."
              "task.close\",\"status\":\"disabled\"},{\"id\":\"erp.task.dispense\",\"status\":\"disabled\"},{\"id\":"
              "\"erp.task.abort\",\"status\":\"disabled\"},{\"id\":\"erp.communication.new\",\"status\":\"disabled\"},{"
              "\"id\":\"erp.task.vertreter\",\"status\":\"disabled\"},{\"id\":\"erp.chargeitem.update\",\"status\":"
              "\"disabled\"},{\"id\":\"erp.eu.prescription.redeem\",\"status\":\"disabled\"},{\"id\":\"erp.eu."
              "prescription.close\",\"status\":\"disabled\"}]}");
}

TEST_F(ChannelsTest, wrongChannel)
{
    model::Channels channels;
    EXPECT_THROW(channels.add(model::toChannelId("erp.task.wrong")), model::InvalidParameterException);
}
