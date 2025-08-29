#include <Platform/Jobs.h>

namespace
{
    bool win32_do_next_job_entry(JobQueue* queue)
    {
        bool should_sleep = false;
    
        u32 r   = queue->next_entry_to_read.load(std::memory_order_relaxed);
        u32 w   = queue->next_entry_to_write.load(std::memory_order_acquire);
        u32 nr  = (r + 1) % ArrayCount(queue->entries);
    
        if (r != w)
        {
            // Try to claim this slot. On success, we "acquire" the published entry.
            if (queue->next_entry_to_read.compare_exchange_weak(
                    r, nr,
                    std::memory_order_acquire,   // success
                    std::memory_order_relaxed))  // failure
            {
                JobQueueEntry entry = queue->entries[r];
                entry.callback(queue, entry.data);
    
                // Publish “work done”: release makes prior writes visible to waiters.
                queue->completion_count.fetch_add(1, std::memory_order_release);
            }
            // else: another worker grabbed it; just fall through and return (no sleep)
        }
        else
        {
            should_sleep = true;
        }
    
        return should_sleep;
    }

    DWORD WINAPI thread_proc(LPVOID lpParameter)
    {
        JobQueue* queue = (JobQueue*)lpParameter;

        for(;;)
        {
            if (win32_do_next_job_entry(queue))
            {
                WaitForSingleObjectEx(queue->semaphore_handle, INFINITE, FALSE);
            }
        }
    }
}

void win32_create_job_queue(JobQueue* queue, u32 thread_count)
{
    queue->completion_goal.store(0);
    queue->completion_count.store(0);
    queue->next_entry_to_write.store(0);
    queue->next_entry_to_read.store(0);

    u32 initial_count = 0;
    queue->semaphore_handle = CreateSemaphoreEx(0, initial_count, thread_count, 0, 0, SEMAPHORE_ALL_ACCESS);

    for (u32 thread_index = 0; thread_index < thread_count; ++thread_index)
    {
        DWORD thread_id;
        HANDLE thread_handle = CreateThread(0, 0, thread_proc, queue, 0, &thread_id);
        CloseHandle(thread_handle);
    }
}

void win32_add_job_entry(JobQueue* queue, JobQueueCallback* callback, void* data)
{
    u32 w  = queue->next_entry_to_write.load(std::memory_order_relaxed);
    u32 nw = (w + 1) % ArrayCount(queue->entries);

    // Acquire pairs with the consumer's release of NextEntryToRead;
    // also prevents overwriting unread entries.
    zv_assert(nw != queue->next_entry_to_read.load(std::memory_order_acquire));

    JobQueueEntry* entry = queue->entries + w;
    entry->callback = callback;
    entry->data     = data;

    // Count isn't used to publish data, so relaxed is fine here.
    queue->completion_goal.fetch_add(1, std::memory_order_relaxed);

    // Publish the slot: everything above becomes visible to consumers.
    queue->next_entry_to_write.store(nw, std::memory_order_release);

    ReleaseSemaphore(queue->semaphore_handle, 1, 0);
}

void win32_complete_all_jobs(JobQueue* queue)
{
    // Acquire on count observes workers' release increments.
    while (queue->completion_count.load(std::memory_order_acquire) !=
           queue->completion_goal.load(std::memory_order_relaxed))
    {
        win32_do_next_job_entry(queue);
    }

    // These resets are just bookkeeping.
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
