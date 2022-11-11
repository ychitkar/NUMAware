#include "main.h"
using namespace std;

int main(int argc, char *argv[])
{
    int ThreadUpperLimit;
    unsigned long long int ClkPerSec;
    double NSecClk;
    uint64_t StartIterTick,StopIterTick;
    Thread *iThread = new Thread();
    Thread *oThread;
    ArgParser *InputArgs = new ArgParser(argc,argv);

    InputArgs->ParseCommandLine(iThread,argc,argv);

    if(iThread->isCPU())
    {
        oThread = new Thread_Cores(iThread);
        oThread->InitializeThread();
        delete(iThread);
    }

    Calibrate(&ClkPerSec,NSecClk);

    if(!(numa_available()))
    {
        for(int Node = 0;Node<numa_num_configured_nodes();Node++)
        {
            oThread->BuildThreadAddressTable(Node);
            oThread->SetupThread();
            StartIterTick = rdtsc();
            oThread->StartThread();
            if(RunTime)
            {
                printf("Going to Sleep for %f seconds.\n",RunTime);
                sleep(RunTime);
                oThread->StopThread(true);
            }
            else
            {
                printf("Waiting for %d iterations to complete.\n",RunIter);
                oThread->StopThread(false);
                StopIterTick = rdtsc();

                RunTime = (double)(StopIterTick - StartIterTick)/ClkPerSec;
            }
        }
    }
    return 0;
}