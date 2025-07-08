#pragma once
#include<memory>
#include<forward_list>
#include<functional>
#include<type_traits>

//���������ڴ�ز�ͬ����ƣ�
//ÿ��http�ӿ������ȡʱ��¼���ۼ�ʵ������Ҫ���ڴ��С��
//���ȴ�m_ptr�����ڴ�飬����ĴӺ��ڴ���m_shadow��ȡ��ʵʱ���䣬ʵʱ��������ܿ�ͨ��tmalloc��jmalloc�ȵ������ڴ�ؽ�һ�����٣�
//�ڽӿڽ������֮�󣬵���prepare����
// prepare�����Ǽ���ۼ�ʵ������Ҫ���ڴ��С�Ƿ����m_ptr�ڴ��Ĵ�С��������m_shadow�ͷ�
// ������������һ�����·��䣬����ֻ��Ҫ��������0
//���������ɴβ�ͬ�Ľӿ������ȷ��m_ptr�ڵ��ڴ����������󲿷�����ȫ�����ڴ����󣬴�������������ڴ���Ч�ʺ�����
//����ù����ڴ�أ������м����������ģ���������ڴ��Ч��û������ʵ�ּ򵥸�Ч������prepare�Ĺ��̼����޷�ʵ��
//��Ϊ�����ڴ��ͬһʱ����N��socket������ʹ�ã�����Ҫ�ҵ�ʱ�������ط����������ۼ�ʵ������Ҫ���ڴ��С�Լ��ͷŶ����ڴ���Ƿǳ����ѵ�


// ����ģ�壺��ָ������ֱ�ӷ���
template<typename T>
struct remove_all_pointers
{
	using type = T;
};

// ƫ�ػ�������ָ�����ͣ��ݹ��⣩
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
		//�ж��Ƿ���ָ�룬�����Ƿ�Ϊvoid*����
		static_assert(std::is_pointer_v<T>,
			"T must be a  pointer type");

		//��ȡԭʼָ���е�ԭʼ����
		using RawType = std::remove_pointer_t<T>;

		if (!getLen)
			return nullptr;

		//��������ڴ�����ֵ��ÿ�λ�ȡʱ���жԱ�
		//���ȴ�m_ptr��ȡ�ʵ��ڴ棬��֮�Ӻ󱸿ռ�m_shadow��ȡ
		static constexpr unsigned int maxMemorySize{ 100 * 1024 };

		//���㱾����Ҫ������ڴ�������С
		unsigned int thisGetLen{};

		//�ݹ���T���ͣ��ж����������ǲ���void
		if constexpr (!std::is_same_v<remove_all_pointers_t<T>, void>)
		{
			thisGetLen = getLen * sizeof(RawType);
		}
		else
		{
			thisGetLen = getLen * sizeof(char*);
		}

		//������ɷ���������ȥ��ǰ�ѷ����С
		if (maxMemorySize - m_needSize < thisGetLen)
			return nullptr;

		if (!m_ptrMaxSize)
		{
			try
			{
				m_ptr.reset(new char[thisGetLen]);
				//�ۼ�ʵ������Ҫ���ڴ��С
				m_ptrMaxSize = m_ptrNowSize = m_needSize = thisGetLen;
				return reinterpret_cast<T>(m_ptr.get());
			}
			catch (const std::exception& e)
			{
				return nullptr;
			}
		}
		else
		{
			if (m_ptrNowSize + thisGetLen < m_ptrMaxSize)
			{
				T returnPtr{ reinterpret_cast<T>(m_ptr.get()) + m_ptrNowSize };
				m_ptrNowSize += thisGetLen;
				//�ۼ�ʵ������Ҫ���ڴ��С
				m_needSize += thisGetLen;
				return returnPtr;
			}
			else
			{
				try
				{
					//�ж��Ƿ�Խ��unsigned int���ֵ
					if (m_needSize + thisGetLen < m_needSize || (m_needSize + thisGetLen) < thisGetLen)
						return nullptr;
					m_iter = m_shadow.emplace_after(m_iter, std::unique_ptr<char[]>(new char[thisGetLen]));
					//�ۼ�ʵ������Ҫ���ڴ��С
					m_needSize += thisGetLen;
					return reinterpret_cast<T>(m_iter->get());
				}
				catch (const std::exception& e)
				{
					return nullptr;
				}
			}
		}
	}


	//�ڽӿڽ������֮��ʹ��
	//������ڴ��ǿգ����Խ���ѡ�ڴ�������ۼ�ʵ������Ҫ���ڴ��С
	void prepare()
	{
		if (!m_shadow.empty())
		{
			try
			{
				m_shadow.clear();
				m_ptr.reset(new char[m_needSize]);
				m_ptrMaxSize = m_needSize;
				m_iter = m_shadow.before_begin();
			}
			catch (const std::exception& e)
			{
				m_ptrMaxSize = 0;
			}
		}
		m_needSize = m_ptrNowSize = 0;
	}



	//����������ԴΪ��ʼ״̬����socket����ʱ����
	void reset()
	{
		if (m_ptrMaxSize > prepareSize)
		{
			m_ptr.reset(new char[prepareSize]);
			m_ptrMaxSize = prepareSize;
		}
		m_ptrNowSize = m_needSize = 0;
		if (!m_shadow.empty())
		{
			m_shadow.clear();
			m_iter = m_shadow.before_begin();
		}
	}



private:
	//��ΪC++�涨 char����������ƽ̨��С����1�ֽڣ����Ը���char���;��У�����void��
	std::unique_ptr<char[]> m_ptr{};    //��ѡ�ڴ��ռ�

	unsigned int m_ptrMaxSize{ };              //��ǰ�ڴ�����뵽�Ŀռ��С

	unsigned int m_ptrNowSize{};              //��ǰ�ڴ���Ѿ������ȥ���ڴ�ռ��С

	std::forward_list < std::unique_ptr<char[]>> m_shadow{};                                  //���ڴ��ռ�

	typedef typename std::forward_list < std::unique_ptr<char[]>> ::iterator ITERATOR;

	ITERATOR m_iter{ m_shadow.before_begin() };                                                 //���ڴ��ռ���������                                          

	unsigned int m_needSize{};                //�ۼ�ʵ������Ҫ���ڴ��С
};