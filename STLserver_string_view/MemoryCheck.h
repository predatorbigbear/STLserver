#pragma once


template<typename T>
struct MEMORYCHECK
{
	MEMORYCHECK() = default;


	MEMORYCHECK(T *begin, T *end):m_begin(begin),m_end(end)
	{
		if (!m_begin || !m_end || m_begin > m_end)
			*m_ptr = '1';
	}


	void set(T *begin, T *end)
	{
		if (!begin || !end || begin > end)
			*m_ptr = '1';
		m_begin = begin;
		m_end = end;
	}


	void check(T *ptr)
	{
		if (!ptr || (ptr < m_begin || ptr >= m_end))
			*m_ptr = '1';
	}


	void check(T *begin,T *end)
	{
		if (!begin || !end || begin > end)
			*m_ptr = '1';
		while (begin != end)
		{
			if (!begin || (begin < m_begin || begin >= m_end))
				*m_ptr = '1';
			++begin;
		}
	}


private:
	T *m_begin{};
	T *m_end{};
	T *m_ptr{ nullptr };
};