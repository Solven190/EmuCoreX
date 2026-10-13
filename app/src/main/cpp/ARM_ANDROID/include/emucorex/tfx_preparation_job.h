#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace emucorex::android
{
enum class TFXPreparationState
{
    Idle, WaitingForLifecycle, Starting, Running, Cancelling,
    Completed, Cancelled, Failed, Unsupported
};

enum class TFXPreparationError
{
    None, LifecycleBusy, UnsupportedRenderer, CacheDisabled,
    MissingSource, DeviceCreationFailed, PipelineFailures, CheckpointFailed, InternalError
};

struct TFXPreparationProgress
{
    uint32_t total = 0;
    uint32_t completed = 0;
    uint32_t already_available = 0;
    uint32_t failed = 0;
    uint32_t cancelled = 0;
    uint32_t in_flight = 0;
    bool finished = false;
    bool checkpoint_saved = false;
};

struct TFXPreparationStatus : TFXPreparationProgress
{
    uint64_t job_id = 0;
    uint32_t attempt = 0;
    TFXPreparationState state = TFXPreparationState::Idle;
    TFXPreparationError error = TFXPreparationError::None;
    bool cancel_requested = false;
    std::string adapter_name;
};

// Polling and cancellation never access a Vulkan device or the VM lifecycle lock.
class TFXPreparationJob
{
public:
    TFXPreparationJob(uint64_t job_id, uint32_t attempt);
    static bool IsActiveState(TFXPreparationState state);
    TFXPreparationStatus Snapshot() const;
    bool RequestCancel();
    bool CancellationRequested() const;
    bool PublishStage(TFXPreparationState state);
    bool PublishProgress(const TFXPreparationProgress& progress, std::string adapter_name = {});
    // The lifecycle owner calls this only after device teardown and lock release.
    bool Finish(TFXPreparationState state, TFXPreparationError error = TFXPreparationError::None);

private:
    mutable std::mutex mutex_;
    TFXPreparationStatus status_;
    std::atomic<bool> cancel_requested_{false};
};
}
