#pragma once
#include<memory>
#include<forward_list>


//���������ڴ�ز�ͬ����ƣ�
//ÿ��http�ӿ������ȡʱ��¼���ۼ�ʵ������Ҫ���ڴ��С��
//���ȴ�m_ptr�����ڴ�飬����ĴӺ��ڴ���m_shadow��ȡ��ʵʱ���䣬ʵʱ��������ܿ�ͨ��tmalloc��jmalloc�ȵ������ڴ�ؽ�һ�����٣�
//�ڽӿڽ������֮�󣬵���prepare����
//���������ɴβ�ͬ�Ľӿ������ȷ��m_ptr�ڵ��ڴ����������󲿷�����ȫ�����ڴ����󣬴�������������ڴ���Ч�ʺ�����


template<typename MEMORYTYPE, unsigned int prepareSize = 1024>
struct MEMORYPOOL
{
	MEMORYPOOL() = default;


	MEMORYTYPE* getMemory(unsigned int getLen)
	{
		if (!getLen)
			return nullptr;
		//��������ڴ�����ֵ��ÿ�λ�ȡʱ���жԱ�
		//���ȴ�m_ptr��ȡ�ʵ��ڴ棬��֮�Ӻ󱸿ռ�m_shadow��ȡ
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


	//�ڽӿڽ������֮��ʹ��
	//������ڴ��ǿգ����Խ���ѡ�ڴ�������ۼ�ʵ������Ҫ���ڴ��С
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


	//����������ԴΪ��ʼ״̬
	void reset()
	{
		m_ptr.reset();
		m_ptrMaxSize = m_ptrNowSize = m_needSize = 0;
		m_shadow.clear();
		m_iter = m_shadow.before_begin();
	}



private:
	std::unique_ptr<MEMORYTYPE[]> m_ptr{};    //��ѡ�ڴ��ռ�

	unsigned int m_ptrMaxSize{};              //��ǰ�ڴ�����뵽�Ŀռ��С
	 
	unsigned int m_ptrNowSize{};              //��ǰ�ڴ���Ѿ������ȥ���ڴ�ռ��С

	std::forward_list<std::unique_ptr<MEMORYTYPE[]>>m_shadow{};                                  //���ڴ��ռ�

	typedef typename std::forward_list<std::unique_ptr<MEMORYTYPE[]>>::iterator ITERATOR;

	ITERATOR m_iter{ m_shadow.before_begin() };                                                 //���ڴ��ռ���������                                          

	unsigned int m_needSize{};                //�ۼ�ʵ������Ҫ���ڴ��С
};