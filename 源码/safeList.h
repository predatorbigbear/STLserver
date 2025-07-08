#pragma once

#include<forward_list>
#include "spinLock.h"


// safeList��ʵ����safe��ֻ��һ������
// ��Ϊ�ں��ڿ��������з��֣�Ҫ�ڲ����б�֤���ܣ�������Ҫ�ֶ�����mutex�ȽϺ���

//ʹ�ó�Ա����ǰ�Ƚ��м������ٽ��в����������ý���
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
		catch (const std::exception &e)
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

