#pragma once
#include<memory>
#include<forward_list>
#include<tuple>



template<typename MEMORYTYPE, unsigned int prepareSize = 1024>
struct MEMORYPOOL
{
	MEMORYPOOL() = default;


	MEMORYTYPE* getMemory(unsigned int getLen)
	{
		if (!getLen)
			return nullptr;
		if (!m_ptrMaxSize)
		{
			try
			{
				m_ptr.reset(new MEMORYTYPE[getLen]);
				m_ptrMaxSize = m_ptrNowSize = m_needSize = getLen;
				return m_ptr.get();
			}
			catch (const std::exception &e)
			{
				return nullptr;
			}
		}
		else
		{
			if (m_ptrNowSize + getLen < m_ptrMaxSize)
			{
				MEMORYTYPE* returnPtr{ m_ptr.get() + m_ptrNowSize };
				m_ptrNowSize += getLen;
				m_needSize += getLen;
				return returnPtr;
			}
			else
			{
				try
				{
					if (m_shadow.empty())
					{
						if (m_needSize + getLen < m_needSize || (m_needSize + getLen) < getLen)
							return nullptr;
						m_iter = m_shadow.emplace_after(m_shadow.cbefore_begin(), new MEMORYTYPE[getLen]);
						m_needSize += getLen;
					}
					else
					{
						if (m_needSize + getLen < m_needSize || (m_needSize + getLen) < getLen)
							return nullptr;
						m_iter = m_shadow.emplace_after(m_shadow.cbefore_begin(), new MEMORYTYPE[getLen]);
						m_needSize += getLen;
					}
					return m_iter->get();
				}
				catch (const std::exception &e)
				{
					return nullptr;
				}
			}
		}
	}



	void prepare()
	{
		if (!m_shadow.empty())
		{
			try
			{
				m_shadow.clear();
				m_ptr.reset(new MEMORYTYPE[m_needSize]);
				m_ptrMaxSize = m_needSize;
			}
			catch (const std::exception &e)
			{
				m_ptrMaxSize = 0;
			}
		}
		m_needSize = m_ptrNowSize = 0;
	}


	void reset()
	{
		m_ptr.reset();
		m_ptrMaxSize = m_ptrNowSize = m_needSize = 0;
		m_shadow.clear();
	}



private:
	std::unique_ptr<MEMORYTYPE[]> m_ptr{};

	unsigned int m_ptrMaxSize{};

	unsigned int m_ptrNowSize{};

	std::forward_list<std::unique_ptr<MEMORYTYPE[]>>m_shadow{};

	typedef typename std::forward_list<std::unique_ptr<MEMORYTYPE[]>>::iterator ITERATOR;

	ITERATOR m_iter;

	unsigned int m_needSize{};
};