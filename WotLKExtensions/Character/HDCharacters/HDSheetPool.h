#pragma once

#include <atomic>
#include <functional>

namespace HDCharacters::Pool
{
	struct Group
	{
		std::atomic<int> remaining{ 0 };
	};

	void Submit(std::function<void()> task, Group* group, bool front);
	void Wait(Group& group);
	int ThreadCount();
}
