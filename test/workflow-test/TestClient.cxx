#include "TestClient.hxx"

std::function<std::unique_ptr<TestClient>(void)> TestClient::mFactory;

void TestClient::setFactory(TestClient::Factory&& factory)
{
    mFactory = std::move(factory);
}

std::unique_ptr<TestClient> TestClient::create()
{
    return mFactory();
}

