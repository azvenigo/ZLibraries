// TempThreadPoolApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <future>
#include <thread>
#include <chrono>
#include "helpers/ThreadPool.h"
#include "helpers/CommandLineParser.h"

using namespace std;

class SearchJobResult
{
public:
    SearchJobResult(uint64_t nSum = 0) : mnSum(nSum) {}

    uint64_t mnSum;
};

ThreadPool computePool(2);

uint64_t ComputeProc(uint64_t nValue)
{
    uint64_t nResult = 0;
    for (uint64_t i = 0; i < nValue; i++)
        nResult += rand();

    std::this_thread::sleep_for(std::chrono::microseconds(1000));
    cout << "computeproc:" << nValue << "...done\n";

    return nResult;
}


SearchJobResult   SearchProc(uint64_t nStart, uint64_t nMod)
{
    vector<shared_future<uint64_t> > computeResults;

    for (int i = 0; i < 10; i++)
        computeResults.emplace_back(computePool.enqueue(&ComputeProc, (nStart*i) %nMod));


    SearchJobResult result;
    for (int i = 0; i < 10; i++)
        result.mnSum += computeResults[i].get();

    cout << "searchproc start:" << nStart << " mod:" << nMod << "...done\n";


    return result;
}



using namespace CLP;

int main(int argc, char* argv[])
{
    CommandLineParser parser;

    string sSourcePath;
    bool bVerbose = false;

    parser.RegisterAppDescription("Does a variety of things, really. It tests the command line parser for one.Outputs usage based on command.... list's all available, etc.");

    parser.RegisterMode("list", "Lists stuff, I guess.");
    parser.RegisterParam("list", ParamDesc("SOURCE_PATH", &sSourcePath, CLP::kPositional | CLP::kRequired, "Test parameter."));
    parser.RegisterParam("list", ParamDesc("verbose", &bVerbose, CLP::kNamed | CLP::kOptional, "Test bool parameter."));

    parser.RegisterMode("copy", "copies stuff, naturally. This is a long description of what this command does. Super duper duper long.\nmultiline even.");
    parser.RegisterParam("copy", ParamDesc("SOURCE_PATH", &sSourcePath, CLP::kPositional | CLP::kRequired, "Test parameter."));
    parser.RegisterParam("copy", ParamDesc("DEST_PATH", &sSourcePath, CLP::kPositional | CLP::kRequired, "Test parameter."));
    parser.RegisterParam("copy", ParamDesc("verbose", &bVerbose, CLP::kNamed | CLP::kOptional, "Test bool parameter."));
    
    parser.RegisterParam(ParamDesc("verbose", &bVerbose, CLP::kNamed | CLP::kOptional, "Test bool parameter."));


    if (!parser.Parse(argc, argv, true))
    {
         return -1;
    }

    ThreadPool pool(5);
    vector<shared_future<SearchJobResult> > jobResults;

    for (int i = 0; i < 1; i++)
    {
        jobResults.emplace_back(pool.enqueue(&SearchProc, i * 1000, 1+i/14));
/*        int nStart = i * 1000;
        jobResults.emplace_back(pool.enqueue([=]
            {
                SearchJobResult result(nStart);
                for (int i = 0; i < rand(); i++)
                    result.mnSum += rand();


                return result;

            }));*/
    }

    for (auto result : jobResults)
        cout << result.get().mnSum << ",";

    return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
