#include "SentryBacktrace.hpp"

#if SENTRY_TARGET_PROFILING_SUPPORTED

#    include "SentryAsyncSafeLogging.h"
#    include "SentryCompiler.h"
#    include "SentryMachLogging.hpp"
#    include "SentryStackBounds.hpp"
#    include "SentryStackFrame.hpp"
#    include "SentryThreadHandle.hpp"
#    include "SentryThreadMetadataCache.hpp"
#    include "SentryThreadState.hpp"
#    include "SentryTime.h"

#    include <cassert>

using namespace sentry::profiling;
using namespace sentry::profiling::thread;

#    define LIKELY(x) __builtin_expect(!!(x), 1)
#    define UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace {
ALWAYS_INLINE bool
isValidFrame(std::uintptr_t frame, const StackBounds &bounds)
{
    return bounds.contains(frame) && StackFrame::isAligned(frame);
}

constexpr std::size_t kMaxBacktraceDepth = 128;

} // namespace

namespace sentry {
namespace profiling {
    NOT_TAIL_CALLED NEVER_INLINE std::size_t
    backtrace(const ThreadHandle &targetThread, const ThreadHandle &callingThread,
        std::uintptr_t *addresses, const StackBounds &bounds, bool *reachedEndOfStackPtr,
        std::size_t maxDepth, std::size_t skip) noexcept
    {
        assert(addresses != nullptr);
        if (UNLIKELY(maxDepth == 0 || !bounds.isValid())) {
            return 0;
        }
        std::size_t depth = 0;
        MachineContext machineContext;
        if (fillThreadState(targetThread.nativeHandle(), &machineContext) != KERN_SUCCESS) {
            SENTRY_LOG_ASYNC_SAFE_ERROR("Failed to fill thread state");
            return 0;
        }
        if (LIKELY(skip == 0)) {
            addresses[depth++] = getPreviousInstructionAddress(getProgramCounter(&machineContext));
        } else {
            skip--;
        }
        if (LIKELY(depth < maxDepth)) {
            const auto lr = getLinkRegister(&machineContext);
            if (isValidFrame(lr, bounds)) {
                if (LIKELY(skip == 0)) {
                    addresses[depth++] = getPreviousInstructionAddress(lr);
                } else {
                    skip--;
                }
            }
        }
        std::uintptr_t current;
        if (UNLIKELY(callingThread == targetThread)) {
            current = reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
        } else {
            current = getFrameAddress(&machineContext);
        }
        // Even if this bounds check passes, the frame pointer address could still be invalid if the
        // thread was suspended in an inconsistent state. The best we can do is to detect these
        // situations at symbolication time on the server and filter them out -- there's not an easy
        // architecture agnostic way to detect this on the client without a more complicated stack
        // unwinding implementation (e.g. DWARF)
        if (UNLIKELY(!isValidFrame(current, bounds))) {
            return 0;
        }
        bool reachedEndOfStack = false;
        while (depth < maxDepth) {
            const auto frame = reinterpret_cast<StackFrame *>(current);
            if (LIKELY(skip == 0)) {
                addresses[depth++] = getPreviousInstructionAddress(frame->returnAddress);
            } else {
                skip--;
            }
            const auto next = reinterpret_cast<std::uintptr_t>(frame->next);
            if (next > current && isValidFrame(next, bounds)) {
                current = next;
            } else {
                reachedEndOfStack = true;
                break;
            }
        }
        if (LIKELY(reachedEndOfStackPtr != nullptr)) {
            *reachedEndOfStackPtr = reachedEndOfStack;
        }
        return depth;
    }

    void
    enumerateBacktracesForAllThreads(const std::function<void(const Backtrace &)> &f,
        const std::shared_ptr<ThreadMetadataCache> &cache)
    {
        const auto pair = ThreadHandle::allExcludingCurrent();
        for (const auto &thread : pair.first) {
            if (thread->isIdle()) {
                continue;
            }
            Backtrace bt;
            auto metadata = cache->metadataForThread(*thread);
            if (metadata.threadID == 0) {
                continue;
            } else {
                bt.threadMetadata = std::move(metadata);
            }
            // This function calls `pthread_from_mach_thread_np`, which takes a lock,
            // so we must read the value before suspending the thread to avoid risking
            // a deadlock. See the comment below.
            const auto stackBounds = thread->stackBounds();

            // This one is probably safe to call while the thread is suspended, but
            // being conservative here in case the platform time functions take any
            // locks that we're not aware of.
            bt.absoluteTimestamp = getAbsoluteTime();

            // ############################################
            // DEADLOCK WARNING: It is not safe to call any functions that acquire a
            // lock between here and `thread->resume()` -- this may cause a deadlock.
            // Pay special attention to functions that may end up calling any of the
            // pthread_*_np functions, which typically take a lock used by other
            // OS APIs like GCD. You can see the full list of functions that take the
            // lock by going here and searching for `_pthread_list_lock:
            // https://github.com/apple/darwin-libpthread/blob/master/src/pthread.c
            // ############################################
            if (!thread->suspend()) {
                continue;
            }

            bool reachedEndOfStack = false;
            std::uintptr_t addresses[kMaxBacktraceDepth];
            const auto depth = backtrace(*thread, *pair.second, addresses, stackBounds,
                &reachedEndOfStack, kMaxBacktraceDepth, 0);

            thread->resume();
            // ############################################
            // END DEADLOCK WARNING
            // ############################################

            // Consider the backtraces only if we're able to collect the full stack
            if (reachedEndOfStack) {
                for (std::remove_const<decltype(depth)>::type i = 0; i < depth; i++) {
                    bt.addresses.push_back(addresses[i]);
                }
                f(bt);
            }
        }
    }

} // namespace profiling
} // namespace sentry

#endif
