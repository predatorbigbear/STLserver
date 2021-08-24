#pragma once

#include "publicHeader.h"



// safeList��ʵ����safe��ֻ��һ������
// ��Ϊ�ں��ڿ��������з��֣�Ҫ�ڲ����б�֤���ܣ�������Ҫ�ֶ�����mutex�ȽϺ���

template<typename T>
struct SAFELIST
{
	SAFELIST()
	{

	}


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
		m_listMutex.lock();
		if (m_list.empty())
		{
			m_listMutex.unlock();
			return true;
		}
		m_listMutex.unlock();
		return false;
	}


	std::forward_list<T>& listSelf()
	{
		return m_list;
	}

	std::mutex& getMutex()
	{
		return m_listMutex;
	}

	typename std::forward_list<T>::iterator& iter()
	{
		return m_iter;
	}

private:
	std::forward_list<T> m_list;
	typename std::forward_list<T>::iterator m_iter;
	std::mutex m_listMutex;	
};

