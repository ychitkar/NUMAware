#include "args.h"

void ArgParser :: ParseCommandLine(Thread *ThreadPtr,int &argc, char **argv)
{
    int CpuCountPerNumaNode=0,OptionIndex,Param,BufferValIndex,RunTimeIndex,Thread=-1,Proc=-1;
    struct bitmask *cpus;
    char *end_ptr;
    if(cmdOptionExists("--fencing"))
    {
        FencingEn = true;
    }
    if(cmdOptionExists("--prefetch"))
    {
        PrefetchDis = true;
    }

    /* ****** Get NUMAware Command Line Options ******  */

    Param = getopt_long(argc, argv, CtgShortOptions, CtgLongOptions, &OptionIndex);

    while(Param != -1)
    {

        if(0 && (errno != 0 || end_ptr == NULL || (*end_ptr != '\0')))
        {
            printf("option -%c with an invalid input = %s\n", Param, string(optarg).c_str());
            exit(0);
        }

        switch(Param)
        {
            case 't': /* Time of Execution */
                printf("Option -t with value %s\n",optarg);

                if(strchr(string(optarg).c_str(),'s'))              /* Seconds */
                {
                    RunTimeIndex = strchr(optarg,'s') - optarg;
                    RunTime = (double)stoi(string( (string(optarg).erase(RunTimeIndex,strlen(optarg)-1)) ));
                }
                else if(strchr(string(optarg).c_str(),'m'))         /* Minutes */
                {
                    RunTimeIndex = strchr(optarg,'m') - optarg;
                    RunTime = (double)stoi(string( (string(optarg).erase(RunTimeIndex,strlen(optarg)-1)) ));
                    RunTime = RunTime * 60.0;
                }
                else if(strchr(string(optarg).c_str(),'h'))         /* Hours */
                {
                    RunTimeIndex = strchr(optarg,'h') - optarg;
                    RunTime = (double)stoi(string( (string(optarg).erase(RunTimeIndex,strlen(optarg)-1)) ));
                    RunTime = RunTime * 3600.0;
                }
                else
                {
                    RunTime = (double)stoi(string(optarg));
                }

                RunIter = 0;
                break;

            case 'S': /* CPU Socket Enable */
                printf("Option --sockets/-S with value %s\n",optarg);
                if(stoi(optarg) > (numa_num_configured_nodes() - 1) || stoi(optarg) < 0)
                {
                    cerr << "Socket Count Mismatch!\nMin Socket Count is 0\nMax Socket Count is " << (numa_num_configured_nodes() - 1) << endl;
                    exit(0);
                }

                /* ****** Enable CPU Socket ****** */

                ThreadPtr->mDevice.core = true;

                Proc = stoi(optarg);
                numa_run_on_node(Proc);              /* Run Process on specific NUMA Node */
                
                cpus = numa_allocate_cpumask();
                numa_node_to_cpus(Proc,cpus);        /* Get CPUs belonging to NUMA Node */
                for(int i = 0;i<cpus->size;i++)
                {
                    if(numa_bitmask_isbitset(cpus,i))
                    {
                        if(CpuCountPerNumaNode == 0)
                            Thread = i;
                        CpuCountPerNumaNode++;
                    }
                }

                break;

            case 'C': /* CPU Cores/Threads Enable */
                printf("Option --cores/-C with value %s\n",optarg);

                if(stoi(optarg) > numa_num_configured_cpus() || stoi(optarg) < 0)
                {
                    cerr << "Core/Thread Count Mismatch!\nMin Core count is 0\nMax Core Count is " << numa_num_configured_cpus() << endl;
                    exit(0);
                }
                else
                {
                    if(1)
                    {
                        /* Hyper-Threaded Core Count restarts from beginning Node after Physical Core Count Expires */

                        if(stoi(optarg) < (CpuCountPerNumaNode) || stoi(optarg) == CpuCountPerNumaNode)
                        {
                            for(j=Thread;j<(Thread + stoi(optarg));j++)
                            {
                                if(numa_bitmask_isbitset(cpus,j))
                                {
                                    ThreadPtr->mDevice.DeviceInfo.push_back(make_pair(Proc,j));
                                    ThreadPtr->mOpcode.insert({make_pair(Proc,j),make_pair(-1,-1)});
                                }
                            }
                        }
                        else
                        {
                            cerr << "Please Keep Core Count less than or equal to " << CpuCountPerNumaNode << endl;
                            exit(0);
                        }
                    }
                    else
                    {
                        cerr << "Cannot enable Core Threads without enabling Sockets.\n"
                             << "Run ./ctg --help cmdline for more information"
                             << endl;
                        exit(0);
                    }
                }

                break;

            case 'b': /* Memory Buffer Enable */
                printf("Option --buffer/-b with value %s\n",optarg);

                if(strchr(string(optarg).c_str(),'g'))         /* Gigabyte Buffer Space */
                {
                    BufferValIndex = strchr(optarg,'g') - optarg;
                    ThreadPtr->Tgt->MemTgt.Size = (uint64_t)stoi(string( (string(optarg).erase(BufferValIndex,strlen(optarg)-1)) ));
                    printf("Working on %lld Gigabytes of Memory.\n",ThreadPtr->Tgt->MemTgt.Size);
                    ThreadPtr->Tgt->MemTgt.Size = ThreadPtr->Tgt->MemTgt.Size * 1024 * 1024 * 1024;
                }
                else if(strchr(string(optarg).c_str(),'m'))    /* Megabyte Buffer Space */
                {
                    BufferValIndex = strchr(optarg,'m') - optarg;
                    ThreadPtr->Tgt->MemTgt.Size = (uint64_t)stoi(string( (string(optarg).erase(BufferValIndex,strlen(optarg)-1)) ));
                    printf("Working on %lld Megabytes of Memory.\n",ThreadPtr->Tgt->MemTgt.Size);
                    ThreadPtr->Tgt->MemTgt.Size = ThreadPtr->Tgt->MemTgt.Size * 1024 * 1024;
                }
                else if(strchr(string(optarg).c_str(),'k'))    /* KiloByte Buffer Space */
                {
                    BufferValIndex = strchr(optarg,'k') - optarg;
                    ThreadPtr->Tgt->MemTgt.Size = (uint64_t)stoi(string( (string(optarg).erase(BufferValIndex,strlen(optarg)-1)) ));
                    printf("Working on %lld Kilobytes of Memory.\n",ThreadPtr->Tgt->MemTgt.Size);
                    ThreadPtr->Tgt->MemTgt.Size = ThreadPtr->Tgt->MemTgt.Size * 1024;
                }
                else
                {
                    cerr << "Invalid Buffer Size!\nValid Buffer sizes are g/m/k only!\n";
                    exit(0);
                } 
                break;
        }
    }
}

const string& ArgParser :: getCmdOption(const string &option)
{
    vector<string>::const_iterator itr;
    itr =  find(this->tokens.begin(), this->tokens.end(), option);
    if (itr != this->tokens.end() && ++itr != this->tokens.end())
        return *itr;
    static const string EmptyStr("");
    return EmptyStr;
}

bool ArgParser :: cmdOptionExists(const string &option)
{
    return find(this->tokens.begin(), this->tokens.end(), option) != this->tokens.end();
}