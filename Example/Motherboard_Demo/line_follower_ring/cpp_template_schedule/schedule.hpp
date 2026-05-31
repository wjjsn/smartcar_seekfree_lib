#include <cstddef>
#include <cstdint>

using TimeProviderFunc = uint64_t (*)();
using CallbackFunc = void (*)();

struct TaskConfig {
  uint64_t interval;
  CallbackFunc callback;
};

// 通过接收一个顶层的静态 constexpr 数组指针/引用来规避 C++20 的限制
template <TimeProviderFunc GetTime, const TaskConfig *TimerArray,
          std::size_t Count>
class StaticTimerManager {
private:
  static constexpr std::size_t TimerCount = Count;
  static inline uint64_t lastTriggerTimes[TimerCount] = {0}; // C++17 inline

public:
  static void poll() {
    uint64_t currentTime = GetTime();

    for (std::size_t i = 0; i < TimerCount; ++i) {
      // 直接从编译期期指针中读取
      uint64_t interval = TimerArray[i].interval;
      CallbackFunc callback = TimerArray[i].callback;

      if (lastTriggerTimes[i] == 0) {
        lastTriggerTimes[i] = currentTime;
        continue;
      }

      if (currentTime - lastTriggerTimes[i] >= interval) {
        if (callback) {
          callback();
        }
        lastTriggerTimes[i] = currentTime;
      }
    }
  }
};