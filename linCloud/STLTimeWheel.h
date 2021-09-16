#pragma once


//�ο�����ʱ���ֵĸ����ʵ�֣�������������ʵ�ָ���������Ч��
//ÿһ��ʱ�������ڴ洢������Ϊһ�����������ڵ�ÿһ���ڵ���ΪeverySecondFunctionNumber���ص���������
//���Ŀ�����ڿռ����������������
//�����ڵ�ǰĿ���������������һ���½ڵ㣬���ص���������
//������ʧ�����ԷŽ�Ŀ����+1��Ŀռ��ڣ�ֱ�������붼װ��Ϊֹ��������ã�����ʧ��
//���ÿ����ֻ��Ҫά����βָ�뼴�ɣ����ú͸�λ����Ϊ��Ч

//�����ȵ�������ڵ�װ�ص����ص����������Ч
//�ȴ���̬���� ����ʱ��Ҫ˫���ռ�Ҫʡ�ռ�


#include <memory>
#include <functional>
#include <tuple>
#include <mutex>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <chrono>
#include <forward_list>

struct STLTimeWheel
{
	//�������ö�ʱ����io_context
	//��ʱ��ÿ����ת��ʱʱ��
	//��ʱ��������
	//ÿ����ʱ���������ɻص������ĸ�������
	STLTimeWheel(std::shared_ptr<boost::asio::io_context> ioc,
		const unsigned int checkSecond = 1 ,const unsigned int wheelNum = 60, const unsigned int everySecondFunctionNumber = 1000);



	//���뺯�� 
	//turnNum ��ʾ�ӵ�ǰm_WheelInfoIter����ת���
	bool insert(const std::function<void()> &callBack ,const unsigned int turnNum);




private:
	struct WheelInformation
	{
		//װ�ػص������ĵ�����
		//�ڵ�����ΪeverySecondFunctionNumber���ص���������
		std::forward_list<std::unique_ptr<std::function<void()>[]>> functionList{};

		//����׷�ӽڵ�ʱ�õĵ�����
		std::forward_list<std::unique_ptr<std::function<void()>[]>>::const_iterator listAppendIter{ functionList.cbefore_begin() };

		//��ǰ�����ڸ��ӽڵ�λ��
		std::forward_list<std::unique_ptr<std::function<void()>[]>>::const_iterator listIter{};

		//ÿ�������ڸ��ӵ�ǰ��Чָ�������ַ
		std::function<void()> *bufferIter{};
	};


	//��ʼִ��
	void start();




private:
	//��ʱ��
	std::unique_ptr<boost::asio::steady_timer> m_timer{};  

	//��ʱ��ÿ����ת��ʱʱ��
	int m_checkSecond{};

	//��ʱ��������
	int m_wheelNum{};

	//ÿ����ʱ���������ɻص������ĸ�������
	int m_everySecondFunctionNumber{};

	//������
	std::mutex m_mutex;

	//װ�ش洢������Ϣ������
	std::unique_ptr<WheelInformation[]>m_WheelInformationPtr{};

	//�׵�ַ
	WheelInformation *m_WheelInfoBegin{};

	//β��ַ
	WheelInformation *m_WheelInfoEnd{};

	//��ǰ��ַ
	WheelInformation *m_WheelInfoIter{};

	bool m_hasInit{false};
};
