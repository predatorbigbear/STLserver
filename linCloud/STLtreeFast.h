#pragma once
#include<memory>
#include<algorithm>
#include<iterator>
#include "publicHeader.h"


namespace MAKEJSON
{
	static const char *comma{ "," };
	static const unsigned int commaLen{ strlen(comma) };


	static const char *doubleQuotation{ "\"" };
	static const unsigned int doubleQuotationLen{ strlen(doubleQuotation) };


	static const char *putStrMiddle{ "\":\"" };
	static const unsigned int putStrMiddleLen{ strlen(putStrMiddle) };


	static const char *pushBackMiddle1{ "\":" };
	static const unsigned int pushBackMiddle1Len{ strlen(pushBackMiddle1) };


	static const char *leftBracket{ "{" };
	static const unsigned int leftBracketLen{ strlen(leftBracket) };


	static const char *rightBracket{ "}" };
	static const unsigned int rightBracketLen{ strlen(rightBracket) };


	static const char *leftRight{ "[]" };
	static const unsigned int leftRightLen{ strlen(leftRight) };


	static const char *leftAngle{ "[" };
	static const unsigned int leftAngleLen{ strlen(leftAngle) };


	static const char *rightAngle{ "]" };
	static const unsigned int rightAngleLen{ strlen(rightAngle) };


	static const char *httpFront{ "HTTP/" };
	static size_t httpFrontLen{ strlen(httpFront) };


	static const char *newLine{ "\r\n\r\n" };
	static size_t newLineLen{ strlen(newLine) };


	static const char *result{ "result" };
	static size_t resultLen{ strlen(result) };

	static const char *halfNewLine{ "\r\n" };
	static size_t halfNewLineLen{ strlen(halfNewLine) };


	static const char *space{ " " };
	static unsigned int spaceLen{ strlen(space) };


	static const char *colon{ ":" };
	static unsigned int colonLen{ strlen(colon) };

	static const char *ContentLength{ "content-length" };
	static unsigned int ContentLengthLen{ strlen(ContentLength) };


	static const char *httpOneZero{ "1.0" };
	static unsigned int httpOneZerolen{ strlen(httpOneZero) };

	static const char *http200{ "200" };
	static unsigned int http200Len{ strlen(http200) };


	static const char *httpOK{ "OK" };
	static unsigned int httpOKLen{ strlen(httpOK) };

	static const char *AccessControlAllowOrigin{ "Access-Control-Allow-Origin" };
	static unsigned int AccessControlAllowOriginLen{ strlen(AccessControlAllowOrigin) };


	static const char *httpStar{ "*" };
	static unsigned int httpStarLen{ strlen(httpStar) };

	static const char *ContentType{ "Content-Type" };
	static unsigned int ContentTypeLen{ strlen(ContentType) };


	static const char *SetCookie{ "Set-Cookie" };
	static unsigned int SetCookieLen{ strlen(SetCookie) };

};


struct TRANSFORMTYPE
{

};


struct AESECB
{

};


struct CUSTOMTAG
{

};



struct STLtreeFast
{
	//需要完善，因为分配可能不成功
	STLtreeFast(MEMORYPOOL<char*> *memoryPoolCharPointer, MEMORYPOOL<char> *sendMemoryPool):m_memoryPoolCharPointer(memoryPoolCharPointer), m_sendMemoryPool(sendMemoryPool)
	{
		if (!resize())
		{
			std::cout << "bad alloc\n";
			int num;
			std::cin >> num;
		}
	}


	STLtreeFast& operator=(const STLtreeFast& other)
	{
		m_ptr = other.m_ptr;
		m_pos = other.m_pos;
		m_maxSize = other.m_maxSize;
		m_empty = other.m_empty;


		m_transformPtr = other.m_transformPtr;
		m_transformPos = other.m_transformPos;
		m_maxTransformSize = other.m_maxTransformSize;
		m_transformEmpty = other.m_transformEmpty;


		m_strSize = other.m_strSize;
		m_resize= other.m_strSize;

		index = other.index;

		m_memoryPoolCharPointer = other.m_memoryPoolCharPointer;
		m_sendMemoryPool = other.m_sendMemoryPool;

		return *this;
	}


