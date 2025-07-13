#pragma once

#include<forward_list>
#include<stdexcept>
#include<exception>
#include "spinLock.h"


// safeList其实并不safe，只是一个名字
// 因为在后期开发过程中发现，要在操作中保证性能，还是需要手动操作mutex比较合适

//使用成员函数前先进行加锁，再进行操作，最后调用解锁
template<typename T>
struct SAFELIST
{
	SAFELIST() = default;


	bool unsafeInsert(T buffer)
	{
		try
		{
			if (m_list.empty())
				m_iter = m_list.emplace_after(m_list.before_begin(), buffer);
			else
				m_iter = m_list.emplace_after(m_iter, buffer);
			return true;
		}
		catch (const std::bad_alloc &e)
		{
			return false;
		}
	}

	void unsafePop()
	{
		if(!m_list.empty())
			m_list.pop_front();
	}


	void unsafeClear()
	{
		m_list.clear();
	}


	bool empty()
	{
		return m_list.empty();
	}


	std::forward_list<T>& listSelf()
	{
		return m_list;
	}

	void lock()
	{
		m_listMutex.lock();
	}

	void unlock()
	{
		m_listMutex.unlock();
	}


	typename std::forward_list<T>::iterator& iter()
	{
		return m_iter;
	}

private:
	std::forward_list<T> m_list;
	typename std::forward_list<T>::iterator m_iter;
	SpinLock m_listMutex;
};

