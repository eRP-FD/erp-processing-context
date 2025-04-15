// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#ifndef ERP_EXPORTER_UTIL_RUNTIMECONFIGURATION_HXX
#define ERP_EXPORTER_UTIL_RUNTIMECONFIGURATION_HXX

#include "shared/model/Timestamp.hxx"

#include <boost/core/noncopyable.hpp>
#include <mutex>
#include <shared_mutex>
#include <variant>

namespace exporter
{
class RuntimeConfiguration
{
public:
  static constexpr std::string_view parameter_pause = "Pause";
  static constexpr std::string_view parameter_resume = "Resume";
  static constexpr std::string_view parameter_throttle = "Throttle";

  class Getter;
  class Setter;

private:
  bool isPaused() const;
  std::chrono::milliseconds throttleValue() const;

  void pause();
  void resume();
  void throttle(const std::chrono::milliseconds& throttle);

  mutable std::shared_mutex mSharedMutex;

  bool mPause{false};
  std::chrono::milliseconds mThrottle{0};
};

class RuntimeConfiguration::Getter : boost::noncopyable
{
public:
  explicit Getter(std::shared_ptr<const RuntimeConfiguration> runtimeConfiguration);

  bool isPaused() const;
  std::chrono::milliseconds throttleValue() const;

private:
  std::shared_ptr<const RuntimeConfiguration> mRuntimeConfiguration;
  std::shared_lock<std::shared_mutex> mSharedLock;
};

class RuntimeConfigurationGetter : public RuntimeConfiguration::Getter
{
public:
  using Getter::Getter;
};


class RuntimeConfiguration::Setter : boost::noncopyable
{
public:
  explicit Setter(std::shared_ptr<RuntimeConfiguration> runtimeConfiguration);

  void pause();
  void resume();
  void throttle(const std::chrono::milliseconds& throttle);

  bool isPaused() const;
  std::chrono::milliseconds throttleValue() const;

private:
  std::shared_ptr<RuntimeConfiguration> mRuntimeConfiguration;
  std::unique_lock<std::shared_mutex> mUniqueLock;
};

class RuntimeConfigurationSetter : public RuntimeConfiguration::Setter
{
public:
  using Setter::Setter;
};
}

#endif
