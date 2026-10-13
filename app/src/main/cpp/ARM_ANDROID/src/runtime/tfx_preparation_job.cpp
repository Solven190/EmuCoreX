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

bool TFXPreparationJob::CommitCompleted(const std::function<bool()>& persist_receipt)
{
    std::lock_guard lock(mutex_);
    if (!IsActiveState(status_.state))
        return false;

    if (status_.cancel_requested)
	{
        if (status_.finished && status_.in_flight == 0)
        {
            status_.state = TFXPreparationState::Cancelled;
            status_.error = TFXPreparationError::None;
        }
        return false;
	}

    const uint64_t accounted = uint64_t(status_.completed) + status_.already_available +
        status_.failed + status_.cancelled + status_.in_flight;
    if (status_.state != TFXPreparationState::Running || !status_.finished || !status_.checkpoint_saved ||
        !status_.total || accounted != status_.total || status_.failed || status_.cancelled || status_.in_flight)
    {
        return false;
    }

    bool receipt_saved = false;
    try
    {
        receipt_saved = persist_receipt && persist_receipt();
    }
    catch (...)
    {
        receipt_saved = false;
    }

    if (!receipt_saved)
    {
        status_.checkpoint_saved = false;
        status_.state = TFXPreparationState::Failed;
        status_.error = TFXPreparationError::CheckpointFailed;
        return false;
    }

    status_.state = TFXPreparationState::Completed;
    status_.error = TFXPreparationError::None;
    return true;
}
}