	bool clear()
	{
		m_ptr = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_resize));

		if (!m_ptr)
			return false;
		

		m_transformPtr = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_resize));

		if (!m_transformPtr)
			return false;

		m_maxSize = m_maxTransformSize = m_resize;

		m_strSize = 0;
		m_pos = 0;
		m_transformPos = 0;
		index = -1;
		m_empty = m_transformEmpty = true;

		return true;
	}


	bool resize()
	{
		m_ptr = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_resize));

		if (!m_ptr)
			return false;

		m_transformPtr = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_resize));

		if (!m_transformPtr)
			return false;

		m_maxSize = m_maxTransformSize = m_resize;

		m_strSize = 0;
		m_pos = 0;
		m_transformPos = 0;
		index = -1;
		m_empty = m_transformEmpty = true;

		return true;
	}


	void reset()
	{
		m_ptr = nullptr;
		m_pos = m_maxSize = 0;
		m_empty = true;

		m_transformPtr = nullptr;
		m_transformPos = m_maxTransformSize = 0;
		m_transformEmpty = true;


		m_strSize = 0;
		index = -1;
	}



	template<typename T = void>
	bool put(const char *begin, const char *end)
	{
		int len{ ((begin && end) ? end - begin : 0) };
		if (!len)
			return false;
		int thisSize{ 8 };
		if (m_maxSize - m_pos < thisSize)
		{
			const char **ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxSize * 2));
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxSize + thisSize * 2));
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}
			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (m_maxTransformSize == m_transformPos)
			{
				const char **ch{};
				ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxTransformSize * 2));
				if (!ch)
					return false;
				m_maxTransformSize *= 2;

				std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
				m_transformPtr = ch;
			}

			*(m_transformPtr + m_transformPos++) = begin;
			m_transformEmpty = false;
			len *= 2;
		}

		const char **ptr{ m_ptr + m_pos };
		const char **ptrBegin{ ptr };
		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;

			++m_strSize;
		}
		*ptr++ = MAKEJSON::doubleQuotation;
		*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

		*ptr++ = begin;
		*ptr++ = end;

		*ptr++ = MAKEJSON::doubleQuotation;
		*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

		m_strSize += 2 + len;
		m_pos += std::distance(ptrBegin, ptr);
		m_empty = false;
		return true;
	}



	template<typename T = void>
	bool putraw(const char *begin, const char *end)
	{
		if (begin && end)
		{
			int len{ ((begin && end) ? end - begin : 0) };
			if (!len)
				return false;
			int thisSize{ 4 };
			if (m_maxSize - m_pos < thisSize)
			{
				const char **ch{};
				if (m_maxSize * 2 - m_pos > thisSize)
				{
					ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxSize * 2));
					if (!ch)
						return false;
					m_maxSize *= 2;
				}
				else
				{
					ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxSize + thisSize * 2));
					if (!ch)
						return false;
					m_maxSize += thisSize * 2;
				}
				std::copy(m_ptr, m_ptr + m_pos, ch);
				m_ptr = ch;
			}

			if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
			{
				if (m_maxTransformSize == m_transformPos)
				{
					const char **ch{};
					ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxTransformSize * 2));
					if (!ch)
						return false;
					m_maxTransformSize *= 2;

					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}

				*(m_transformPtr + m_transformPos++) = begin;
				m_transformEmpty = false;
				len *= 2;
			}

			const char **ptr{ m_ptr + m_pos };
			const char **ptrBegin{ ptr };

			if (++index)
			{
				*ptr++ = MAKEJSON::comma;
				*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;
				++m_strSize;
			}
			*ptr++ = begin;
			*ptr++ = end;
			m_strSize += len;
			m_pos += std::distance(ptrBegin, ptr);
			m_empty = false;
		}
		return true;
	}



	template<typename T = void>
	bool putrawStr(const char *begin, const char *end)
	{
		int len{ ((begin && end) ? end - begin : 0) };
		if (!len)
			return false;
		int thisSize{ 8 };
		if (m_maxSize - m_pos < thisSize)
		{
			const char **ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxSize * 2));
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxSize + thisSize * 2));
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}
			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (m_maxTransformSize == m_transformPos)
			{
				const char **ch{};
				ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxTransformSize * 2));
				if (!ch)
					return false;
				m_maxTransformSize *= 2;

				std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
				m_transformPtr = ch;
			}

			*(m_transformPtr + m_transformPos++) = begin;
			m_transformEmpty = false;
			len *= 2;
		}

		const char **ptr{ m_ptr + m_pos };
		const char **ptrBegin{ ptr };

		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;
			++m_strSize;
		}
		*ptr++ = MAKEJSON::doubleQuotation;
		*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

		*ptr++ = begin;
		*ptr++ = end;

		*ptr++ = MAKEJSON::doubleQuotation;
		*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;
		m_strSize += 2 + len;

		m_pos += std::distance(ptrBegin, ptr);
		m_empty = false;
		return true;
	}



	//确定不存在转换字符的情况下不需要管默认参数T，否则填入TRANSFORMTYPE
	template<typename T = void>
	bool put(const char * str1Begin, const char * str1End, const char * str2Begin, const char * str2End)
	{
		int len1{ ((str1End && str1Begin) ? str1End - str1Begin : 0) }, len2{ ((str2End && str2Begin) ? str2End - str2Begin : 0) };
		if (!len1)
			return false;
		int thisSize{ 12 };
		if (m_maxSize - m_pos < thisSize)
		{
			const char **ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxSize * 2));
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxSize + thisSize * 2));
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}

			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (m_maxTransformSize < (m_transformPos + 2))
			{
				const char **ch{};
				ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxTransformSize * 2));
				if (!ch)
					return false;
				m_maxTransformSize *= 2;

				std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
				m_transformPtr = ch;
			}

			*(m_transformPtr + m_transformPos++) = str1Begin;
			*(m_transformPtr + m_transformPos++) = str2Begin;
			m_transformEmpty = false;
			len1 *= 2;
			len2 *= 2;
		}

		const char **ptr{ m_ptr + m_pos };
		const char **ptrBegin{ ptr };
		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;

			++m_strSize;
		}
		*ptr++ = MAKEJSON::doubleQuotation;
		*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

		*ptr++ = str1Begin;
		*ptr++ = str1End;

		*ptr++ = MAKEJSON::putStrMiddle;
		*ptr++ = MAKEJSON::putStrMiddle + MAKEJSON::putStrMiddleLen;

		//m_strSize += MAKEJSON::doubleQuotationLen + MAKEJSON::putStrMiddleLen;

		if (len2)
		{
			*ptr++ = str2Begin;
			*ptr++ = str2End;
		}

		*ptr++ = MAKEJSON::doubleQuotation;
		*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

		//m_strSize += MAKEJSON::doubleQuotationLen;

		m_strSize += 5 + len1 + len2;
		
		m_pos += std::distance(ptrBegin, ptr);
		m_empty = false;
		return true;
	}


	template<typename T = void>
	bool push_back(const char * begin, const char * end, const STLtreeFast &other)
	{
		int len1{ ((begin && end) ? end - begin : 0) }, len2{ static_cast<int>(other.m_strSize) };
		if (!len1)
			return false;
		int thisSize{ 12 + static_cast<int>(other.m_pos) };
		if (m_maxSize - m_pos < thisSize)
		{
			const char **ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxSize * 2));
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxSize + thisSize * 2));
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}
			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (!other.isTransformEmpty())
			{
				if (m_maxTransformSize < (m_transformPos + other.m_transformPos))
				{
					const char **ch{};
					ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxTransformSize * 2 + other.m_transformPos));
					if (!ch)
						return false;
					m_maxTransformSize *= 2;
					m_maxTransformSize += other.m_transformPos;


					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}


				*(m_transformPtr + m_transformPos++) = begin;
				std::copy(other.m_transformPtr, other.m_transformPtr + other.m_transformPos, m_transformPtr + m_transformPos);
				m_transformPos += other.m_transformPos;
			}
			else
			{
				if (m_maxTransformSize == m_transformPos)
				{
					const char **ch{};
					ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxTransformSize * 2));
					if (!ch)
						return false;
					m_maxTransformSize *= 2;

					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}

				*(m_transformPtr + m_transformPos++) = begin;
			}

			m_transformEmpty = false;
			len1 *= 2;
		}

		const char **ptr{ m_ptr + m_pos };
		const char **ptrBegin{ ptr };

		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;
			++m_strSize;
		}

		*ptr++ = MAKEJSON::doubleQuotation;
		*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

		*ptr++ = begin;
		*ptr++ = end;

		*ptr++ = MAKEJSON::pushBackMiddle1;
		*ptr++ = MAKEJSON::pushBackMiddle1 + MAKEJSON::pushBackMiddle1Len;

		if (!other.isEmpty())
		{
			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

			std::copy(other.m_ptr, other.m_ptr + other.m_pos, ptr);
			ptr += other.m_pos;

			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;
		}
		else
		{
			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;
		}

		m_strSize += 6 + len1 + len2;
		m_pos += std::distance(ptrBegin, ptr);
		m_empty = false;
		return true;
	}


	template<typename T = void>
	bool push_back(const STLtreeFast &other)
	{
		unsigned int len{ other.m_strSize };
		if (!len)
			return false;
		unsigned int thisSize{ 6 + other.m_pos };
		if (m_maxSize - m_pos < thisSize)
		{
			const char **ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxSize * 2));
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxSize + thisSize * 2));
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}
			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (!other.isTransformEmpty())
			{
				if (m_maxTransformSize < other.m_transformPos)
				{
					const char **ch{};
					ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxTransformSize * 2 + other.m_transformPos));
					if (!ch)
						return false;
					m_maxTransformSize *= 2;
					m_maxTransformSize += other.m_transformPos;


					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}

				std::copy(other.m_transformPtr, other.m_transformPtr + other.m_transformPos, m_transformPtr + m_transformPos);
				m_transformPos += other.m_transformPos;
			}

			m_transformEmpty = false;
		}

		const char **ptr{ m_ptr + m_pos };
		const char **ptrBegin{ ptr };

		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;
			++m_strSize;
		}

		if (other.isEmpty())
		{
			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

			//m_strSize += MAKEJSON::doubleQuotationLen * 2;
			//m_strSize += 2;
		}
		else
		{
			*ptr++ = MAKEJSON::leftBracket;
			*ptr++ = MAKEJSON::leftBracket + MAKEJSON::leftBracketLen;


			std::copy(other.m_ptr, other.m_ptr + other.m_pos, ptr);
			ptr += other.m_pos;


			*ptr++ = MAKEJSON::rightBracket;
			*ptr++ = MAKEJSON::rightBracket + MAKEJSON::rightBracketLen;

			//m_strSize += MAKEJSON::leftBracketLen + MAKEJSON::rightBracketLen;
			//m_strSize += 2;
		}

		m_strSize += 2 + len;
		m_pos += std::distance(ptrBegin, ptr);

		m_empty = false;
		return true;
	}



	template<typename T = void>
	bool put_child(const char * begin, const char * end, const STLtreeFast &other)
	{
		int len1{ ((begin && end) ? end - begin : 0) }, len2{ other.isEmpty() ? 0 : static_cast<int>(other.m_strSize) };
		if (!len1)
			return false;
		int thisSize{ 12 + static_cast<int>(other.m_pos) };
		if (m_maxSize - m_pos < thisSize)
		{
			const char **ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxSize * 2));
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxSize + thisSize * 2));
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}
			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (!other.isTransformEmpty())
			{
				if (m_maxTransformSize < (m_transformPos + other.m_transformPos))
				{
					const char **ch{};
					ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxTransformSize * 2 + other.m_transformPos));
					if (!ch)
						return false;
					m_maxTransformSize *= 2;
					m_maxTransformSize += other.m_transformPos;


					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}


				*(m_transformPtr + m_transformPos++) = begin;
				std::copy(other.m_transformPtr, other.m_transformPtr + other.m_transformPos, m_transformPtr + m_transformPos);
				m_transformPos += other.m_transformPos;
			}
			else
			{
				if (m_maxTransformSize == m_transformPos)
				{
					const char **ch{};
					ch = const_cast<const char**>(m_memoryPoolCharPointer->getMemory(m_maxTransformSize * 2));
					if (!ch)
						return false;
					m_maxTransformSize *= 2;

					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}

				*(m_transformPtr + m_transformPos++) = begin;
			}

			m_transformEmpty = false;
			len1 *= 2;
		}

		const char **ptr{ m_ptr + m_pos };
		const char **ptrBegin{ ptr };

		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;
			++m_strSize;
		}

		*ptr++ = MAKEJSON::doubleQuotation;
		*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

		*ptr++ = begin;
		*ptr++ = end;

		*ptr++ = MAKEJSON::pushBackMiddle1;
		*ptr++ = MAKEJSON::pushBackMiddle1 + MAKEJSON::pushBackMiddle1Len;

		//m_strSize += MAKEJSON::doubleQuotationLen + MAKEJSON::pushBackMiddle1Len;   3

		if (other.isEmpty())
		{
			*ptr++ = MAKEJSON::leftRight;
			*ptr++ = MAKEJSON::leftRight + MAKEJSON::leftRightLen;
			//m_strSize+= MAKEJSON::leftRightLen;   2
		}
		else
		{
			*ptr++ = MAKEJSON::leftAngle;
			*ptr++ = MAKEJSON::leftAngle + MAKEJSON::leftAngleLen;

			std::copy(other.m_ptr, other.m_ptr + other.m_pos, ptr);
			ptr += other.m_pos;

			*ptr++ = MAKEJSON::rightAngle;
			*ptr++ = MAKEJSON::rightAngle + MAKEJSON::rightAngleLen;

			//m_strSize += MAKEJSON::leftAngleLen + MAKEJSON::rightAngleLen;  2
		}

		m_strSize += 5 + len1 + len2;
		m_pos += std::distance(ptrBegin, ptr);
 		m_empty = false;
		return true;
	}



	// 模板参数解释
	// 是否需要考虑json转换
	// 是否存在自定义http header头部
	// 是否需要对body部分做AES加密
	// 自定义http header头部操作函数类型

	//一条http消息回复 比如  HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:长度\r\n\r\njson结构字符串
	//拆成三块看，第一块为HTTP到Content-Length:    第二部分为长度和\r\n\r\n   第三部分为json结构字符串
	//STLTreeFast在插入字符串时仅保存json指针串，并且将指定转码的指针存入m_transformPtr中，并存入预判所需的空间m_strSize
	//生成时，利用m_strSize计算出长度所需空间大小
	//那么整条HTTP回复所需预判空间为HTTP到Content-Length:所需空间  长度所需空间+\r\n\r\n所需空间  +json所需空间
	//一次性获取该大小的空间，跳到json空间起始位置根据指针串生成json，根据模板参数T的标志判断是否需要处理json转义
	//生成json字符串后，根据实际生产json串首尾位置获取真正所需的长度，计算长度所需空间大小
	//计算整条HTTP回复所需预判空间为HTTP到Content-Length:所需空间  长度所需空间+\r\n\r\n所需空间，从json起始位置往前跳生成整条http消息
	//返回http消息起始位置和长度用于发送
	//如果某些http header需要自定义办法写入，则设置HTTPFLAG，在httpWrite传入写入lambda ，类型为(char*&)   httpLen为自定义http header加内容长度
	//则会在检测HTTPFLAG类型后再写入长度之后，返回指针位置进行自定义写入
	//利用此算法一次性生成http json回复，对于常规的生成算法具有绝对性能优势
