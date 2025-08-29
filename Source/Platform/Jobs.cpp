#include <Platform/Jobs.h>

#include <Platform/PlatformContext.h>

#ifdef ZV_COMPILER_CL
#include <immintrin.h> // for _mm_pause on MSVC (optional micro-wait)
#endif

namespace
{
    inline void cpu_relax()
    {
#ifdef ZV_COMPILER_CL
        _mm_pause();
#else
        // TODO: Implement for other compilers
#endif
    }

    // Pop exactly one job (assumes caller has already observed "one job exists",
    // e.g., by waiting on the semaphore).
    inline void win32_pop_and_execute(JobQueue* q)
    {
        const u32 pos = q->head.fetch_add(1, std::memory_order_acq_rel);
        JobQueueEntry* cell = &q->entries[pos & k_job_queue_mask];

        // Wait until the producer publishes this slot (seq == pos + 1)
        u32 expected = pos + 1;
        while (cell->seq.load(std::memory_order_acquire) != expected)
            cpu_relax();

        // Read payload
        JobQueueCallback* cb = cell->callback;
        void* data = cell->data;

        // Mark slot as free for a future producer: seq = pos + JOBQ_CAP
        cell->seq.store(pos + k_max_num_jobs, std::memory_order_release);

        // Do the work and publish completion
        cb(q, data);
        q->completion_count.fetch_add(1, std::memory_order_release);
    }

    DWORD WINAPI thread_proc(LPVOID lpParameter)
    {
        JobQueue* q = (JobQueue*)lpParameter;
        for (;;)
        {
            WaitForSingleObjectEx(q->semaphore_handle, INFINITE, FALSE);
            win32_pop_and_execute(q);
        }
    }
}

void win32_create_job_queue(JobQueue* queue, u32 thread_count)
{
    queue->completion_goal.store(0, std::memory_order_relaxed);
    queue->completion_count.store(0, std::memory_order_relaxed);
    queue->head.store(0, std::memory_order_relaxed);
    queue->tail.store(0, std::memory_order_relaxed);

    // Initialize per-slot sequences: empty slot i has seq == i
    for (u32 i = 0; i < k_max_num_jobs; ++i)
    {
        queue->entries[i].seq.store(i, std::memory_order_relaxed);
        queue->entries[i].callback = nullptr;
        queue->entries[i].data     = nullptr;
    }

    // Max count must be able to cover queued jobs
    queue->semaphore_handle = CreateSemaphoreExA(
        /*lpSemaphoreAttributes*/ nullptr,
        /*lInitialCount*/ 0,
        /*lMaximumCount*/ k_max_num_jobs,
        /*lpName*/ nullptr,
        /*dwFlags*/ 0,
        /*dwDesiredAccess*/ SEMAPHORE_ALL_ACCESS);

    for (u32 thread_index = 0; thread_index < thread_count; ++thread_index)
    {
        DWORD thread_id;
        HANDLE h = CreateThread(nullptr, 0, thread_proc, queue, 0, &thread_id);
        CloseHandle(h);
    }
}

void win32_add_job_entry(JobQueue* queue, JobQueueCallback* callback, void* data)
{
    // Reserve a ticket (unique position)
    const u32 pos = queue->tail.fetch_add(1, std::memory_order_acq_rel);
    JobQueueEntry* cell = &queue->entries[pos & k_job_queue_mask];

    // Wait for the slot to become empty for this position
    while (cell->seq.load(std::memory_order_acquire) != pos)
    {
        cpu_relax();
    }

    // Write payload (plain stores are fine, publication happens via seq.store release)
    cell->callback = callback;
    cell->data     = data;

    // Bump goal before making the job visible is fine (relaxed)
    queue->completion_goal.fetch_add(1, std::memory_order_relaxed);

    // Publish: make the slot visible to consumers of position 'pos'
    cell->seq.store(pos + 1, std::memory_order_release);

    // Wake exactly one worker
    ReleaseSemaphore(queue->semaphore_handle, 1, nullptr);
}

void win32_complete_all_jobs(JobQueue* queue)
{
    while (queue->completion_count.load(std::memory_order_acquire) !=
           queue->completion_goal.load(std::memory_order_relaxed))
    {
        // Opportunistically help (optional: you could also Wait+pop here)
        DWORD r = WaitForSingleObjectEx(queue->semaphore_handle, 0, FALSE);
        if (r == WAIT_OBJECT_0)
        {
            win32_pop_and_execute(queue);
        }
        else
        {
            cpu_relax();
        }
    }

    queue->completion_goal.store(0, std::memory_order_relaxed);
    queue->completion_count.store(0, std::memory_order_relaxed);
}

#if 0
temporary_memory BeginTemporaryMemory(memory_arena *Arena)
{
    temporary_memory result;

    result.Arena = Arena;
    result.Used = Arena->Used;

    ++Arena->TempCount;

    return(result);
}

void EndTemporaryMemory(temporary_memory TempMem)
{
    memory_arena *Arena = TempMem.Arena;
    zv_assert(Arena->Used >= TempMem.Used);
    Arena->Used = TempMem.Used;
    zv_assert(Arena->TempCount > 0);
    --Arena->TempCount;
}

task_with_memory* BeginTaskWithMemory(transient_state *TranState)
{
    for (u32 TaskIndex = 0; TaskIndex < ArrayCount(TranState->Tasks); ++TaskIndex)
    {
        task_with_memory *Task = TranState->Tasks + TaskIndex;

        bool expected = false;
        if (Task->BeingUsed.compare_exchange_strong(expected, true,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_relaxed))
        {
            Task->MemoryFlush = BeginTemporaryMemory(&Task->Arena);
            return Task;
        }
    }
    return 0;
}

void EndTaskWithMemory(task_with_memory *Task)
{
    EndTemporaryMemory(Task->MemoryFlush);
    // Release: make the work done in this task visible to someone who acquires.
    Task->BeingUsed.store(false, std::memory_order_release);
}

inline bool IsLocked(asset* Asset)
{
    return (Asset->State.load(std::memory_order_relaxed) & AssetState_Lock) != 0;
}

inline u32 GetState(asset* Asset)
{
    // Acquire so that, if we observe "Loaded", all prior writes (file IO etc.)
    // done by the loader thread are now visible.
    return Asset->State.load(std::memory_order_acquire) & AssetState_StateMask;
}
#endif 
