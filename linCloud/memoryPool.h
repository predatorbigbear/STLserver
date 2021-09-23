#pragma once
#include<memory>
#include<forward_list>


//与以往的内存池不同的设计：
//每次http接口请求读取时记录下累计实际所需要的内存大小，
//优先从m_ptr申请内存块，不足的从后备内存区m_shadow获取（实时分配，实时分配的性能可通过tmalloc或jmalloc等第三方内存池进一步提速）
//在接口结果发送之后，调用prepare处理
//这样在若干次不同的接口请求后，确保m_ptr内的内存可以满足绝大部分甚至全部的内存需求，大幅度提升申请内存块的效率和性能


template<typename MEMORYTYPE, unsigned int prepareSize = 1024>
struct MEMORYPOOL
{
	MEMORYPOOL() = default;


	MEMORYTYPE* getMemory(unsigned int getLen)
	{
		if (!getLen)
			return nullptr;
		//设置最大内存申请值，每次获取时进行对比
		//优先从m_ptr获取适当内存，反之从后备空间m_shadow获取
		static constexpr unsigned int maxMemorySize{ 100 * 1024 };
		if (maxMemorySize - m_needSize < getLen)
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
					if (m_needSize + getLen < m_needSize || (m_needSize + getLen) < getLen)
						return nullptr;
					m_iter = m_shadow.emplace_after(m_iter, new MEMORYTYPE[getLen]);
					m_needSize += getLen;
					return m_iter->get();
				}
				catch (const std::exception &e)
				{
					return nullptr;
				}
			}
		}
	}


	//在接口结果发送之后使用
	//如果后备内存块非空，则尝试将首选内存块扩大到累计实际所需要的内存大小
	void prepare()
	{
		if (!m_shadow.empty())
		{
			try
			{
				m_shadow.clear();
				m_ptr.reset(new MEMORYTYPE[m_needSize]);
				m_ptrMaxSize = m_needSize;
				m_iter = m_shadow.before_begin();
			}
			catch (const std::exception &e)
			{
				m_ptrMaxSize = 0;
			}
		}
		m_needSize = m_ptrNowSize = 0;
	}


	//重置所有资源为初始状态
	void reset()
	{
		m_ptr.reset();
		m_ptrMaxSize = m_ptrNowSize = m_needSize = 0;
		m_shadow.clear();
		m_iter = m_shadow.before_begin();
	}



private:
	std::unique_ptr<MEMORYTYPE[]> m_ptr{};    //首选内存块空间

	unsigned int m_ptrMaxSize{};              //当前内存块申请到的空间大小
	 
	unsigned int m_ptrNowSize{};              //当前内存块已经分配出去的内存空间大小

	std::forward_list<std::unique_ptr<MEMORYTYPE[]>>m_shadow{};                                  //后备内存块空间

	typedef typename std::forward_list<std::unique_ptr<MEMORYTYPE[]>>::iterator ITERATOR;

	ITERATOR m_iter{ m_shadow.before_begin() };                                                 //后备内存块空间插入迭代器                                          

	unsigned int m_needSize{};                //累计实际所需要的内存大小
};