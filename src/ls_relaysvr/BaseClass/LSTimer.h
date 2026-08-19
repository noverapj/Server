#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "UserDefine.h"

class LSPacketQueue;

template<typename T>
class LSTimer
{
public:
	LSTimer() = default;

	LSTimer(bool repeatState, int millSecond, T* buffer, DWORD repeatCount, LSPacketQueue* queue, OpMemPool* pool)
		: m_cancelled(false)
		, m_repeatState(repeatState)
		, m_millSecond(millSecond)
		, m_buffer(buffer)
		, m_repeatCount(repeatCount)
		, m_queue(queue)
		, m_pool(pool)
	{
		m_thread = std::thread(&LSTimer::Run, this);
		m_thread.detach();
	}

	virtual ~LSTimer(void)
	{
		Cancel();
	}

	void Cancel()
	{
		{
			std::lock_guard<std::mutex> lk(m_mutex);
			m_cancelled = true;
		}
		m_cv.notify_all();
	}

	void SetQueue(LSPacketQueue* queue)
	{
		m_queue = queue;
	}

private:
	void Run()
	{
		while (true)
		{
			std::unique_lock<std::mutex> lk(m_mutex);
			if (m_cv.wait_for(lk, std::chrono::milliseconds(m_millSecond),
				[this] { return m_cancelled; }))
				break;

			lk.unlock();
			try
			{
				CountTime countTime;
				SP2Packet st(Protocols::ITPK_OPERATIONTYPE);

				countTime.Start();
				st << *m_buffer;

				m_queue->InsertQueue(NULL, st, (PacketQueueTypes)PK_QUEUE_INTERNAL);

				if (m_repeatState)
				{
					if (m_repeatCount != INFINITE)
					{
						m_repeatCount--;
						if (m_repeatCount <= 0)
							break;
					}
					countTime.End();
				}
				else
				{
					break;
				}
			}
			catch (const std::exception& e)
			{
				LOG.PrintTimeAndLog(0, "LSTimer Error :%s", e.what());
			}
			lk.lock();
		}

		m_pool->Push(m_buffer);
		m_buffer = NULL;
		delete this;
	}

protected:
	std::thread m_thread;
	std::mutex m_mutex;
	std::condition_variable m_cv;
	bool m_cancelled;
	T* m_buffer;
	bool m_repeatState;
	int m_millSecond;
	LSPacketQueue* m_queue;
	OpMemPool* m_pool;
	DWORD m_repeatCount;
};
