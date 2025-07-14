/*#include<iostream>
#include<string>
#include<fstream>
#include<algorithm>
#include<functional>
#include<vector>
#include<memory>
#include<map>
#include<sstream>
#include<cctype>
#include<iterator>

using std::distance;
using std::stringstream;
using std::pair;
using std::shared_ptr;
using std::cout;
using std::ifstream;
using std::ofstream;
using std::vector;
using std::string;
using std::to_string;
using std::getline;
using std::isspace;
using std::remove_if;
using std::sort;
using std::mismatch;
using std::is_sorted;
using std::equal;

void makeSortFile(const char *inFile, const char *outFile, const char *errorFile, const int beginNum, const int endNum)
{
	if (!inFile || !outFile || !errorFile || endNum < beginNum)
		return;
	{
		ofstream(outFile, std::ios::trunc) << "";
	}
	{
		ofstream(errorFile, std::ios::trunc) << "";
	}

	ofstream filr(outFile, std::ios::binary | std::ios::app);
	if (!filr)
		return;
	ofstream filerr(errorFile, std::ios::binary | std::ios::app);
	if (!filerr)
		return;
	string fileName{ inFile };

	int index{}, errorIndex{}, num{};
	vector<shared_ptr<pair<ifstream, string>>>vec;
	for (index = beginNum; index != endNum; ++index)                                  //将可以成功打开的文件装进容器里面，方便下面循环处理
	{
		shared_ptr<pair<ifstream, string>>ptr{ std::make_shared<pair<ifstream, string>>() };
		ptr->first.open(fileName + to_string(index), std::ios::binary);
		if (!ptr->first)
			continue;
		vec.emplace_back(ptr);
	}
	if (vec.empty())
		return;

	stringstream vaildStream, errorStream;
	vector<string>result;
	pair<string::const_iterator, string::const_iterator>p1;
	index = errorIndex = 0;
	for (auto &elem : vec)
	{
		while (!elem->first.eof())
		{
			getline(elem->first, elem->second);
			if (elem->second.empty())
				continue;
			if (elem->second.size() >= 19 &&
				isdigit(elem->second[0]) && isdigit(elem->second[1]) && isdigit(elem->second[2]) && isdigit(elem->second[3])                 //year
				&& isdigit(elem->second[5]) && isdigit(elem->second[6]) && isdigit(elem->second[8]) && isdigit(elem->second[9])              //mon day
				&& isdigit(elem->second[11]) && isdigit(elem->second[12]) && isdigit(elem->second[14]) && isdigit(elem->second[15])
				&& isdigit(elem->second[17]) && isdigit(elem->second[18])
				&& elem->second[4] == '-' && elem->second[7] == '-' &&
				isspace(elem->second[10]) && elem->second[13] == ':' && elem->second[16] == ':')
			{
				result.emplace_back(elem->second);
				continue;
			}
			else
			{
				errorStream << std::move(elem->second) << '\n';
				if (++errorIndex == 100)
				{
					filerr << errorStream.str();
					errorStream.str("");
					errorStream.clear();
					errorIndex = 0;
				}
			}
		}
	}

	if (result.empty())
		return;

	sort(result.begin(), result.end(), [&p1](const string &left, const string &right)
	{
		p1 = mismatch(left.cbegin(), left.cbegin() + 19, right.cbegin(), right.cbegin() + 19);
		if (p1.first == left.cbegin() + 19)
			return false;
		return *p1.first < *p1.second;
	});

	for (auto const &i : result)
	{
		vaildStream << std::move(i) << '\n';
		if (++index == 100)
		{
			filr << vaildStream.str();
			vaildStream.str("");
			vaildStream.clear();
			index = 0;
		}
	}

	if (index)
	{
		filr << vaildStream.str();
	}
	if (errorIndex)
	{
		filerr << errorStream.str();
	}
}




int main()
{
	makeSortFile("D:\\deng\\log", "D:\\1.txt", "D:\\error.txt", 0, 16);
	cout << "yes\n";

	return 0;
}

*/