#if defined(EMUCOREX_ENABLE_NATIVE_SELF_TESTS)
#include <cstdio>
#include <condition_variable>
#include <mutex>
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

    const auto prepare_commit_candidate = [](TFXPreparationJob& candidate, uint32_t total = 4,
        uint32_t completed = 4, bool checkpoint_saved = true, bool finished = true) {
        return candidate.PublishStage(State::Starting) && candidate.PublishStage(State::Running) &&
            [&]() {
                TFXPreparationProgress value;
                value.total = total;
                value.completed = completed;
                value.finished = finished;
                value.checkpoint_saved = checkpoint_saved;
                return candidate.PublishProgress(value);
            }();
    };

    TFXPreparationJob commit_success(12, 1);
    unsigned receipt_writes = 0;
    const bool success_candidate_ready = prepare_commit_candidate(commit_success);
    const bool committed = commit_success.CommitCompleted([&]() {
        ++receipt_writes;
        return true;
    });
    check(success_candidate_ready && committed && receipt_writes == 1 &&
        commit_success.Snapshot().state == State::Completed &&
        commit_success.Snapshot().error == Error::None,
        "completed state is published only after receipt persistence succeeds");
    check(!commit_success.CommitCompleted([&]() { ++receipt_writes; return true; }) && receipt_writes == 1,
        "terminal completion cannot persist a second receipt");

    TFXPreparationJob commit_write_failed(13, 1);
    unsigned failed_receipt_writes = 0;
    const bool failed_candidate_ready = prepare_commit_candidate(commit_write_failed);
    const bool failed_commit = commit_write_failed.CommitCompleted([&]() {
        ++failed_receipt_writes;
        return false;
    });
    check(failed_candidate_ready && !failed_commit && failed_receipt_writes == 1 &&
        commit_write_failed.Snapshot().state == State::Failed &&
        commit_write_failed.Snapshot().error == Error::CheckpointFailed &&
        !commit_write_failed.Snapshot().checkpoint_saved,
        "receipt write failure publishes typed checkpoint failure");

    TFXPreparationJob commit_write_threw(14, 1);
    unsigned thrown_receipt_writes = 0;
    const bool threw_candidate_ready = prepare_commit_candidate(commit_write_threw);
    const bool threw_commit = commit_write_threw.CommitCompleted([&]() -> bool {
        ++thrown_receipt_writes;
        throw 1;
    });
    check(threw_candidate_ready && !threw_commit && thrown_receipt_writes == 1 &&
        commit_write_threw.Snapshot().state == State::Failed &&
        commit_write_threw.Snapshot().error == Error::CheckpointFailed &&
        !commit_write_threw.Snapshot().checkpoint_saved,
        "receipt writer exception publishes typed checkpoint failure");

    TFXPreparationJob commit_cancelled(15, 1);
    unsigned cancelled_receipt_writes = 0;
    const bool cancelled_candidate_ready = prepare_commit_candidate(commit_cancelled);
    const bool accepted_cancel = commit_cancelled.RequestCancel();
    const bool cancelled_commit = commit_cancelled.CommitCompleted([&]() {
        ++cancelled_receipt_writes;
        return true;
    });
    check(cancelled_candidate_ready && accepted_cancel && !cancelled_commit &&
        cancelled_receipt_writes == 0 && commit_cancelled.Snapshot().state == State::Cancelled,
        "accepted cancellation before completion claim writes no receipt");

    TFXPreparationJob partial_commit(16, 1);
    unsigned partial_receipt_writes = 0;
    const bool partial_candidate_ready = prepare_commit_candidate(partial_commit, 4, 3, true, false);
    const bool partial_committed = partial_commit.CommitCompleted([&]() {
        ++partial_receipt_writes;
        return true;
    });
    check(partial_candidate_ready && !partial_committed && partial_receipt_writes == 0 &&
        partial_commit.Snapshot().state == State::Running,
        "partial batch cannot invoke receipt writer");

    TFXPreparationJob failed_batch_commit(18, 1);
    unsigned failed_batch_receipt_writes = 0;
    const bool failed_batch_started = failed_batch_commit.PublishStage(State::Starting) &&
        failed_batch_commit.PublishStage(State::Running);
    TFXPreparationProgress failed_batch_progress;
    failed_batch_progress.total = 4;
    failed_batch_progress.completed = 3;
    failed_batch_progress.failed = 1;
    failed_batch_progress.finished = true;
    failed_batch_progress.checkpoint_saved = true;
    const bool failed_batch_published = failed_batch_commit.PublishProgress(failed_batch_progress);
    const bool failed_batch_claimed = failed_batch_commit.CommitCompleted([&]() {
        ++failed_batch_receipt_writes;
        return true;
    });
    check(failed_batch_started && failed_batch_published && !failed_batch_claimed &&
        failed_batch_receipt_writes == 0 && failed_batch_commit.Snapshot().state == State::Running,
        "pipeline failures cannot invoke receipt writer");

    TFXPreparationJob no_checkpoint_commit(19, 1);
    unsigned no_checkpoint_receipt_writes = 0;
    TFXPreparationProgress no_checkpoint_progress;
    no_checkpoint_progress.total = no_checkpoint_progress.completed = 4;
    no_checkpoint_progress.finished = true;
    const bool no_checkpoint_started = no_checkpoint_commit.PublishStage(State::Starting) &&
        no_checkpoint_commit.PublishStage(State::Running);
    const bool no_checkpoint_published = no_checkpoint_commit.PublishProgress(no_checkpoint_progress);
    const bool no_checkpoint_claimed = no_checkpoint_commit.CommitCompleted([&]() {
        ++no_checkpoint_receipt_writes;
        return true;
    });
    check(no_checkpoint_started && no_checkpoint_published && !no_checkpoint_claimed &&
        no_checkpoint_receipt_writes == 0 && no_checkpoint_commit.Snapshot().state == State::Running,
        "missing checkpoint cannot invoke receipt writer");

    TFXPreparationJob commit_race(17, 1);
    const bool race_candidate_ready = prepare_commit_candidate(commit_race);
    std::mutex barrier_mutex;
    std::condition_variable barrier_cv;
    bool writer_entered = false;
    bool release_writer = false;
    bool cancel_attempting = false;
    bool cancel_finished = false;
    bool cancel_was_accepted = true;
    bool race_committed = false;
    std::thread committing_thread([&]() {
        race_committed = commit_race.CommitCompleted([&]() {
            std::unique_lock lock(barrier_mutex);
            writer_entered = true;
            barrier_cv.notify_all();
            barrier_cv.wait(lock, [&]() { return release_writer; });
            return true;
        });
    });
    {
        std::unique_lock lock(barrier_mutex);
        barrier_cv.wait(lock, [&]() { return writer_entered; });
    }
    std::thread cancelling_thread([&]() {
        {
            std::lock_guard lock(barrier_mutex);
            cancel_attempting = true;
            barrier_cv.notify_all();
        }
        cancel_was_accepted = commit_race.RequestCancel();
        {
            std::lock_guard lock(barrier_mutex);
            cancel_finished = true;
            barrier_cv.notify_all();
        }
    });
    {
        std::unique_lock lock(barrier_mutex);
        barrier_cv.wait(lock, [&]() { return cancel_attempting; });
        release_writer = true;
        barrier_cv.notify_all();
    }
    committing_thread.join();
    cancelling_thread.join();
    check(race_candidate_ready && race_committed && cancel_finished && !cancel_was_accepted &&
        commit_race.Snapshot().state == State::Completed,
        "cancellation arriving during durable commit cannot split completion from receipt");

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
