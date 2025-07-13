#pragma once

#include<type_traits>
#include<memory>
#include "spinLock.h"
#include<iostream>


template<typename T>
struct is_shared_ptr : std::false_type {};


template<typename T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};


template<typename T>
struct is_shared_ptr<const std::shared_ptr<T>> : std::true_type {};


template<typename T>
inline constexpr bool is_shared_ptr_v = is_shared_ptr<T>::value;


// safeList其实并不safe，只是一个名字
// 因为在后期开发过程中发现，要在操作中保证性能，还是需要手动操作mutex比较合适

//使用成员函数前先进行加锁，再进行操作，最后调用解锁

//根据SAFELIST进行改进，用于redis查询的高速list,进一步缩短加锁时长
//unsafeInsert与unsafePop均不会增删节点，比SAFELIST更高效
//仅使用指针移动实现模拟SAFELIST的插入 删除操作
template<typename T, unsigned int listSize = 256>
struct FASTSAFELIST
{
	FASTSAFELIST()
	{
		m_list.reset(new T[listSize]);
		m_begin = m_list.get();
		//不能指向m_list.get() + listSize，因为会对m_maxSize位置取值和赋值
		m_maxSize = m_list.get() + listSize - 1;
		m_end = m_begin;
	}


	void reset(unsigned int bufNum)
	{
		if (bufNum > listSize)
		{
			m_list.reset(new T[bufNum]);
			m_begin = m_list.get();
			//不能指向m_list.get() + bufNum，因为会对m_maxSize位置取值和赋值
			m_maxSize = m_list.get() + bufNum - 1;
			m_end = m_begin;
		}
	}


	bool unsafeInsert(T &buffer)
	{	
		if (empty)
		{		
			*m_end++ = buffer;	
			empty = false;
			return true;
		}
		else
		{
			if ((m_begin == m_list.get() && m_end == m_maxSize) || std::distance(m_end, m_begin) == 1)
				return false;
			if (std::distance(m_begin, m_end) > 0)
			{
				if (m_end < m_maxSize)
				{
					*m_end++ = buffer;
					return true;
				}
				else
				{
					*m_end = buffer;
					m_end = m_list.get();
					return true;
				}
			}
			else
			{
				*m_end++ = buffer;
				return true;
			}
		}
	}

	void unsafePop(T & temp)
	{
		if (empty)
		{
			temp = nullptr;
			return;
		}
		else
		{
			if (std::distance(m_begin, m_end) > 0)
			{
				if constexpr (is_shared_ptr_v<T>)
				{
					//对于共享指针，使用右值传递避免内部引用计数改变，同时将m_begin内的值置为nullptr
					temp = std::move(*m_begin++);
				}
				else
				{
					temp = *m_begin++;
				}
				if (m_begin == m_end)
				{
					empty = true;
					m_begin = m_end = m_list.get();
				}
				return;
			}
			else
			{		
				if constexpr (is_shared_ptr_v<T>)
				{
					//对于共享指针，使用右值传递避免内部引用计数改变，同时将m_begin内的值置为nullptr
					temp = std::move(*m_begin);
				}
				else
				{
					temp = *m_begin;
				}
				if (m_begin == m_maxSize)
				{
					m_begin = m_list.get();
					if (m_begin == m_end)
					{
						empty = true;
					}
				}
				else
					++m_begin;
				return;
			}
		}
	}

	bool isEmpty()
	{
		return empty;
	}


	void lock()
	{
		m_listMutex.lock();
	}

	void unlock()
	{
		m_listMutex.unlock();
	}


private:
	std::unique_ptr<T[]> m_list{};
	T* m_begin{};                      //本次操作首位置
	T* m_maxSize{};                    //缓冲区的末尾位置
	T* m_end{};                        //存储数据的末尾位置
	bool empty{ true };                //是否为空
	SpinLock m_listMutex;
};

