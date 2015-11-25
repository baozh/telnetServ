
#telnet监控线程

###功能

- 外部可通过telnet，输出打印信息到 telnet窗口中。
- 程序中可注册回调。外部可通过telnet，调用 之前注册的函数。这样可以：观看程序的状态，手动执行某个方法之类。

###使用方法

1. 添加头文件、定义全局变量、初始化

		#include <telnetServ/telnetServ.h>

	在main.cc中定义一个全局变量：
	
		TelnetServer gTelServ;

	在main函数中初始化：
	
		gTelServ.init();

2. 注册回调函数：

	例：
	定义一个回调函数：
	```cpp
	int connstat()
	{
		if (pdcserver != NULL)
		{
			pdcserver->getServerLoop()->runInLoop(boost::bind(&DCServer::printTelConnStat, pdcserver));
		}
		return 0;
	}
	```
	在main函数中注册此回调函数及提示信息（尤其是函数参数的意义）：
	
		gTelServ.regCommand("connstat", (void *)connstat, "print all conn stat");

3. 输出打印到 telnet终端：

		//不输出程序执行的当前时间（一般在telnet回调函数中打印的，都不需要打印时间.）
		gTelServ.telPrintfNoTime("[printTelConnStat] RealStat:%d, DisplayStat:%d\n", curContext->builtStat.realServerStat, curContext->builtStat.displayServerStat);

		//打印消息的同时，输出程序执行的当前时间
		gTelServ.telPrintf("[muduo::AnaUtil::rDataAnalyse] rData: %s\n\n", rdata);

4. 连接到telnet，看输出，调用回调函数

		telnet IP  2500         //端口默认是2500，如果被占用了，会向后叠加(2501, 2502,...)。
	
	可以看到程序输出到telnet窗口的打印。
	
	输入：telhelp    可看到注册到telnet的回调函数列表，帮助提示信息。
	
	输入：connstat   即执行注册的回调函数connstat

	注：向telnet注册的回调函数是在telnet线程环境中执行的，要注意它是否访问了共享数据，要保证它是线程安全的。

###编译

程序中用到了muduo库(线程相关、时间相关)、boost库，编译时需要链接这两个库。如果不想链接，可修改下相关代码，很容易整合到自己的程序中。


