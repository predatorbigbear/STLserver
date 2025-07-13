#pragma once
#include<memory>
#include<functional>
#include<type_traits>
#include<new>

//与以往的内存池不同的设计：
//每次http接口请求读取时记录下累计实际所需要的内存大小，
//优先从m_ptr申请内存块，不足的从后备内存区m_shadow获取（实时分配，实时分配的性能可通过tmalloc或jmalloc等第三方内存池进一步提速）
//在接口结果发送之后，调用prepare处理
// prepare处理是检查累计实际所需要的内存大小是否大于m_ptr内存块的大小，大于则将m_shadow释放
// 如果大于则进行一次重新分配，否则只需要将计数归0
//这样在若干次不同的接口请求后，确保m_ptr内的内存可以满足绝大部分甚至全部的内存需求，大幅度提升申请内存块的效率和性能
//如果用共享内存池，首先有加锁解锁消耗，其次申请内存块效率没有这样实现简单高效，而且prepare的过程几乎无法实现
//因为共享内存池同一时间有N个socket连接在使用，所以要找到时机进行重分配以满足累计实际所需要的内存大小以及释放多余内存块是非常困难的


// 基础模板：非指针类型直接返回
template<typename T>
struct remove_all_pointers
{
	using type = T;
};

// 偏特化：处理指针类型（递归拆解）
template<typename T>
struct remove_all_pointers<T*> 
{
	using type = typename remove_all_pointers<T>::type;
};



template<typename T>
using remove_all_pointers_t = typename remove_all_pointers<T>::type;


template<unsigned int prepareSize = 1024>
struct MEMORYPOOL
{
	MEMORYPOOL() = default;


	template<typename T>
	T getMemory(unsigned int getLen)
	{
		//判断是否是指针，并且是否不为void*类型
		static_assert(std::is_pointer_v<T>,
			"T must be a  pointer type");

		//获取原始指针中的原始类型
		using RawType = std::remove_pointer_t<T>;

		if (!getLen)
			return nullptr;

		//设置最大内存申请值，每次获取时进行对比
		//优先从m_ptr获取适当内存，反之从后备空间m_shadow获取
		static constexpr unsigned int maxMemorySize{ 2048 * 2048 };

		//计算本次需要分配的内存容量大小
		unsigned int thisGetLen{};

		//递归拆解T类型，判断最终类型是不是void
		if constexpr (!std::is_same_v<remove_all_pointers_t<T>, void>)
		{
			thisGetLen = getLen * sizeof(RawType);
		}
		else
		{
			thisGetLen = getLen * sizeof(char*);
		}

		//如果最大可分配容量减去当前已分配大小
		if (maxMemorySize - m_needSize < thisGetLen)
			return nullptr;

		if (!m_ptrMaxSize)
		{
			try
			{
				m_ptr.reset(new char[thisGetLen]);
				//累计实际所需要的内存大小
				m_ptrMaxSize = m_ptrNowSize = m_needSize = thisGetLen;
				return reinterpret_cast<T>(m_ptr.get());
			}
			catch (const std::bad_alloc& e)
			{
				m_ptrMaxSize = m_ptrNowSize = m_needSize = 0;
				return nullptr;
			}
		}
		else
		{
			if (m_ptrNowSize + thisGetLen < m_ptrMaxSize)
			{
				T returnPtr{ reinterpret_cast<T>(m_ptr.get() + m_ptrNowSize) };
				m_ptrNowSize += thisGetLen;
				//累计实际所需要的内存大小
				m_needSize += thisGetLen;
				return returnPtr;
			}
			else
			{
				try
				{
					//判断是否超越了unsigned int最大值
					if (m_needSize + thisGetLen < m_needSize || (m_needSize + thisGetLen) < thisGetLen 
						|| !m_shadow || m_shadowEnd == m_shadow.get() + prepareSize)
						return nullptr;
					
					*m_shadowEnd++ = new char[thisGetLen];
					
					//累计实际所需要的内存大小
					m_needSize += thisGetLen;
					return reinterpret_cast<T>(*(m_shadowEnd-1));
				}
				catch (const std::bad_alloc& e)
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
		if (m_needSize > m_ptrMaxSize)
		{
			do
			{
				delete[] * m_shadowBegin;
			} while (++m_shadowBegin != m_shadowEnd);
			m_shadowBegin = m_shadowEnd = m_shadow.get();

			try
			{
				m_ptr.reset(new char[m_needSize]);
				m_ptrMaxSize = m_needSize;
			}
			catch (const std::bad_alloc& e)
			{
				m_ptrMaxSize = 0;
			}
		}
		
		m_needSize = m_ptrNowSize = 0;
	}



	//重置所有资源为初始状态，在socket回收时调用
	void reset()
	{
		if (!m_ptr)
		{
			try
			{
				m_ptr.reset(new char[prepareSize]);
				m_ptrMaxSize = prepareSize;
			}
			catch (std::bad_alloc& e)
			{
				m_ptrMaxSize = 0;
			}
		}
		if (!m_shadow)
		{
			try
			{
				m_shadow.reset(new char*[prepareSize]);
				m_shadowBegin = m_shadowEnd = m_shadow.get();
			}
			catch (std::bad_alloc& e)
			{

			}
		}
		
		m_ptrNowSize = m_needSize = 0;
		
	}



private:
	//因为C++规定 char类型在任意平台大小都是1字节，所以改用char类型就行，不用void了
	std::unique_ptr<char[]> m_ptr{};    //首选内存块空间

	unsigned int m_ptrMaxSize{ };              //当前内存块申请到的空间大小

	unsigned int m_ptrNowSize{};              //当前内存块已经分配出去的内存空间大小

	std::unique_ptr<char*[]> m_shadow{};             //后备内存分配区

	char** m_shadowBegin{};                          //后备内存分配区起始位置

	char** m_shadowEnd{};                            //后备内存分配区结束位置
	

	unsigned int m_needSize{};                //累计实际所需要的内存大小
};