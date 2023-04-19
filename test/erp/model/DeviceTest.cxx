/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/model/Device.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/util/FileHelper.hxx"

#include <gtest/gtest.h>

using namespace model;

TEST(DeviceTest, DefaultConstruct)//NOLINT(readability-function-cognitive-complexity)
{
    const Device device;

    EXPECT_EQ(device.id(), "1");
    EXPECT_EQ(device.status(), Device::Status::active);
    EXPECT_EQ(device.serialNumber(), ErpServerInfo::ReleaseVersion());
    EXPECT_EQ(device.version(), ErpServerInfo::ReleaseVersion());
    EXPECT_EQ(device.name(), Device::Name);
    EXPECT_TRUE(device.contact(Device::CommunicationSystem::email).has_value());
    EXPECT_EQ(device.contact(Device::CommunicationSystem::email).value(), Device::Email);

    EXPECT_NO_THROW((void)device.serializeToXmlString());
    EXPECT_NO_THROW((void)device.serializeToJsonString());
}


TEST(DeviceTest, ConstructFromJson)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string json = R"(
    {
      "resourceType":"Device",
      "id":"DEVICE_ID",
      "meta":{
        "profile":[
          "https://gematik.de/fhir/StructureDefinition/ErxDevice"
        ]
      },
      "status":"DEVICE_STATUS",
      "deviceName":[
        {
          "name":"DEVICE_NAME",
          "type":"user-friendly-name"
        }
      ],
      "version":[
        {
          "value":"ERP_RELEASE_VERSION"
        }
      ],
      "serialNumber":"ERP_RELEASE_VERSION",
      "contact":[
        {
          "system":"email",
          "value":"EMAIL"
        }
      ]
    })";

    // For testing use different values than the default constructor will assign.
    const std::string deviceId = "9876";
    const Device::Status status = Device::Status::entered_in_error;
    const std::string serialNumber = "R1.12345";
    const std::string version = "4.3.2";
    const std::string deviceName = "Our service";
    const std::string phoneContact = "+49 333 123456789 00";
    const std::string faxContact = "+49 333 123456789 10";
    const std::string emailContact = "mailbox@gematic.de";
    const std::string pagerContact = "+49 333 123456789 20";
    const std::string urlContact = "www::gematic.de";
    const std::string smsContact = "+49 333 123456789 30";

    Device device = Device::fromJsonNoValidation(json);

    device.setStatus(Device::Status::entered_in_error);
    device.setVersion(version);
    device.setSerialNumber(serialNumber);
    device.setName(deviceName);
    device.setContact(Device::CommunicationSystem::phone, phoneContact);
    device.setContact(Device::CommunicationSystem::fax, faxContact);
    device.setContact(Device::CommunicationSystem::email, emailContact);
    device.setContact(Device::CommunicationSystem::pager, pagerContact);
    device.setContact(Device::CommunicationSystem::url, urlContact);
    device.setContact(Device::CommunicationSystem::sms, smsContact);

    EXPECT_EQ(device.status(), status);
    EXPECT_EQ(device.version(), version);
    EXPECT_EQ(device.serialNumber(), serialNumber);
    EXPECT_EQ(device.name(), deviceName);
    EXPECT_TRUE(device.contact(Device::CommunicationSystem::phone).has_value());
    EXPECT_EQ(device.contact(Device::CommunicationSystem::phone).value(), phoneContact);
    EXPECT_TRUE(device.contact(Device::CommunicationSystem::fax).has_value());
    EXPECT_EQ(device.contact(Device::CommunicationSystem::fax).value(), faxContact);
    EXPECT_TRUE(device.contact(Device::CommunicationSystem::email).has_value());
    EXPECT_EQ(device.contact(Device::CommunicationSystem::email).value(), emailContact);
    EXPECT_TRUE(device.contact(Device::CommunicationSystem::pager).has_value());
    EXPECT_EQ(device.contact(Device::CommunicationSystem::pager).value(), pagerContact);
    EXPECT_TRUE(device.contact(Device::CommunicationSystem::url).has_value());
    EXPECT_EQ(device.contact(Device::CommunicationSystem::url).value(), urlContact);
    EXPECT_TRUE(device.contact(Device::CommunicationSystem::sms).has_value());
    EXPECT_EQ(device.contact(Device::CommunicationSystem::sms).value(), smsContact);

    const ::testing::TestInfo* testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string testName = testInfo->name();
    std::string testCaseName = testInfo->test_case_name();
    std::string fileName = testCaseName + "-" + testName;

    EXPECT_NO_THROW((void)device.serializeToXmlString());
    EXPECT_NO_THROW((void)device.serializeToJsonString());
}
