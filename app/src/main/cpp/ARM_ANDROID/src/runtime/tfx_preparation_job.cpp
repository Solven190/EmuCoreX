#include "emucorex/tfx_preparation_job.h"

namespace emucorex::android
{
TFXPreparationJob::TFXPreparationJob(uint64_t job_id, uint32_t attempt)
{
    status_.job_id = job_id;
    status_.attempt = attempt;
    status_.state = TFXPreparationState::WaitingForLifecycle;
}

bool TFXPreparationJob::IsActiveState(TFXPreparationState state)
{
    return state == TFXPreparationState::WaitingForLifecycle || state == TFXPreparationState::Starting ||
        state == TFXPreparationState::Running || state == TFXPreparationState::Cancelling;
}

TFXPreparationStatus TFXPreparationJob::Snapshot() const
{
    std::lock_guard lock(mutex_);
    return status_;
}

bool TFXPreparationJob::RequestCancel()
{
    std::lock_guard lock(mutex_);
    if (!IsActiveState(status_.state) || status_.cancel_requested)
        return false;
    status_.cancel_requested = true;
    status_.state = TFXPreparationState::Cancelling;
    cancel_requested_.store(true, std::memory_order_release);
    return true;
}

bool TFXPreparationJob::CancellationRequested() const
{
    return cancel_requested_.load(std::memory_order_acquire);
}

bool TFXPreparationJob::PublishStage(TFXPreparationState state)
{
    std::lock_guard lock(mutex_);
    if (status_.cancel_requested)
        return false;
    if ((status_.state == TFXPreparationState::WaitingForLifecycle && state == TFXPreparationState::Starting) ||
        (status_.state == TFXPreparationState::Starting && state == TFXPreparationState::Running))
    {
        status_.state = state;
        return true;
    }
    return false;
}

bool TFXPreparationJob::PublishProgress(const TFXPreparationProgress& progress, std::string adapter_name)
{
    std::lock_guard lock(mutex_);
    if (status_.state != TFXPreparationState::Running && status_.state != TFXPreparationState::Cancelling)
        return false;
    const uint64_t accounted = uint64_t(progress.completed) + progress.already_available +
        progress.failed + progress.cancelled + progress.in_flight;
    if (accounted > progress.total || (progress.finished && (progress.in_flight || accounted != progress.total)) ||
        (status_.total && status_.total != progress.total) || progress.completed < status_.completed ||
        progress.already_available < status_.already_available || progress.failed < status_.failed ||
        progress.cancelled < status_.cancelled || (status_.finished && !progress.finished) ||
        (status_.checkpoint_saved && !progress.checkpoint_saved))
    {
        return false;
    }
    static_cast<TFXPreparationProgress&>(status_) = progress;
    if (!adapter_name.empty())
        status_.adapter_name = std::move(adapter_name);
    return true;
}

bool TFXPreparationJob::Finish(TFXPreparationState state, TFXPreparationError error)
{
    std::lock_guard lock(mutex_);
    if (!IsActiveState(status_.state))
        return false;
    switch (state)
    {
    case TFXPreparationState::Completed:
        if (status_.cancel_requested || !status_.finished || !status_.checkpoint_saved || !status_.total ||
            status_.failed || status_.cancelled || status_.in_flight || error != TFXPreparationError::None)
            return false;
        break;
    case TFXPreparationState::Cancelled:
        if (!status_.cancel_requested || (status_.total && (!status_.finished || status_.in_flight)))
            return false;
        break;
    case TFXPreparationState::Unsupported:
        if (status_.total || (error != TFXPreparationError::UnsupportedRenderer && error != TFXPreparationError::CacheDisabled))
            return false;
        break;
    case TFXPreparationState::Failed:
        if (error == TFXPreparationError::None || status_.in_flight)
            return false;
        break;
    default:
        return false;
    }
    status_.state = state;
    status_.error = error;
    return true;
}
}

#if defined(EMUCOREX_ENABLE_NATIVE_SELF_TESTS)
#include <cstdio>
#include <thread>
#include <vector>

