/*

#include<iostream>
#include<string>
#include<algorithm>
#include<functional>
#include<cctype>

static const char *action{ "action" };
static const char *function{ "function" };
static const char *funcData{ "funcdata" };

static constexpr unsigned int actionLen{ 6 };
static constexpr unsigned int functionLen{ 8 };
static constexpr unsigned int funcDataLen{ 8 };


namespace HTTPINTERFACENUM
{
	enum TEST1
	{
		action = 0,
		function = 2,
		funcData=4
	};
};


template<typename INTERFACE,unsigned int INTERFACEPARANUM>
static bool parseBody(const char *source, const unsigned int sourceLen,const char **des, const unsigned int desLen)
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

			
				if constexpr (std::is_same<INTERFACE, HTTPINTERFACENUM::TEST1>::value)
				{
					switch (std::distance(iterStrBegin, iterStrEnd))
					{
					case actionLen:
						if (!std::equal(iterStrBegin, iterStrEnd, action, action + actionLen,
							std::bind(std::equal_to<>(), std::bind(::std::tolower, std::placeholders::_1), std::placeholders::_2)))
							return false;

						*(des + HTTPINTERFACENUM::TEST1::action) = iterStrBegin;
						*(des + HTTPINTERFACENUM::TEST1::action + 1) = iterStrBegin;

						break;
					case functionLen:
						switch (*(iterStrBegin + 4))
						{
						case 't':
						case 'T':
							if (!std::equal(iterStrBegin, iterStrEnd, function, function + functionLen, 
								std::bind(std::equal_to<>(), std::bind(::std::tolower, std::placeholders::_1), std::placeholders::_2)))
								return false;

							*(des + HTTPINTERFACENUM::TEST1::function) = iterStrBegin;
							*(des + HTTPINTERFACENUM::TEST1::function + 1) = iterStrBegin;

							break;
						case 'd':
						case 'D':
							if (!std::equal(iterStrBegin, iterStrEnd, funcData, funcData + funcDataLen, 
								std::bind(std::equal_to<>(), std::bind(::std::tolower, std::placeholders::_1), std::placeholders::_2)))
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

			
			iterBegin = iterStrEnd;
			if (iterStrEnd != iterEnd)
				++iterBegin;
		}
		else
		{
			iterWordBegin = iterStrEnd + 1;
			iterWordEnd = std::find(iterWordBegin, iterEnd,'&');

			if (++paraNum > INTERFACEPARANUM)
				return false;


				if constexpr (std::is_same<INTERFACE, HTTPINTERFACENUM::TEST1>::value)
				{
					switch (std::distance(iterStrBegin, iterStrEnd))
					{
					case actionLen:
						if (!std::equal(iterStrBegin, iterStrEnd, action, action + actionLen, 
							std::bind(std::equal_to<>(), std::bind(::std::tolower, std::placeholders::_1), std::placeholders::_2)))
							return false;

						*(des + HTTPINTERFACENUM::TEST1::action) = iterWordBegin;
						*(des + HTTPINTERFACENUM::TEST1::action + 1) = iterWordEnd;

						break;
					case functionLen:
						switch (*(iterStrBegin + 4))
						{
						case 't':
						case 'T':
							if (!std::equal(iterStrBegin, iterStrEnd, function, function + functionLen,
								std::bind(std::equal_to<>(), std::bind(::std::tolower, std::placeholders::_1), std::placeholders::_2)))
								return false;

							*(des + HTTPINTERFACENUM::TEST1::function) = iterWordBegin;
							*(des + HTTPINTERFACENUM::TEST1::function + 1) = iterWordEnd;

							break;
						case 'd':
						case 'D':
							if (!std::equal(iterStrBegin, iterStrEnd, funcData, funcData + funcDataLen, 
								std::bind(std::equal_to<>(), std::bind(::std::tolower, std::placeholders::_1), std::placeholders::_2)))
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

			iterBegin = iterWordEnd;
			if (iterWordEnd != iterEnd)
				++iterBegin;
		}
	}

	return true;
}




int main()
{
	const char *test{ "action=123&funcData&function=5236" };
	const unsigned int testLen{ strlen(test) };

	const char *test1{ "action=123&funcData&function=5236&rfghj" };
	const unsigned int test1Len{ strlen(test1) };

	const char *des[50]{};

	if (parseBody<HTTPINTERFACENUM::TEST1,3>(test, testLen, des, 50))
	{
		std::cout << "yes\n";
	}
	else
	{
		std::cout << "no\n";
	}

	if (parseBody<HTTPINTERFACENUM::TEST1, 3>(test1, test1Len, des, 50))
	{
		std::cout << "yes\n";
	}
	else
	{
		std::cout << "no\n";
	}

	return 0;
}


*/