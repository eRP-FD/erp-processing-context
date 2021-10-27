#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include "erp/model/Task.hxx"
#include "erp/model/Bundle.hxx"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>

class Erp6619Test : public ErpWorkflowTest
{
};


namespace
{

class TestWorker : public ErpWorkflowTestBase, public std::enable_shared_from_this<TestWorker>
{
public:
    void run(size_t lifecycleCount)
    {
        std::string kvnr;
        generateNewRandomKVNR(kvnr);
        for (size_t l = 0; l < lifecycleCount; ++l) { ASSERT_NO_FATAL_FAILURE(lifecycle(kvnr)); }
    }

    void lifecycle(const std::string& kvnr)
    {
        std::optional<model::Task> task;
        ASSERT_NO_FATAL_FAILURE(task = taskCreate());
        ASSERT_TRUE(task.has_value());
        std::string accessCode{task->accessCode()};
        auto prescriptionId{task->prescriptionId()};

        auto qesBundle = makeQESBundle(kvnr, prescriptionId, model::Timestamp::now());
        ASSERT_NO_FATAL_FAILURE(taskActivate(prescriptionId, accessCode, qesBundle));

        {
            std::optional<model::Bundle> bundle;
            ASSERT_NO_FATAL_FAILURE(bundle = taskAccept(prescriptionId, accessCode));
            ASSERT_TRUE(bundle.has_value());
            auto tasksFromBundle = bundle->getResourcesByType<model::Task>("Task");
            ASSERT_EQ(tasksFromBundle.size(), 1);
            task.emplace(std::move(tasksFromBundle.front()));
        }
        auto secret = task->secret();
        ASSERT_TRUE(secret.has_value());

        ASSERT_NO_FATAL_FAILURE(taskClose(prescriptionId, std::string{*secret}, kvnr));

        std::optional<model::Bundle> taskBundle;
        ASSERT_NO_FATAL_FAILURE(taskBundle = taskGet(kvnr));
        ++cyclesCompleted;
    }
    static std::atomic_uint32_t cyclesCompleted;
};

std::atomic_uint32_t TestWorker::cyclesCompleted;

}

// can only be executed as part of erp-integration-test
TEST_F(Erp6619Test, DISABLED_keep_alives_parallel)
{
    using namespace std::chrono_literals;
    using namespace std::chrono;
    constexpr auto threadCount = 20;
    constexpr auto lifecycleCount = 10;
    constexpr bool showProgress = false;

    std::list<std::thread> threads;


    auto startTime = std::chrono::steady_clock::now();

    for (size_t t = 0; t < threadCount; ++t)
    {
        auto worker = std::make_shared<TestWorker>();
        threads.emplace_back(std::thread{[=] { worker->run(lifecycleCount); }});
    }


    std::atomic_bool done;

    std::thread activity;

    if constexpr (showProgress)
    {
        activity = std::thread([&]{
            while (!done)
            {
                auto total = (threadCount * lifecycleCount);
                double runtimeSeconds = duration_cast<duration<double, std::ratio<1,1>>>(steady_clock::now() - startTime).count();
                std::cout << '\r' << std::right << std::setw(std::log10(total) + 1) << TestWorker::cyclesCompleted << '/' << total
                        << std::setw(12) << std::setprecision(3) << std::fixed << runtimeSeconds << "s" << std::flush;
                std::this_thread::sleep_for(73ms);
            }
        });
    }

    for (auto& thr: threads)
    {
        thr.join();
    }

    done = true;

    if (activity.joinable())
    {
        activity.join();
    }

}
