
telnet����̣߳�
���ܣ�
��1���ⲿ��ͨ��telnet�������ӡ��Ϣ�� telnet�����С�
��2�������п�ע��ص����ⲿ��ͨ��telnet������ ֮ǰע��ĺ������������ԣ��ۿ������״̬���ֶ�ִ��ĳ������֮�ࡣ

ʹ�÷�����
1. ���ͷ�ļ�������ȫ�ֱ�������ʼ��
#include <telnetServ/telnetServ.h>

��main.cc�ж���һ��ȫ�ֱ�����
TelnetServer gTelServ;

��main�����г�ʼ����
gTelServ.init();

2.ע��ص�������
����
����һ���ص�������
int connstat()
{
	if (pdcserver != NULL)
	{
		pdcserver->getServerLoop()->runInLoop(boost::bind(&DCServer::printTelConnStat, pdcserver));
	}
	return 0;
}
��main������ע��˻ص���������ʾ��Ϣ�������Ǻ������������壩��
gTelServ.regCommand("connstat", (void *)connstat, "print all conn stat");

3.�����ӡ�� telnet�նˣ�
//���������ִ�еĵ�ǰʱ�䣨һ����telnet�ص������д�ӡ�ģ�������Ҫ��ӡʱ��.��
gTelServ.telPrintfNoTime("[printTelConnStat] RealStat:%d, DisplayStat:%d\n", curContext->builtStat.realServerStat, curContext->builtStat.displayServerStat);

//��ӡ��Ϣ��ͬʱ���������ִ�еĵ�ǰʱ��
gTelServ.telPrintf("[muduo::AnaUtil::rDataAnalyse] rData: %s\n\n", rdata);

4.���ӵ�telnet������������ûص�����
telnet IP  2500         //�˿�Ĭ����2500�������ռ���ˣ���������(2501, 2502,...)��
���Կ������������telnet���ڵĴ�ӡ��
���룺telhelp    �ɿ���ע�ᵽtelnet�Ļص������б�������ʾ��Ϣ.
���룺connstat   ��ִ��ע��Ļص�����connstat


ע����telnetע��Ļص���������telnet�̻߳�����ִ�еģ�Ҫע�����Ƿ�����˹������ݣ�Ҫ��֤�����̰߳�ȫ�ġ�



������
2015.6.29
