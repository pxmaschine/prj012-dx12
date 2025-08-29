#pragma once

#include <CoreDefs.h>

// #include <atomic>

// #define CompletePreviousReadsBeforeFutureReads   std::atomic_signal_fence(std::memory_order_acquire)
// #define CompletePreviousWritesBeforeFutureWrites std::atomic_signal_fence(std::memory_order_release)

// inline u32 atomic_compare_exchange_u32(std::atomic<u32>* value, u32 new_val, u32 expected)
// {
//     u32 cur = expected;
//     value->compare_exchange_strong(cur, new_val,
//                                    std::memory_order_acq_rel,
//                                    std::memory_order_relaxed);
//     return cur;
// }

#include <Windows.h>
#include <Log.h>
#include <atomic>

struct JobQueue;
#define JOB_QUEUE_CALLBACK(name) void name(JobQueue* queue, void* data)
typedef JOB_QUEUE_CALLBACK(JobQueueCallback);

// typedef void job_queue_add_entry(JobQueue* queue, JobQueueCallback* callback, void* data);
// typedef void job_queue_complete_all_work(JobQueue* queue);

enum class JobPriority : u8
{
    High,
    Low,
};

constexpr u32 k_max_num_jobs = 256;  // must be power of two
constexpr u32 k_job_queue_mask = k_max_num_jobs - 1;

struct JobQueueEntry
{
    std::atomic<u32> seq;                   // sequence number (see algo below)
    JobQueueCallback* callback;
    void* data;
};

struct JobQueue
{
    std::atomic<u32> completion_goal{0};
    std::atomic<u32> completion_count{0};

    std::atomic<u32> head{0};              // consumer ticket counter
    std::atomic<u32> tail{0};              // producer ticket counter

    HANDLE semaphore_handle{};
    JobQueueEntry entries[k_max_num_jobs];
};

void win32_create_job_queue(JobQueue* queue, u32 thread_count);
void win32_add_job_entry(JobQueue* queue, JobQueueCallback* callback, void* data);
void win32_complete_all_jobs(JobQueue* queue);

// struct memory_arena
// {
//     u32 Size;
//     u8* Base;
//     u32 Used;
//     u32 TempCount;
// };

// struct temporary_memory
// {
//     memory_arena* Arena;
//     u32 Used;
// };

// struct task_with_memory
// {
//     std::atomic<bool> BeingUsed{false};
//     memory_arena Arena;
//     temporary_memory MemoryFlush;
// };

// struct transient_state
// {
//     bool IsInitialized;

//     memory_arena TranArena;
//     task_with_memory Tasks[4];

//     JobQueue *HighPriorityQueue;
//     JobQueue *LowPriorityQueue;
// };

// temporary_memory BeginTemporaryMemory(memory_arena *Arena);

// void EndTemporaryMemory(temporary_memory TempMem);

// task_with_memory* BeginTaskWithMemory(transient_state *TranState);

// void EndTaskWithMemory(task_with_memory *Task);

// enum asset_state
// {
//     AssetState_Unloaded,
//     AssetState_Queued,
//     AssetState_Loaded,
//     AssetState_StateMask = 0xFFF,

//     AssetState_Lock = 0x10000,
// };

// struct asset
// {
//     std::atomic<u32> State{AssetState_Unloaded}; // bits: state + lock
//     // asset_memory_header *Header;                    // plain pointer is OK here
//     // hha_asset HHA;
//     u32 FileIndex;
// };

// inline bool IsLocked(asset* Asset);

// inline u32 GetState(asset* Asset);