extern "C" __attribute__((visibility("default"))) int EmuCoreXRunTFXPreparationJobTests(const char*)
{
    using namespace emucorex::android;
    using State = TFXPreparationState;
    using Error = TFXPreparationError;
    unsigned checks = 0, failures = 0;
    const auto check = [&](bool condition, const char* name) {
        ++checks;
        if (!condition) ++failures;
        std::printf("%s %s\n", condition ? "PASS" : "FAIL", name);
    };
    TFXPreparationJob job(7, 1);
    check(job.Snapshot().job_id == 7 && job.Snapshot().attempt == 1 &&
        job.Snapshot().state == State::WaitingForLifecycle, "job reserves lifecycle before worker admission");
    check(TFXPreparationJob::IsActiveState(State::WaitingForLifecycle) &&
        TFXPreparationJob::IsActiveState(State::Cancelling) &&
        !TFXPreparationJob::IsActiveState(State::Completed), "waiting and cancelling jobs exclude VM admission");
    check(!job.PublishStage(State::Running), "job rejects running before device setup");
    check(job.PublishStage(State::Starting), "job publishes setup only after lifecycle acquisition");
    check(!job.Finish(State::Completed), "job cannot complete before a finished pipeline batch");
    check(job.PublishStage(State::Running), "job accepts real batch startup");
    TFXPreparationProgress progress;
    progress.total = 164;
    progress.completed = 8;
    progress.in_flight = 3;
    check(job.PublishProgress(progress, "current adapter"), "job publishes actual compile progress");
    check(job.Snapshot().completed == 8 && job.Snapshot().in_flight == 3 &&
        job.Snapshot().adapter_name == "current adapter", "status copies counts and adapter coherently");
    progress.failed = 165;
    check(!job.PublishProgress(progress), "job rejects impossible progress accounting");
    check(job.RequestCancel() && job.CancellationRequested(), "cancellation is accepted without lifecycle lock");
    check(!job.RequestCancel(), "duplicate cancellation is not a second request");
    check(job.Snapshot().state == State::Cancelling && !job.PublishStage(State::Running),
        "worker stage cannot overwrite pending cancellation");
    progress.failed = 0;
    progress.in_flight = 0;
    progress.cancelled = 156;
    progress.finished = true;
    progress.checkpoint_saved = true;
    check(job.PublishProgress(progress), "cancelled batch keeps real completed work and checkpoint");
    check(job.Finish(State::Cancelled), "terminal cancellation follows lifecycle cleanup");
    check(!job.PublishProgress({}) && !job.PublishStage(State::Starting) &&
        !job.RequestCancel() && !job.Finish(State::Failed), "terminal job status is immutable");
    const auto final = job.Snapshot();
    check(final.completed == 8 && final.cancelled == 156 && final.checkpoint_saved &&
        final.cancel_requested && final.state == State::Cancelled, "terminal snapshot survives device deletion");
    TFXPreparationJob resumed(7, 2);
    check(resumed.Snapshot().total == 0 && resumed.Snapshot().attempt == 2 &&
        resumed.Snapshot().state == State::WaitingForLifecycle && !resumed.CancellationRequested(),
        "resume uses a fresh attempt without claiming driver cache hits");
    check(resumed.RequestCancel() && resumed.Finish(State::Cancelled), "cancellation before device creation completes");
    TFXPreparationJob unsupported(8, 1);
    check(unsupported.Finish(State::Unsupported, Error::CacheDisabled) &&
        unsupported.Snapshot().error == Error::CacheDisabled, "unsupported settings preserve a typed terminal reason");
    TFXPreparationJob complete(10, 1);
    check(complete.PublishStage(State::Starting) && complete.PublishStage(State::Running),
        "successful job enters the real worker phase");
    progress = {};
    progress.total = progress.completed = 164;
    progress.finished = true;
    check(complete.PublishProgress(progress) && !complete.Finish(State::Completed),
        "completed pipelines alone do not claim a durable checkpoint");
    progress.checkpoint_saved = true;
    check(complete.PublishProgress(progress) && complete.Finish(State::Completed),
        "successful job requires finished work and saved checkpoint");
    TFXPreparationJob failed(11, 1);
    check(failed.Finish(State::Failed, Error::DeviceCreationFailed) &&
        failed.Snapshot().error == Error::DeviceCreationFailed && !failed.Snapshot().checkpoint_saved,
        "creation failure cannot advertise a usable checkpoint");
    TFXPreparationJob concurrent(9, 1);
    std::atomic<unsigned> accepted{0};
    std::vector<std::thread> requesters;
    for (unsigned i = 0; i < 8; ++i)
        requesters.emplace_back([&]() { if (concurrent.RequestCancel()) ++accepted; });
    for (auto& thread : requesters) thread.join();
    check(accepted == 1 && concurrent.CancellationRequested(), "concurrent cancellation accepts exactly one requester");
    std::printf("RESULT checks=%u failures=%u\n", checks, failures);
    return failures ? 1 : 0;
}
#endif
