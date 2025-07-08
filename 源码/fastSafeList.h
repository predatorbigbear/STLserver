#pragma once

#include<type_traits>
#include<memory>
#include "spinLock.h"



template<typename T>
struct is_shared_ptr : std::false_type {};


template<typename T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};


template<typename T>
struct is_shared_ptr<const std::shared_ptr<T>> : std::true_type {};


template<typename T>
inline constexpr bool is_shared_ptr_v = is_shared_ptr<T>::value;


// safeList��ʵ����safe��ֻ��һ������
// ��Ϊ�ں��ڿ��������з��֣�Ҫ�ڲ����б�֤���ܣ�������Ҫ�ֶ�����mutex�ȽϺ���

//ʹ�ó�Ա����ǰ�Ƚ��м������ٽ��в����������ý���

//����SAFELIST���иĽ�������redis��ѯ�ĸ���list,��һ�����̼���ʱ��
//unsafeInsert��unsafePop��������ɾ�ڵ㣬��SAFELIST����Ч
//��ʹ��ָ���ƶ�ʵ��ģ��SAFELIST�Ĳ��� ɾ������
template<typename T, unsigned int listSize = 256>
struct FASTSAFELIST
{
	FASTSAFELIST()
	{
		m_list.reset(new T[listSize]);
		m_begin = m_list.get();
		m_maxSize = m_list.get() + listSize;
		m_end = m_begin;
	}


	void reset(unsigned int bufNum)
	{
		if (bufNum > listSize)
		{
			m_list.reset(new T[bufNum]);
			m_begin = m_list.get();
			m_maxSize = m_list.get() + bufNum;
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
				if (m_end != m_maxSize)
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
					//���ڹ���ָ�룬ʹ����ֵ���ݱ����ڲ����ü����ı䣬ͬʱ��m_begin�ڵ�ֵ��Ϊnullptr
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
					//���ڹ���ָ�룬ʹ����ֵ���ݱ����ڲ����ü����ı䣬ͬʱ��m_begin�ڵ�ֵ��Ϊnullptr
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
	T* m_begin{};                      //���β�����λ��
	T* m_maxSize{};                    //��������ĩβλ��
	T* m_end{};                        //�洢���ݵ�ĩβλ��
	bool empty{ true };                //�Ƿ�Ϊ��
	SpinLock m_listMutex;
};

