#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "telnetServ.h"
#include <muduo/base/TimeZone.h>
#include <muduo/base/Timestamp.h>
#include <time.h>
#include <poll.h>
#include <boost/bind.hpp>


/* Telnet server's port range */
const uint16_t MIN_TELSVR_PORT = 2500;
const uint16_t MAX_TELSVR_PORT = 8000;


const int MAX_COMMAND_LENGTH = 2048;   //����telnet�������󳤶�
const char NEWLINE_CHAR = '\n';
const char BACKSPACE_CHAR = 8;
const char BLANK_CHAR = ' ';
const char RETURN_CHAR = 13;
const char TAB_CHAR = 9;
const char DEL_CHAR = 127;
const char CTRL_S = 19;
const char CTRL_R = 18;
const char UP_ARROW = 27;
const char DOWN_ARROW = 28;
const char LEFT_ARROW = 29;
const char RIGHT_ARROW = 30;

#define SOCKET_ERROR		   (-1)
#define BACK_LOG_SIZE          15
#define MAX_LOG_MSG_LEN        2000         //ÿ����־����󳤶�

static muduo::TimeZone sLocalZone(::getenv("MUDUO_TZ_PATH")?::getenv("MUDUO_TZ_PATH"):"/etc/localtime");

namespace telnet_serv
{
__thread char t_time[25];
__thread time_t t_lastSecond;

int TelnetServer::createTcpListenSock(uint16_t wPort)
{
	int tSock = INVALID_SOCKET;
	sockaddr_in tSvrINAddr;

	memset( &tSvrINAddr, 0, sizeof(tSvrINAddr) );

	// set up the local address 
	tSvrINAddr.sin_family = AF_INET; 
	tSvrINAddr.sin_port = htons(wPort);
	tSvrINAddr.sin_addr.s_addr = INADDR_ANY;

	//Allocate a socket
	tSock = socket(AF_INET, SOCK_STREAM, 0);
	if(tSock == INVALID_SOCKET)
	{
		std::cout<<"[TELNET] Tcp server can't create socket!"<<std::endl;
		return INVALID_SOCKET;
	}

	if(bind(tSock, (sockaddr *)&tSvrINAddr, sizeof(tSvrINAddr)) == SOCKET_ERROR)
	{
		std::cout<<"[TELNET] PassiveTcp: bind error!"<<std::endl;
		close(tSock);
		return INVALID_SOCKET;
	}

	if(listen(tSock, BACK_LOG_SIZE) == SOCKET_ERROR) // max 15 waiting connection
	{
		std::cout<<"[TELNET] PassiveTcp can't listen on port:"<<wPort<<std::endl;
		close(tSock);
		return INVALID_SOCKET;
	}
	return tSock;
}

void TelnetServer::PromptShow()
{
	if (isHaveTelnetClient() == false)
	{
		return;
	}
	char prompt[20] = {0};	/* "%s->"*/
	sprintf(prompt, "->");

	sendMsgToTerminal(prompt, strlen(prompt) + 1);	
}

bool TelnetServer::isNullStr(const char* pchCmd)
{
	bool isNull = true;
	while(*pchCmd != '\0')
	{
		if (*pchCmd != ' ')
		{
			isNull =  false;
			break;
		}
	}
	return isNull;
}

void TelnetServer::TelnetDaemonFun(uint16_t wPort)
{
#define TELE_FD_NUM 3

	int epfd = 0;
	int nfds = 0;
	int nEpIndex = 0;
	struct epoll_event ev, events[TELE_FD_NUM];

	sockaddr_in addrClient;
	int addrLenIn = sizeof(addrClient);
	char cmdChar = 0;
	char command[MAX_COMMAND_LENGTH] = {0};
	unsigned char cmdLen = 0;

	epfd = epoll_create(TELE_FD_NUM);

	/* ���ָ���˿ںţ���ΪTelnet�������ڸö˿ں��ϴ���һ���׽��� */
	if(wPort != 0)
	{
		sockServ_ = createTcpListenSock(wPort); // server's port
		if(sockServ_ != INVALID_SOCKET) 
		{
			portListen_ = wPort;
		}
	}

	/* ���δָ���˿ںŻ���ָ���˿ں��ϴ����׽���ʧ�ܣ������д��� */
	if(sockServ_ == INVALID_SOCKET)
	{
		for(int port=MIN_TELSVR_PORT; port<MAX_TELSVR_PORT; port++)
		{
			sockServ_ = createTcpListenSock(port); // server's port
			if(sockServ_ != INVALID_SOCKET) 
			{
				portListen_ = port;
				break;
			}			
		}
	}

	if(sockServ_ == INVALID_SOCKET)
	{
		static int nRetCode = 0;
		pthread_exit(&nRetCode);
		return;
	}

	/* listen ��socketӦ������Ϊ������*/
	uint32_t on = 1;
	ioctl(sockServ_, FIONBIO, &on);

	//�����û�����
	ev.data.fd = sockServ_;
	ev.events = EPOLLIN;
	epoll_ctl(epfd, EPOLL_CTL_ADD, sockServ_, &ev);

	while(isRunning_)
	{       
		int ret;
		nfds = epoll_wait(epfd, events, TELE_FD_NUM, 500);
		for(nEpIndex = 0; nEpIndex < nfds; ++nEpIndex)
		{
			if ((events[nEpIndex].events & EPOLLHUP) && !(events[nEpIndex].events & EPOLLIN)
				&& (sockClient_ == events[nEpIndex].data.fd))
			{
				printf("OspTeleDaemon: peer closed\n");
				close(sockClient_);
				setInvalidTelnetClient();
				continue;
			}

			if (events[nEpIndex].events & (EPOLLERR | POLLNVAL)
				&& (sockClient_ == events[nEpIndex].data.fd))
			{
				printf("OspTeleDaemon: peer closed\n");
				close(sockClient_);
				setInvalidTelnetClient();
				continue;
			}

			if(events[nEpIndex].data.fd == sockServ_)
			{
				int newTelHandle = accept(sockServ_, (sockaddr *)&addrClient, (socklen_t*)&addrLenIn);
				if (newTelHandle == INVALID_SOCKET)
				{
					std::cout<<"[TELNET] TeleDaemon : Telnet Server Accept Error! errorNo:"<<errno<<std::endl;
					continue;
				}
				if (sockClient_!= INVALID_SOCKET)
				{
					close(sockClient_);
				}
				setTelnetClient(newTelHandle);

				ev.data.fd = sockClient_;
				ev.events = EPOLLIN;
				epoll_ctl(epfd, EPOLL_CTL_ADD, sockClient_,&ev);

				memset(command,0,MAX_COMMAND_LENGTH);

				/* �����ӭ���� */
				pushMsgToTerminalImmidate("%s\n", std::string("*===============================================================*").c_str());
				pushMsgToTerminalImmidate("%s\n", std::string("Welcome to Telnet Server.").c_str());
				pushMsgToTerminalImmidate("%s\n", std::string("*===============================================================*").c_str());

				PromptShow();
				continue;
			}
			else if(events[nEpIndex].events & EPOLLIN && (sockClient_ == events[nEpIndex].data.fd))
			{
				//�����ؽ����û�����
				ret = recv(sockClient_, &cmdChar, 1, 0);

				//�ͻ��˹ر�
				if(ret <= 0)
				{
					printf("OspTeleDaemon: peer closed\n");
					close(sockClient_);
					setInvalidTelnetClient();
					continue;
				}

				//���˹ر�
				if(ret == SOCKET_ERROR)
				{
					printf("SOCKET_ERROR.\n");
					close(sockClient_);
					setInvalidTelnetClient();
					usleep(500*1000);
					continue;
				}		

				//�����û�����, ���Ե�Telnet�ͻ�����Ļ��, ������������ʵ�����Ӧ
				switch(cmdChar)
				{
				case CTRL_S:
					{

					}
					break;
				case CTRL_R:
					{

					}
					break;
				case RETURN_CHAR:		  // �س���
					{
						std::string str(command, cmdLen);
						pushMsgToTerminalImmidate("%s\r\n", str.c_str());
						if (isNullStr(command) == false)
						{
							CmdParse(command, cmdLen);   //parse ����������ִ������
						}

						cmdLen = 0;
						memset(command,0,MAX_COMMAND_LENGTH);
						PromptShow();		  // ��ʾ��ʾ��
					}
					break;
				case NEWLINE_CHAR:
				case UP_ARROW:			  // �ϼ�ͷ
				case DOWN_ARROW:		  // �¼�ͷ
				case LEFT_ARROW:		  // ���ͷ
				case RIGHT_ARROW:		  // �Ҽ�ͷ
					break;

				case BACKSPACE_CHAR:		 // �˸��
					{
						if(cmdLen <= 0)
						{
							continue;
						}

						cmdLen--;	
						if(cmdLen >= 0 && cmdLen < MAX_COMMAND_LENGTH )
						{			 
							command[cmdLen] = '\0';
						}
						/* ʹ�����ˣ���һ���ո����ԭ�ַ�����ʹ������ */
						char tmpChar[3] = {0};
						tmpChar[0] = BACKSPACE_CHAR;
						tmpChar[1] = BLANK_CHAR;
						tmpChar[2] = BACKSPACE_CHAR;

						pushMsgToTerminalImmidate("%s", std::string(tmpChar, 3).c_str());
					}
					break;
				default:
					{
						if(cmdLen < MAX_COMMAND_LENGTH)
						{				
							command[cmdLen++] = cmdChar;	
						}
						else
						{				 
							pushMsgToTerminalImmidate("\n");
							std::cout << "default cmd:" << command << ", cmdLen:" << (int)cmdLen << std::endl;
							CmdParse(command, cmdLen);	

							PromptShow();		  // ��ʾ��ʾ��
							cmdLen = 0;
							memset(command,0,MAX_COMMAND_LENGTH);
						}
					}
					break;
				}
			}
			else
			{
				printf("TeleDaemon: system error, maybe epoll_wait event %d don't match EPOLLIN %d OR g_sockClient %d != events[nEpIndex].data.fd %d\n", 
					events[nEpIndex].events, EPOLLIN, sockClient_, events[nEpIndex].data.fd);
				close(sockClient_);
				setInvalidTelnetClient();
				usleep(500*1000);
			}
		} 
	}
}

void TelnetServer::CmdParse(const char* pchCmd, unsigned char byCmdLen)
{  
	unsigned char count = 0;
	int nCpyLen = 0;
	char command[MAX_COMMAND_LENGTH] = {0};
	//int count = 0;
	if(byCmdLen > 0)
	{       
		//ȥͷ
		for(count=0; count<byCmdLen; count++)
		{
			char chTmp;
			chTmp = pchCmd[count];
			if (isdigit(chTmp) || islower(chTmp) || isupper(chTmp))
			{
				break;
			}
		}

		nCpyLen = byCmdLen-count;
	}

	if ( nCpyLen > MAX_COMMAND_LENGTH )
	{
		printf("osp bug in cmdparse\n");
		return ;
	}

	memcpy(command, pchCmd+count, nCpyLen);   
	if(byCmdLen < MAX_COMMAND_LENGTH)
	{
		command[nCpyLen] = '\0';
	}
	else
	{
		command[MAX_COMMAND_LENGTH-1] = '\0';
	}

	RunCmd(command);
}

typedef struct 
{
	char *paraStr;   //�洢ÿ������ֵ ָ��ĵ�ַ
	bool bInQuote;
	bool bIsChar;
}TRawPara;

void TelnetServer::RunCmd(char *szCmd)
{ 
	if (isHaveTelnetClient() == false)
	{
		return;
	}

	uint64_t para[10] = {0};
	TRawPara atRawPara[10];
	int paraNum = 0;
	unsigned char count = 0;
	unsigned char chStartCnt = 0;
	bool bStrStart = false;
	bool bCharStart = false;
	uint32_t cmdLen = strlen(szCmd)+1;

    memset(para, 0, sizeof(para));
    memset(atRawPara, 0, sizeof(TRawPara)*10);

    /* �������������� */
    while( count < cmdLen )
    {	
		switch(szCmd[count])
		{
		case '\'':
			szCmd[count] = '\0';
			if(!bCharStart)
			{
				chStartCnt = count;
			}
			else
			{
				if(count > chStartCnt + 2)
				{
					printf("[TelnetServer::RunCmd] input error.\n");
					return;
				}
			}
			bCharStart = !bCharStart;
			break;

		case '\"':
			szCmd[count] = '\0';
			bStrStart = !bStrStart;
			break;

		case ',':
		case ' ':
		case '\t':
		case '\n':
		case '(':
		case ')':
			if(!bStrStart)
			{
				szCmd[count] = '\0';
			}
			break;

		default:
			/* ������ַ�Ϊ��Ч�ַ���ǰһ�ַ�ΪNULL����ʾ�ɵ��ʽ������µ��ʿ�ʼ */
			if(count > 0 && szCmd[count-1] == '\0')
			{				
				atRawPara[paraNum].paraStr = &szCmd[count];
				if(bStrStart)
				{
					atRawPara[paraNum].bInQuote = true;
				}
				if(bCharStart)
				{
					atRawPara[paraNum].bIsChar = true;
				}
				if( ++paraNum >= 10)
					break;
			}
		}//end switch
		count++;
    }//end while

    if(bStrStart || bCharStart)
    {
		printf("input error. bStrStart or bCharStart is true.\n");
		return;
    }

    for(count = 0; count < 10; count++)
    {
		if(atRawPara[count].paraStr == NULL)
		{
			para[count] = 0;
			continue;
		}

		if(atRawPara[count].bInQuote)
		{
			para[count] = (uint64_t)atRawPara[count].paraStr;   //�����⣬���ܽ�ָ��ֱ��ת�ͳ�int
			continue;
		}

		if(atRawPara[count].bIsChar)
		{
			para[count] = (char)atRawPara[count].paraStr[0];
			continue;
		}

		para[count] = WordParse(atRawPara[count].paraStr);
    }

    /* ��ִ������ */
    if (strcmp("bye", szCmd) == 0 )
    {
		pushMsgToTerminalImmidate("\n  bye......\n");
		usleep(500*1000);
		close(sockClient_);
		setInvalidTelnetClient();
		return;
    }    

    if (strcmp("telhelp", szCmd) == 0)
    {
	    telhelp();
	    return;
    } 

    UniformFunc func = FindCommand(szCmd);
    if (func != NULL)
    {
	    int ret = (*func)(para[0],para[1],para[2],para[3],para[4],para[5],para[6],para[7],para[8],para[9]);
	    pushMsgToTerminalImmidate("Return value: %d\n", ret);
    } 
    else
    {
	    pushMsgToTerminalImmidate("function '%s' doesn't exist!\n", szCmd);
    }
    return;
}

UniformFunc TelnetServer::FindCommand(const char* name)
{
	std::string funName(name);
	if (cmdFunTable_.find(funName) != cmdFunTable_.end())
	{
		return cmdFunTable_[funName].cmdFunc_;
	}
	return NULL;
}

void TelnetServer::telhelp()
{
	if (isHaveTelnetClient() == false)
	{
		return;
	}

	pushMsgToTerminalImmidate("Whole User Help Menu:\n\n");

	std::map<std::string, TCmdTable>::iterator iter = cmdFunTable_.begin();
	for (; iter != cmdFunTable_.end(); iter++)
	{
		pushMsgToTerminalImmidate("Command: %s, Usage:%s \n", iter->second.funName_.c_str(), iter->second.funUsage_.c_str());
	}
}

void TelnetServer::regCommand(const char* name, void* func, const char* usage)
{
	TCmdTable cmdInfo;
	cmdInfo.funName_ = name;
	cmdInfo.funUsage_ = usage;
	cmdInfo.cmdFunc_ = (UniformFunc)func;
	cmdFunTable_[cmdInfo.funName_] = cmdInfo;
}

int TelnetServer::WordParse(const char* word)
{
	int tmp = 0;
	if(word == NULL) return 0;

	tmp = atoi(word);
	if(tmp == 0 && word[0] != '0')
	{
		return (uint64_t)word;
	}
	return tmp;
}


bool TelnetServer::init(int flushInterval /*= 1*/, uint16_t telnetPort/* = 2500*/)
{
	isRunning_ = true;
	flushInterval_ = flushInterval;

	//���� �߳�
	thdTelnetDaemonHandle_ = new muduo::Thread(boost::bind(&TelnetServer::TelnetDaemonFun, this, telnetPort), "TelnetDaemonThread");
	thdLogOutPutHandle_ = new muduo::Thread(boost::bind(&TelnetServer::LogOutPutFun, this), "TelnetLogOutPutThread");

	thdTelnetDaemonHandle_->start();
	thdLogOutPutHandle_->start();

	return true;
}

bool TelnetServer::destory()
{
	isRunning_ =  false;
	cond_.notify();

	if (thdTelnetDaemonHandle_ != NULL)
	{
		thdTelnetDaemonHandle_->join();

		delete thdTelnetDaemonHandle_;
		thdTelnetDaemonHandle_ = NULL;
	}

	if (thdLogOutPutHandle_ != NULL)
	{
		thdLogOutPutHandle_->join();

		delete thdLogOutPutHandle_;
		thdLogOutPutHandle_ = NULL;
	}

	portListen_ = 0;
	sockClient_ = INVALID_SOCKET;
	sockServ_ = INVALID_SOCKET;
	cmdFunTable_.clear();

	return true;
}

void TelnetServer::appendToMsgBuf(const char* logline, int len)
{
	if (isHaveTelnetClient() == false)
	{
		return;
	}

	/* �����Ϣ����, �ض� */
	if(len > MAX_LOG_MSG_LEN)
	{
		std::cout<<"[TELNET][appendToMsgBuf] log message too long to output."<<std::endl;
		return;
	}

	{
		muduo::MutexLockGuard lock(mutex_);
		if (currentBuffer_->avail() > len)
		{
			currentBuffer_->append(logline, len);
		}
		else
		{
			buffers_.push_back(currentBuffer_.release());

			if (nextBuffer_)
			{
				currentBuffer_ = boost::ptr_container::move(nextBuffer_);
			}
			else
			{
				currentBuffer_.reset(new Buffer); // Rarely happens
			}
			currentBuffer_->append(logline, len);
			cond_.notify();
		}
	}
}

void TelnetServer::LogOutPutFun()
{
	assert(isRunning_ == true);
	BufferPtr newBuffer1(new Buffer);
	BufferPtr newBuffer2(new Buffer);
	newBuffer1->bzero();
	newBuffer2->bzero();
	boost::ptr_vector<Buffer> buffersToWrite;
	buffersToWrite.reserve(8);

	while (isRunning_)
	{
		assert(newBuffer1 && newBuffer1->length() == 0);
		assert(newBuffer2 && newBuffer2->length() == 0);
		assert(buffersToWrite.empty());

		{
			muduo::MutexLockGuard lock(mutex_);
			if (buffers_.empty())
			{
				cond_.waitForSeconds(flushInterval_);
			}
			buffers_.push_back(currentBuffer_.release());
			currentBuffer_ = boost::ptr_container::move(newBuffer1);
			buffersToWrite.swap(buffers_);
			if (!nextBuffer_)
			{
				nextBuffer_ = boost::ptr_container::move(newBuffer2);
			}
		}

		assert(!buffersToWrite.empty());

		if (buffersToWrite.size() > 25)
		{
			std::cout<<"[TELNET][LogOutPutFun] too many telnet messages, so drop tail messages."<<std::endl;
			buffersToWrite.erase(buffersToWrite.begin(), buffersToWrite.end() - 2);
		}

		for (size_t i = 0; i < buffersToWrite.size(); ++i)
		{ 
			if (buffersToWrite[i].length() != 0)
			{
				sendMsgToTerminal(buffersToWrite[i].data(), buffersToWrite[i].length());
			}
		}

		if (buffersToWrite.size() > 2)
		{
			// drop non-bzero-ed buffers, avoid trashing
			buffersToWrite.resize(2);
		}

		if (!newBuffer1)
		{
			assert(!buffersToWrite.empty());
			newBuffer1 = buffersToWrite.pop_back();
			newBuffer1->resetData();
		}

		if (!newBuffer2)
		{
			assert(!buffersToWrite.empty());
			newBuffer2 = buffersToWrite.pop_back();
			newBuffer2->resetData();
		}

		buffersToWrite.clear();
	}
}


/*====================================================================
  ���ܣ���Telnet��Ļ�ϴ�ӡ��NULL��β���ַ���
  �㷨ʵ�֣����ַ��������ʵ���ת������Ҫ�ǰ�'\n'ת��Ϊ'\r\n'��
  ���͵�Telnet�ͻ���.
  ====================================================================*/
bool TelnetServer::sendMsgToTerminal(const char *pchMsg, int msgLen)
{
	if (isHaveTelnetClient() == false)
	{
		return true;
	}

    char chCur;
    uint32_t dwStart = 0;
    uint32_t dwCount = 0;
    char *pchRetStr = "\n\r";
    bool bSendOK = false;
	int localSock = sockClient_;

    if( (pchMsg == NULL) || (localSock == INVALID_SOCKET) )
    {
		return false;
    }

	bSendOK = SockSend(localSock, &pchMsg[dwStart], msgLen);
	if( !bSendOK )
	{
		return false;
	}

  //  while(dwCount < msgLen)
  //  {
		//chCur = pchMsg[dwCount];

		///* ����'\n'��'\0', �����ǰһ��'\n'����'\n'֮��������ַ� */
		//if(chCur == '\0' || chCur == '\n' || dwCount == msgLen - 1)
		//{
		//    bSendOK = SockSend(localSock, &pchMsg[dwStart], dwCount-dwStart);
		//    if( !bSendOK )
		//    {
		//		return false;
		//    }

		//    /* ����'\n', ���"\r\n" */
		//    if(chCur == '\n')
		//    {
		//		bSendOK = SockSend(localSock, pchRetStr, 2);
		//		if( !bSendOK )
		//		{
		//		    return false;
		//		}
		//    }

		//    /* ����'\0', ��ʾ�ַ�������Ӧ����ѭ�� */
		//    if(chCur == '\0' || dwCount == msgLen - 1)
		//    {
		//		break;
		//    }

		//    /* ��һ��������� */
		//    dwStart = dwCount+1;
		//}
		//dwCount++;
  //  }
    return true;
}


bool TelnetServer::SockSend(int tSock, const char* pchBuf, uint32_t dwLen)
{   
	int ret = SOCKET_ERROR;
	int nTotalSendLen = 0;
	int nTryNum = 0;

	if((tSock == INVALID_SOCKET) || (pchBuf == NULL))
	{
		return false;
	}

	nTotalSendLen = 0;
	while (nTotalSendLen < dwLen)
	{
		//����ʧ��ԭ��Ϊ�ײ�û��Bufʱ��Ҫ���³��Է���3��
		for(nTryNum = 0; nTryNum < 3; nTryNum++)
		{
			ret = send(tSock, (char*)(pchBuf + nTotalSendLen), dwLen - nTotalSendLen, MSG_NOSIGNAL);
			if(ret == SOCKET_ERROR)
			{
				std::cout<<"[TELNET][SockSend] send failed!"<<std::endl;
			}
			else
			{
				break;
			}
		}
		nTotalSendLen += ret;
	}
	return true;
}

void TelnetServer::telPrintfNoTime(char *szFormat, ...)
{
	if (isHaveTelnetClient() == false)
	{
		return;
	}

	if(szFormat == NULL)
	{
		return;
	}

	uint32_t actLen = 0;
	va_list pvList;
	char msg[MAX_LOG_MSG_LEN] = {0};

	va_start(pvList, szFormat);
	actLen = vsprintf(msg, szFormat, pvList);
	va_end(pvList);

	if(actLen <= 0 || actLen >= MAX_LOG_MSG_LEN)
	{
		std::cout<<"[TELNET][telPrintfNoTime] vsprintf() failed!"<<std::endl;
		return;
	}
	appendToMsgBuf(msg, actLen);
}

void TelnetServer::pushMsgToTerminalImmidate(char *szFormat, ...)
{
	if (isHaveTelnetClient() == false)
	{
		return;
	}

	if(szFormat == NULL)
	{
		return;
	}

	uint32_t actLen = 0;
	va_list pvList;
	char msg[MAX_LOG_MSG_LEN] = {0};

	va_start(pvList, szFormat);
	actLen = vsprintf(msg, szFormat, pvList);
	va_end(pvList);

	if(actLen <= 0 || actLen >= MAX_LOG_MSG_LEN)
	{
		std::cout<<"[TELNET][telPrintfNoTime] vsprintf() failed!"<<std::endl;
		return;
	}
	sendMsgToTerminal(msg, actLen);
}
void TelnetServer::telPrintf(char *szFormat, ...)
{
	if (isHaveTelnetClient() == false)
	{
		return;
	}

	if(szFormat == NULL)
	{
		return;
	}

	uint32_t actLen = 0;
	va_list pvList;
	char msg[MAX_LOG_MSG_LEN] = {0};

	va_start(pvList, szFormat);
	uint32_t timeStrLen = formatTime(msg);
	actLen = vsnprintf(msg + timeStrLen, MAX_LOG_MSG_LEN - timeStrLen, szFormat, pvList);
	va_end(pvList);

	if(actLen <= 0 || timeStrLen + actLen >= MAX_LOG_MSG_LEN)
	{
		std::cout<<"[TELNET][TelPrintf] vsprintf() failed!"<<std::endl;
		return;
	}
	appendToMsgBuf(msg, timeStrLen + actLen);
}

uint32_t TelnetServer::formatTime(char *pszMsg)
{
	if (pszMsg == NULL)
		return NULL;

	time_t seconds = muduo::Timestamp::now().secondsSinceEpoch();
	if (seconds != t_lastSecond)
	{
		t_lastSecond = seconds;

		struct tm tm_time = sLocalZone.toLocalTime(seconds);
		uint32_t len = snprintf(t_time, sizeof(t_time), "[%4d-%02d-%02d %02d:%02d:%02d]",
			tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
			tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
		assert(len == 21);
	}
	memcpy(pszMsg, t_time, 21);
	return 21;
}

}
