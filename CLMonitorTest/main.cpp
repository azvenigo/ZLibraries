#include <iostream>
#include <fstream>
#include "helpers/CommandLineParser.h"
#include "helpers/CommandLineMonitor.h"
#include "helpers/InlineFormatter.h"
#include "helpers/RandHelpers.h"
#include "helpers/StringHelpers.h"
#include <filesystem>

InlineFormatter gFormatter;

using namespace CLP;
using namespace std;


bool bVerbose = false;
bool bQuit = false;

void SampleLoop(int id)
{
    if (id == 1)
    {
        zout << "Regular line" << std::endl;
        zout << "WARNING: This is a warning" << std::endl;
        zout << "Regular line" << std::endl;
//        std::this_thread::sleep_for(std::chrono::microseconds(120000000));
    }


    int iterations = 0;
    while (!bQuit)
    {
        zout << "SampleLoop with id:" << id << " threadincrementer:" << iterations << std::endl;
        iterations += id;
        if (iterations % 10 == 0)
        {
            zout << "WARNING: This is a warning\n";
            std::this_thread::sleep_for(std::chrono::microseconds(120000 * (rand() % 10)));
//            return;
        }
        if (iterations % 21 == 0)
        {
            zout << "ERROR: This is an error\n";
            std::this_thread::sleep_for(std::chrono::microseconds(120000 * (rand() % 10)));
//            return;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(50000*(rand()%10)));
    }
};

int main(int argc, char* argv[])
{
//    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
//    SetConsoleMode(hConsole, ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    zout << "in the app now." << 12.7 << "\n";

    string sFilename;
    float fPercent = 1.0f;
    string sMood;

    CommandLineParser parser;
    parser.RegisterAppDescription("Test App for CommandLineMonitor");

    parser.RegisterParam(ParamDesc("PATH", &sFilename,  CLP::kRequired|CLP::kNoExistingPath, "This must be a path that does not exist."));

    parser.RegisterParam(ParamDesc("PERCENT", &fPercent, CLP::kOptional, "Some floating point number", -1.2f, 7.8f));

    parser.RegisterParam(ParamDesc("MOOD", &sMood, CLP::kOptional | CLP::kNamed, "Pick one", {"sad", "happy", "mad", "bored"}));


    bool bParseSuccess = parser.Parse(argc, argv);
    if (!bParseSuccess)
    {
        zerr << "Aborting.\n";
        return -1;
    }                
    
    std::thread worker1(SampleLoop, 1);
    std::thread worker2(SampleLoop, 2);
    std::thread worker3(SampleLoop, 3);

    zout << "running\n";
    CommandLineMonitor monitor;
    monitor.Start();

    zout << "Warning 1\n";

    zout << "      ***Warning*** 2\n";

    zout << "      nada 3           \n";

    zout << "      nada 4           \n";
    while (!monitor.IsDone())
    {
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
    monitor.End();

    bQuit = true;
    worker1.join();
    worker2.join();
    worker3.join();
    zout << "Done.";

    return 0;
}
