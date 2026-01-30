#pragma once

#include "../types.hpp"
#include "../fs/sertfs.hpp"

namespace sertos::shell {

constexpr usize MAX_COMMAND_LENGTH = 256;
constexpr usize MAX_ARGS = 16;
constexpr usize MAX_ARG_LENGTH = 128;

struct Command {
    const char* name;
    const char* description;
    void (*handler)(int argc, char** argv);
};

class Shell {
public:
    static void initialize();
    static void run();
    static void executeCommand(const char* cmdLine);

private:
    static void printPrompt();
    static int parseCommand(const char* cmdLine, char** argv);
    static const Command* findCommand(const char* name);
    
    static void cmdHelp(int argc, char** argv);
    static void cmdClear(int argc, char** argv);
    static void cmdEcho(int argc, char** argv);
    static void cmdPwd(int argc, char** argv);
    static void cmdCd(int argc, char** argv);
    static void cmdLs(int argc, char** argv);
    static void cmdMkdir(int argc, char** argv);
    static void cmdTouch(int argc, char** argv);
    static void cmdRm(int argc, char** argv);
    static void cmdCat(int argc, char** argv);
    static void cmdWrite(int argc, char** argv);
    static void cmdMv(int argc, char** argv);
    static void cmdCp(int argc, char** argv);
    static void cmdStat(int argc, char** argv);
    static void cmdTree(int argc, char** argv);
    static void cmdMem(int argc, char** argv);
    static void cmdDf(int argc, char** argv);
    static void cmdDisk(int argc, char** argv);
    
    static void printTree(const char* path, int depth);
    
    static const Command sCommands[];
    static usize sCommandCount;
    static bool sRunning;
    static bool sInitialized;
    static char sArgBuffer[MAX_ARGS][MAX_ARG_LENGTH];
};

}