template<typename T = void, typename HTTPFLAG = void, typename ENCTYPT = void, typename HTTPFUNCTION = void*,  typename ...ARG>
	bool make_json(char *&resultPtr, unsigned int &resultLen, HTTPFUNCTION httpWrite, unsigned int httpLen , AES_KEY *enctyptKey , const char *httpVersionBegin, const char *httpVersionEnd,
		const char *httpCodeBegin, const char *httpCodeEnd, const char *httpResultBegin, const char *httpResultEnd, ARG&&...args)
	{
		if constexpr (!std::is_same<T, TRANSFORMTYPE>::value)
		{
			if constexpr (std::is_same<ENCTYPT, AESECB>::value)
			{
				if (!enctyptKey)
					return false;
			}

			if (!m_pos)
				return false;

			int parSize{ sizeof...(args) }, httpHeadLen{};
			if (!httpVersionBegin || !httpVersionEnd || !httpCodeBegin || !httpCodeEnd || !httpResultBegin || !httpResultEnd  || !calLength(httpHeadLen, args...))
				return false;

			int needLength{ MAKEJSON::httpFrontLen + std::distance(httpVersionBegin,httpVersionEnd) + MAKEJSON::spaceLen + std::distance(httpCodeBegin,httpCodeEnd)
				+ MAKEJSON::spaceLen + std::distance(httpResultBegin,httpResultEnd) + MAKEJSON::halfNewLineLen + httpHeadLen + MAKEJSON::ContentLengthLen + MAKEJSON::colonLen };

			if constexpr (std::is_same<HTTPFLAG, CUSTOMTAG>::value)
			{
				needLength += httpLen + MAKEJSON::halfNewLineLen;
			}


			resultPtr = nullptr, resultLen = 0;

			int thisJsonLen{ jsonLen() };

			if constexpr (std::is_same<ENCTYPT, AESECB>::value)
			{
				thisJsonLen += 16;
			}
		

			unsigned int needFrontLen{ needLength + stringLen(thisJsonLen) + MAKEJSON::newLineLen };

			unsigned int needLen{ needFrontLen + thisJsonLen };     //93

			/////////////////////////////

			//////////////////////////////////////////////////

			
			char *newResultPtr{ m_sendMemoryPool->getMemory(needLen) };

			if (!newResultPtr)
				return false;


			unsigned int i{}, pos{ m_pos };
			char *iterEnd{ newResultPtr + needFrontLen }, *iterBegin{ newResultPtr + needFrontLen };

			const char **begin{ m_ptr }, **end{ m_ptr + pos };

			*iterEnd++ = '{';
			do
			{

				std::copy(*begin, *(begin + 1), iterEnd);
				iterEnd += std::distance(*begin, *(begin + 1));
				begin += 2;
			} while (begin != end);
		
			
			*iterEnd++ = '}';

			if constexpr (!std::is_same<ENCTYPT, void>::value)
			{
				int paddingLen = 16 - (std::distance(iterBegin, iterEnd) % 16);
				if (paddingLen != 16)
				{
					do
					{
						*iterEnd++ = 0;
					} while (--paddingLen);
				}

				char *encryptIter{ iterBegin };

				while (encryptIter != iterEnd)
				{
					AES_ecb_encrypt(const_cast<const unsigned char*>(reinterpret_cast<unsigned char*>(encryptIter)), reinterpret_cast<unsigned char*>(encryptIter), enctyptKey, AES_ENCRYPT);
					encryptIter += 16;
				}
			}

			unsigned int jsonResultLen = std::distance(iterBegin, iterEnd), finalStringLen = stringLen(jsonResultLen);


			//iterBegin - finalStringLen -MAKEJSON::newLineLen -needLength
			resultPtr = newResultPtr = iterBegin - MAKEJSON::newLineLen - finalStringLen - needLength;
			resultLen = std::distance(newResultPtr, iterEnd);

			////////////////////////////////////////////////////////////////////////////////////////////////


			std::copy(MAKEJSON::httpFront, MAKEJSON::httpFront + MAKEJSON::httpFrontLen, newResultPtr);
			newResultPtr += MAKEJSON::httpFrontLen;


			std::copy(httpVersionBegin, httpVersionEnd, newResultPtr);
			newResultPtr +=std::distance(httpVersionBegin, httpVersionEnd);


			std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, newResultPtr);
			newResultPtr += MAKEJSON::spaceLen;
			

			std::copy(httpCodeBegin, httpCodeEnd, newResultPtr);
			newResultPtr += std::distance(httpCodeBegin, httpCodeEnd);


			std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, newResultPtr);
			newResultPtr += MAKEJSON::spaceLen;
			

			std::copy(httpResultBegin, httpResultEnd, newResultPtr);
			newResultPtr += std::distance(httpResultBegin, httpResultEnd);


			std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, newResultPtr);
			newResultPtr += MAKEJSON::halfNewLineLen;
			
			packAgeHttpHeader(newResultPtr, args...);

			///////////////////////////////////////////////////////////////////////////////////////////////////



			std::copy(MAKEJSON::ContentLength, MAKEJSON::ContentLength + MAKEJSON::ContentLengthLen, newResultPtr);
			newResultPtr += MAKEJSON::ContentLengthLen;


			std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, newResultPtr);
			newResultPtr += MAKEJSON::colonLen;


			if (jsonResultLen > 99999999)
				*newResultPtr++ = jsonResultLen / 100000000 + '0';
			if (jsonResultLen > 9999999)
				*newResultPtr++ = jsonResultLen / 10000000 % 10 + '0';
			if (jsonResultLen > 999999)
				*newResultPtr++ = jsonResultLen / 1000000 % 10 + '0';
			if (jsonResultLen > 99999)
				*newResultPtr++ = jsonResultLen / 100000 % 10 + '0';
			if (jsonResultLen > 9999)
				*newResultPtr++ = jsonResultLen / 10000 % 10 + '0';
			if (jsonResultLen > 999)
				*newResultPtr++ = jsonResultLen / 1000 % 10 + '0';
			if (jsonResultLen > 99)
				*newResultPtr++ = jsonResultLen / 100 % 10 + '0';
			if (jsonResultLen > 9)
				*newResultPtr++ = jsonResultLen / 10 % 10 + '0';
			*newResultPtr++ = jsonResultLen % 10 + '0';


			if constexpr (std::is_same<HTTPFLAG, CUSTOMTAG>::value)
			{
				std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, newResultPtr);
				newResultPtr += MAKEJSON::halfNewLineLen;
				httpWrite(newResultPtr);
			}


			std::copy(MAKEJSON::newLine, MAKEJSON::newLine + MAKEJSON::newLineLen, newResultPtr);

			return true;
		}

		else
		{


		if constexpr (std::is_same<ENCTYPT, AESECB>::value)
		{
			if (!enctyptKey)
				return false;
		}


		if (!m_pos)
			return false;

		int parSize{ sizeof...(args) }, httpHeadLen{};
		if (!httpVersionBegin || !httpVersionEnd || !httpCodeBegin || !httpCodeEnd || !httpResultBegin || !httpResultEnd || !calLength(httpHeadLen, args...))
			return false;

		int needLength{ MAKEJSON::httpFrontLen + std::distance(httpVersionBegin,httpVersionEnd) + MAKEJSON::spaceLen + std::distance(httpCodeBegin,httpCodeEnd)
			+ MAKEJSON::spaceLen + std::distance(httpResultBegin,httpResultEnd) + MAKEJSON::halfNewLineLen + httpHeadLen + MAKEJSON::ContentLengthLen + MAKEJSON::colonLen };

		if constexpr (std::is_same<HTTPFLAG, CUSTOMTAG>::value)
		{
			needLength += httpLen + MAKEJSON::halfNewLineLen;
		}


		resultPtr = nullptr, resultLen = 0;

		int thisJsonLen{ jsonLen() };

		if constexpr (std::is_same<ENCTYPT, AESECB>::value)
		{
			thisJsonLen += 16;
		}

		unsigned int needFrontLen{ needLength + stringLen(thisJsonLen) + MAKEJSON::newLineLen };

		unsigned int needLen{ needFrontLen + thisJsonLen };

        /////////////////////////////////////////////////////////////

		
		char *newResultPtr{ m_sendMemoryPool->getMemory(needLen) };

		if (!newResultPtr)
			return false;


		unsigned int i{}, pos{ m_pos };
		char *iterEnd{ newResultPtr + needFrontLen }, *iterBegin{ newResultPtr + needFrontLen };

		const char **begin{ m_ptr }, **end{ m_ptr + pos }, **transformBegin{ m_transformPtr }, **transformEnd{ m_transformPtr + m_transformPos };
		char *iterTempBegin{}, *iterTempEnd{};
		*iterEnd++ = '{';
		do
		{
			if (*begin != *transformBegin)
			{
				std::copy(*begin, *(begin + 1), iterEnd);
				iterEnd += std::distance(*begin, *(begin + 1));
			}
			else
			{
				iterTempBegin = const_cast<char*>(*begin), iterTempEnd = const_cast<char*>(*(begin + 1));
				while (iterTempBegin != iterTempEnd)
				{
					switch (*iterTempBegin)
					{
					case '\"':
						*iterEnd++ = '\\';
						*iterEnd++ = '\"';
						break;
					case '\\':
						*iterEnd++ = '\\';
						*iterEnd++ = '\\';
						break;
					case '/':
						*iterEnd++ = '\\';
						*iterEnd++ = '/';
						break;
					case '\b':
						*iterEnd++ = '\\';
						*iterEnd++ = 'b';
						break;
					case '\f':
						*iterEnd++ = '\\';
						*iterEnd++ = 'f';
						break;
					case '\n':
						*iterEnd++ = '\\';
						*iterEnd++ = 'n';
						break;
					case '\r':
						*iterEnd++ = '\\';
						*iterEnd++ = 'r';
						break;
					case '\t':
						*iterEnd++ = '\\';
						*iterEnd++ = 't';
						break;
					default:
						*iterEnd++ = *iterTempBegin;
						break;
					}
					++iterTempBegin;
				}
				if (++transformBegin == transformEnd)
				{
					begin += 2;
					break;
				}
			}
			begin += 2;
		} while (begin != end);

		while (begin != end)
		{
			std::copy(*begin, *(begin + 1), iterEnd);
			iterEnd += std::distance(*begin, *(begin + 1));
			begin += 2;
		}
		*iterEnd++ = '}';


		if constexpr (std::is_same<ENCTYPT, AESECB>::value)
		{
			int paddingLen = 16 - (std::distance(iterBegin, iterEnd) % 16);
			if (paddingLen != 16)
			{
				do
				{
					*iterEnd++ = 0;
				} while (--paddingLen);
			}

			char *encryptIter{ iterBegin };

			while (encryptIter != iterEnd)
			{
				AES_ecb_encrypt(const_cast<const unsigned char*>(reinterpret_cast<unsigned char*>(encryptIter)), reinterpret_cast<unsigned char*>(encryptIter), enctyptKey, AES_ENCRYPT);
				encryptIter += 16;
			}
		}

		unsigned int jsonResultLen{ std::distance(iterBegin, iterEnd) };
		unsigned int finalStringLen{ stringLen(jsonResultLen) };


		//iterBegin - finalStringLen -MAKEJSON::newLineLen -needLength
		resultPtr = newResultPtr = iterBegin - MAKEJSON::newLineLen - finalStringLen - needLength;
		resultLen = std::distance(newResultPtr, iterEnd);


		std::copy(MAKEJSON::httpFront, MAKEJSON::httpFront + MAKEJSON::httpFrontLen, newResultPtr);
		newResultPtr += MAKEJSON::httpFrontLen;

		std::copy(httpVersionBegin, httpVersionEnd, newResultPtr);
		newResultPtr += std::distance(httpVersionBegin, httpVersionEnd);

		std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, newResultPtr);
		newResultPtr += MAKEJSON::spaceLen;

		std::copy(httpCodeBegin, httpCodeEnd, newResultPtr);
		newResultPtr += std::distance(httpCodeBegin, httpCodeEnd);

		std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, newResultPtr);
		newResultPtr += MAKEJSON::spaceLen;

		std::copy(httpResultBegin, httpResultEnd, newResultPtr);
		newResultPtr += std::distance(httpResultBegin, httpResultEnd);

		std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, newResultPtr);
		newResultPtr += MAKEJSON::halfNewLineLen;

		packAgeHttpHeader(newResultPtr, args...);


		std::copy(MAKEJSON::ContentLength, MAKEJSON::ContentLength + MAKEJSON::ContentLengthLen, newResultPtr);
		newResultPtr += MAKEJSON::ContentLengthLen;

		std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, newResultPtr);
		newResultPtr += MAKEJSON::colonLen;


		if (jsonResultLen > 99999999)
			*newResultPtr++ = jsonResultLen / 100000000 + '0';
		if (jsonResultLen > 9999999)
			*newResultPtr++ = jsonResultLen / 10000000 % 10 + '0';
		if (jsonResultLen > 999999)
			*newResultPtr++ = jsonResultLen / 1000000 % 10 + '0';
		if (jsonResultLen > 99999)
			*newResultPtr++ = jsonResultLen / 100000 % 10 + '0';
		if (jsonResultLen > 9999)
			*newResultPtr++ = jsonResultLen / 10000 % 10 + '0';
		if (jsonResultLen > 999)
			*newResultPtr++ = jsonResultLen / 1000 % 10 + '0';
		if (jsonResultLen > 99)
			*newResultPtr++ = jsonResultLen / 100 % 10 + '0';
		if (jsonResultLen > 9)
			*newResultPtr++ = jsonResultLen / 10 % 10 + '0';
		*newResultPtr++ = jsonResultLen % 10 + '0';

		if constexpr (std::is_same<HTTPFLAG, CUSTOMTAG>::value)
		{
			std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, newResultPtr);
			newResultPtr += MAKEJSON::halfNewLineLen;
			httpWrite(newResultPtr);
		}

		std::copy(MAKEJSON::newLine, MAKEJSON::newLine + MAKEJSON::newLineLen, newResultPtr);

		return true;
		}
	}


	const unsigned int jsonLen()
	{
		return m_strSize + 2;
	}



	const bool isEmpty()const
	{
		return m_empty;
	}


	const bool isTransformEmpty()const
	{
		return m_transformEmpty;
	}


	private:
		template<typename HEADERTYPE>
		bool calLength(int &httpLength, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd)
		{
			if constexpr (std::is_same<HEADERTYPE,const char*>::value)
			{
				if (!headerBegin || !headerEnd || !wordBegin || !wordEnd)
					return false; 
				httpLength += std::distance(headerBegin, headerEnd) + 3 + std::distance(wordBegin, wordEnd);
				return true;
			}
			return false;
		}



		template<typename HEADERTYPE, typename ...ARG>
		bool calLength(int &httpLength, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd, ARG&& ... args)
		{
			if constexpr (std::is_same<HEADERTYPE, const char*>::value)
			{
				if (!headerBegin || !headerEnd || !wordBegin || !wordEnd)
					return false;
				httpLength += std::distance(headerBegin, headerEnd) + 3 + std::distance(wordBegin, wordEnd);
				return calLength(httpLength, args...);
			}
			return false;
		}



		template<typename HEADERTYPE>
		void packAgeHttpHeader(char *&httpPointer, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd)
		{
			if constexpr (std::is_same<HEADERTYPE, const char*>::value)
			{

				std::copy(headerBegin, headerEnd, httpPointer);
				httpPointer += std::distance(headerBegin, headerEnd);


				std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, httpPointer);
				httpPointer += MAKEJSON::colonLen;


				std::copy(wordBegin, wordEnd, httpPointer);
				httpPointer += std::distance(wordBegin, wordEnd);

				std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, httpPointer);
				httpPointer += MAKEJSON::halfNewLineLen;
			}
		}



		template<typename HEADERTYPE, typename ...ARG>
		void packAgeHttpHeader(char *&httpPointer, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd, ARG&& ... args)
		{
			if constexpr (std::is_same<HEADERTYPE, const char*>::value)
			{

				std::copy(headerBegin, headerEnd, httpPointer);
				httpPointer += std::distance(headerBegin, headerEnd);


				std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, httpPointer);
				httpPointer += MAKEJSON::colonLen;

				std::copy(wordBegin, wordEnd, httpPointer);
				httpPointer += std::distance(wordBegin, wordEnd);

				std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, httpPointer);
				httpPointer += MAKEJSON::halfNewLineLen;

				packAgeHttpHeader(httpPointer, args...);
			}
		}




private:
	const char **m_ptr{};
	unsigned int m_pos{};
	unsigned int m_maxSize{ };
	bool m_empty{ true };


	const char **m_transformPtr{};
	unsigned int m_transformPos{};
	unsigned int m_maxTransformSize{ };
	bool m_transformEmpty{ true };




	unsigned int m_resize{ 1024 };
	unsigned int m_strSize{};

	int index{ -1 };

	MEMORYPOOL<char*> *m_memoryPoolCharPointer{ nullptr };

	MEMORYPOOL<char> *m_sendMemoryPool{ nullptr };

};