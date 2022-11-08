#pragma once
#include<getopt.h>
#include<stdlib.h>
#include<string.h>

using namespace std;

const char ShortOptions[] =
    "h"    /* Help    */
    "S"    /* Select CPU Sockets */
    "C"    /* Select CPU Cores per Socket */
    "b"    /* Memory Buffer Size for Cores */
    "P"    /* Prefetch Flag */
    "f"    /* Fencing Flag */

class ArgParser
{
public:
    ArgParser(int &argc, char **argv)
    {
        if(argc < 2)
        { 
            cerr << "Insufficient Command Line Parameters.\n";
            exit(0);
        }
        else
        {
            PrefetchDis = false;
            FencingEn   = false;
            for(int i=1;i<argc;i++)
                this->tokens.push_back(string(argv[i]));

            RunTime = 4.0;
            RunIter = 0;
        }
    }
    void ParseCommandLine(Thread *ThreadPtr,int &argc, char **argv); 
    const string& getCmdOption(const string& option);
    bool cmdOptionExists(const string& option);

private:
    vector<string> tokens;
};