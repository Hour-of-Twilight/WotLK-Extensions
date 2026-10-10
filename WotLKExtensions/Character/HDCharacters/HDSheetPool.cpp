#include <Character/HDCharacters/HDSheetPool.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace HDCharacters::Pool
{
	namespace
	{
		constexpr int kMaxThreads = 6;

		struct Task
		{
			std::function<void()> fn;
			Group* group = nullptr;
		};

		struct State
		{
			std::mutex mutex;
			std::condition_variable wake;
			std::deque<Task> queue;
			int threadCount = 0;
		};

		State* g_state = nullptr;
		std::once_flag g_once;

		void Run(Task& task)
		{
			task.fn();
			if (task.group)
				task.group->remaining.fetch_sub(1, std::memory_order_acq_rel);
		}

		void Worker()
		{
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
			for (;;)
			{
				Task task;
				{
					std::unique_lock<std::mutex> lock(g_state->mutex);
					g_state->wake.wait(lock, []
					{
						return !g_state->queue.empty();
					});
					task = std::move(g_state->queue.front());
					g_state->queue.pop_front();
				}
				Run(task);
			}
		}

		void Start()
		{
			std::call_once(g_once, []
			{
				g_state = new State();
				const unsigned cores = std::thread::hardware_concurrency();
				g_state->threadCount = std::clamp<int>(cores > 2 ? (int)cores - 2 : 1, 1, kMaxThreads);
				for (int i = 0; i < g_state->threadCount; ++i)
					std::thread(Worker).detach();
			});
		}

		bool RunOne(Group* group)
		{
			Task task;
			{
				std::lock_guard<std::mutex> lock(g_state->mutex);
				if (g_state->queue.empty() || g_state->queue.front().group != group)
					return false;
				task = std::move(g_state->queue.front());
				g_state->queue.pop_front();
			}
			Run(task);
			return true;
		}
	}

	void Submit(std::function<void()> task, Group* group, bool front)
	{
		Start();
		if (group)
			group->remaining.fetch_add(1, std::memory_order_acq_rel);
		{
			std::lock_guard<std::mutex> lock(g_state->mutex);
			if (front)
				g_state->queue.push_front(Task{ std::move(task), group });
			else
				g_state->queue.push_back(Task{ std::move(task), group });
		}
		g_state->wake.notify_one();
	}

	void Wait(Group& group)
	{
		Start();
		while (group.remaining.load(std::memory_order_acquire) > 0)
		{
			if (!RunOne(&group))
				std::this_thread::yield();
		}
	}

	int ThreadCount()
	{
		Start();
		return g_state->threadCount;
	}
}
