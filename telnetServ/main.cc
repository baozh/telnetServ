
#include "telnetServ.h"
#include <iostream>

TelnetServer gTelServ;


int testTimePrintf()
{
	gTelServ.telPrintf("[testTimePrintf] prover:%s, testNum:%d\n", "bbbb", 56);
	return 0;
}

int testNoTimePrintf()
{
	gTelServ.telPrintfNoTime("[testNoTimePrintf] prover:%s, testNum:%d\n", "cccccc", 56);
	return 0;
}

int main()
{
	//×¢²átelnetµ÷ÊÔÃüÁî
	gTelServ.regCommand("tt", (void *)testTimePrintf, "print dcServer version");
	gTelServ.regCommand("tn", (void *)testNoTimePrintf, "print dcServer version");


	gTelServ.init();

	int a;
	std::cin >> a;
}













