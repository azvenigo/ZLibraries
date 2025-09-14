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
/*    Table test;
    Table::Style headerStyleLeft(COL_ORANGE, Table::RIGHT, Table::TIGHT, 2, '-');
    Table::tCellArray blah;
    string sBorderChar(COL_BG_GRAY "*" COL_RESET);
    test.SetBorders(sBorderChar, "\x1b(0q\x1b(B", sBorderChar, sBorderChar, sBorderChar);
    blah.push_back(Table::Cell("hello", headerStyleLeft));
    test.AddRow(blah);
    test.AlignWidth(40);

    zout << (string) test;

    zout << "in the app now." << 12.7 << "\n";*/
    
/*
    for (int i = 96; i < 256; i++)
    {
        zout << "[0x" << std::hex << i << "] " << DEC_LINE_START << (char)i << DEC_LINE_END << "\n";
    }
    
  */  







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
        return -1;
    }                
    
    std::vector<std::thread> workers;

    const int kThreads = 3;

    for (int i = 0; i < kThreads; i++)
    {
        std::thread worker(SampleLoop, i);
        workers.emplace_back(std::move(worker));
    }

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

    for (int i = 0; i < kThreads; i++)
    {
        workers[i].join();
    }

    zout << "Done.";

    return 0;
}
