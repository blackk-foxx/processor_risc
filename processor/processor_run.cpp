#include <systemc.h>
#include <unistd.h>

#include "processor.cpp"

#include <sstream>
#include <iostream>
#include <string.h>
#include <vector>
#include <map>

int sc_main(int argn, char * argc[])
{
	processor p("PROC", argc[1]);
    sc_start();

    cout << endl;
    cout << "Completed in " << p.CONTROL.getCycleCount() << " cycles." << endl;
    cout << endl;    
	cout << "/******* REGISTERS ********/" << endl;
	p.REGISTERS.print(10);
	cout << "/******* MEMORY ********/" << endl;
	p.DM.print(10);
	return 0;
}
