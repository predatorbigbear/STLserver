#pragma once

#include<algorithm>
#include<functional>
#include<locale>



//添加新街口时，根据接口字段编写   1  2  3部分


//////////////////////////////////////////////////////////   1
static const char *actionStr{ "action" };
static const char *functionStr{ "function" };
static const char *funcDataStr{ "funcdata" };

static constexpr unsigned int actionLen{ 6 };
static constexpr unsigned int functionLen{ 8 };
static constexpr unsigned int funcDataLen{ 8 };
//////////////////////////////////////////////////////////   1



namespace HTTPINTERFACENUM
{

	//////////////////////////////////////////////////////   2
	enum TEST1
	{
		action = 0,
		function = 2,
		funcData = 4
	};

	enum TESTMAKEJSON
	{
		jsonType = 0
	};

	enum TESTCOMPAREWORKFLOW
	{
		stringLen=0
	};
	//////////////////////////////////////////////////////   2
};




template<typename INTERFACE, unsigned int INTERFACEPARANUM>
static bool parseBodyRamdom(const char *source, const unsigned int sourceLen, const char **des, const unsigned int desLen)
{
	if (!source || !sourceLen || !des || !desLen || !INTERFACEPARANUM || desLen < (INTERFACEPARANUM * 2))
		return false;

	std::fill(des, des + (INTERFACEPARANUM * 2), nullptr);

	const char *iterBegin{ source }, *iterEnd{ source + sourceLen }, *iterStrBegin{}, *iterStrEnd{}, *iterWordBegin{}, *iterWordEnd{};
	unsigned int paraNum{};


	while (iterBegin != iterEnd)
	{
		iterStrBegin = iterBegin;

		iterStrEnd = std::find_if(iterStrBegin + 1, iterEnd, std::bind(std::logical_or<>(), std::bind(std::equal_to<>(), std::placeholders::_1, '='),
			std::bind(std::equal_to<>(), std::placeholders::_1, '&')));


		if (iterStrEnd == iterEnd || *iterStrEnd == '&')
		{
			if (++paraNum > INTERFACEPARANUM)
				return false;


			///////////////////////////////////////////////////////////////////////////////////    3
			if constexpr (std::is_same<INTERFACE, HTTPINTERFACENUM::TEST1>::value)
			{
				switch (std::distance(iterStrBegin, iterStrEnd))
				{
				case actionLen:
					if (!std::equal(iterStrBegin, iterStrEnd, actionStr, actionStr + actionLen,
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
						return false;

					*(des + HTTPINTERFACENUM::TEST1::action) = iterStrBegin;
					*(des + HTTPINTERFACENUM::TEST1::action + 1) = iterStrBegin;

					break;
				case functionLen:
					switch (*(iterStrBegin + 4))
					{
					case 't':
					case 'T':
						if (!std::equal(iterStrBegin, iterStrEnd, functionStr, functionStr + functionLen,
							std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
							return false;

						*(des + HTTPINTERFACENUM::TEST1::function) = iterStrBegin;
						*(des + HTTPINTERFACENUM::TEST1::function + 1) = iterStrBegin;

						break;
					case 'd':
					case 'D':
						if (!std::equal(iterStrBegin, iterStrEnd, funcDataStr, funcDataStr + funcDataLen,
							std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
							return false;

						*(des + HTTPINTERFACENUM::TEST1::funcData) = iterStrBegin;
						*(des + HTTPINTERFACENUM::TEST1::funcData + 1) = iterStrBegin;

						break;
					default:
						return false;
						break;
					}

					break;

				default:

					return false;
					break;
				}
			}
			////////////////////////////////////////////////////////////////////////////////////////  3


			iterBegin = iterStrEnd;
			if (iterStrEnd != iterEnd)
				++iterBegin;
		}
		else
		{
			iterWordBegin = iterStrEnd + 1;
			iterWordEnd = std::find(iterWordBegin, iterEnd, '&');

			if (++paraNum > INTERFACEPARANUM)
				return false;


			//////////////////////////////////////////////////////////////////////////////////////////    3
			if constexpr (std::is_same<INTERFACE, HTTPINTERFACENUM::TEST1>::value)
			{
				switch (std::distance(iterStrBegin, iterStrEnd))
				{
				case actionLen:
					if (!std::equal(iterStrBegin, iterStrEnd, actionStr, actionStr + actionLen,
						std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
						return false;

					*(des + HTTPINTERFACENUM::TEST1::action) = iterWordBegin;
					*(des + HTTPINTERFACENUM::TEST1::action + 1) = iterWordEnd;

					break;
				case functionLen:
					switch (*(iterStrBegin + 4))
					{
					case 't':
					case 'T':
						if (!std::equal(iterStrBegin, iterStrEnd, functionStr, functionStr + functionLen,
							std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
							return false;

						*(des + HTTPINTERFACENUM::TEST1::function) = iterWordBegin;
						*(des + HTTPINTERFACENUM::TEST1::function + 1) = iterWordEnd;

						break;
					case 'd':
					case 'D':
						if (!std::equal(iterStrBegin, iterStrEnd, funcDataStr, funcDataStr + funcDataLen,
							std::bind(std::equal_to<>(), std::bind(::tolower, std::placeholders::_1), std::placeholders::_2)))
							return false;

						*(des + HTTPINTERFACENUM::TEST1::funcData) = iterWordBegin;
						*(des + HTTPINTERFACENUM::TEST1::funcData + 1) = iterWordEnd;

						break;
					default:
						return false;
						break;
					}
					break;

				default:

					return false;
					break;
				}
			}
			///////////////////////////////////////////////////////////////////////////////////////////////   3

			iterBegin = iterWordEnd;
			if (iterWordEnd != iterEnd)
				++iterBegin;
		}
	}

	return true;
}











/*

testExample:
const char *test{ "action=123&funcData&function=5236" };
	const unsigned int testLen{ strlen(test) };

	const char *test1{ "action=123&funcData&function=5236&rfghj" };
	const unsigned int test1Len{ strlen(test1) };

	const char *des[50]{};

	if (parseBodyRamdom<HTTPINTERFACENUM::TEST1,3>(test, testLen, des, 50))
	{
		std::cout << "yes\n";
	}
	else
	{
		std::cout << "no\n";
	}

	if (parseBodyRamdom<HTTPINTERFACENUM::TEST1, 3>(test1, test1Len, des, 50))
	{
		std::cout << "yes\n";
	}
	else
	{
		std::cout << "no\n";
	}

*/