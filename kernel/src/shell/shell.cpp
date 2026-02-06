#include "../../include/shell/shell.hpp"
#include "../../include/graphics/console.hpp"
#include "../../include/wm/wm.hpp"
#include "../../include/input/keyboard.hpp"
#include "../../include/fs/sertfs.hpp"
#include "../../include/disk/ata.hpp"
#include "../../include/memory/pmm.hpp"

namespace sertos::shell {

using namespace graphics;
using namespace input;
using namespace fs;

namespace {

void shellPrint(const char* str) {
    if (Shell::sPrintCallback) {
        Shell::sPrintCallback(str);
    } else if (wm::WindowManager::isInitialized()) {
        wm::GraphicalConsole::print(str);
    } else {
        Console::print(str);
    }
}

void shellPrintln(const char* str) {
    shellPrint(str);
    shellPrint("\n");
}

void shellPrintDec(u64 value) {
    if (value == 0) {
        shellPrint("0");
        return;
    }
    char buffer[21];
    int i = 20;
    buffer[i] = '\0';
    while (value > 0 && i > 0) {
        buffer[--i] = '0' + (value % 10);
        value /= 10;
    }
    shellPrint(&buffer[i]);
}

void shellPrintHex(u64 value) {
    const char* hexChars = "0123456789ABCDEF";
    char buffer[19];
    buffer[0] = '0';
    buffer[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        buffer[17 - i] = hexChars[(value >> (i * 4)) & 0xF];
    }
    buffer[18] = '\0';
    shellPrint(buffer);
}

void shellSetForeground(Color color) {
    if (Shell::sSetColorCallback) {
        Shell::sSetColorCallback(color);
    } else if (wm::WindowManager::isInitialized()) {
        wm::GraphicalConsole::setForeground(color);
    } else {
        Console::setForeground(color);
    }
}

void shellClear() {
    if (Shell::sClearCallback) {
        Shell::sClearCallback();
    } else if (wm::WindowManager::isInitialized()) {
        wm::GraphicalConsole::clear();
    } else {
        Console::clear();
    }
}

usize strLen(const char* str) {
    usize len = 0;
    while (str[len]) len++;
    return len;
}

int strCompare(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

bool isWhitespace(char c) {
    return c == ' ' || c == '\t';
}

void pathJoin(const char* base, const char* relative, char* result) {
    usize baseLen = strLen(base);
    usize relLen = strLen(relative);
    usize i = 0;
    
    while (i < baseLen && i < MAX_PATH - 1) {
        result[i] = base[i];
        i++;
    }
    
    if (i > 0 && result[i-1] != '/' && i < MAX_PATH - 1) {
        result[i++] = '/';
    }
    
    usize j = 0;
    while (j < relLen && i < MAX_PATH - 1) {
        result[i++] = relative[j++];
    }
    
    result[i] = '\0';
}

}

const Command Shell::sCommands[] = {
    {"help", "Display available commands", cmdHelp},
    {"clear", "Clear the screen", cmdClear},
    {"echo", "Print text to console", cmdEcho},
    {"pwd", "Print working directory", cmdPwd},
    {"cd", "Change directory", cmdCd},
    {"ls", "List directory contents", cmdLs},
    {"mkdir", "Create a directory", cmdMkdir},
    {"touch", "Create an empty file", cmdTouch},
    {"rm", "Remove a file or empty directory", cmdRm},
    {"cat", "Display file contents", cmdCat},
    {"write", "Write text to a file", cmdWrite},
    {"mv", "Move/rename a file or directory", cmdMv},
    {"cp", "Copy a file", cmdCp},
    {"stat", "Display file information", cmdStat},
    {"tree", "Display directory tree", cmdTree},
    {"mem", "Display memory information", cmdMem},
    {"df", "Display disk space information", cmdDf},
    {"disk", "Display disk information", cmdDisk},
};

usize Shell::sCommandCount = sizeof(sCommands) / sizeof(sCommands[0]);
bool Shell::sRunning = false;
bool Shell::sInitialized = false;
char Shell::sArgBuffer[MAX_ARGS][MAX_ARG_LENGTH];
ShellPrintCallback Shell::sPrintCallback = nullptr;
ShellPutCharCallback Shell::sPutCharCallback = nullptr;
ShellSetColorCallback Shell::sSetColorCallback = nullptr;
ShellClearCallback Shell::sClearCallback = nullptr;

void Shell::initialize() {
    sRunning = false;
    sInitialized = true;
    sPrintCallback = nullptr;
    sPutCharCallback = nullptr;
    sSetColorCallback = nullptr;
    sClearCallback = nullptr;
}

void Shell::setOutputCallbacks(ShellPrintCallback print, ShellPutCharCallback putChar,
                               ShellSetColorCallback setColor, ShellClearCallback clear) {
    sPrintCallback = print;
    sPutCharCallback = putChar;
    sSetColorCallback = setColor;
    sClearCallback = clear;
}

void Shell::clearOutputCallbacks() {
    sPrintCallback = nullptr;
    sPutCharCallback = nullptr;
    sSetColorCallback = nullptr;
    sClearCallback = nullptr;
}

void Shell::run() {
    if (!sInitialized) {
        initialize();
    }
    
    sRunning = true;
    char cmdLine[MAX_COMMAND_LENGTH];
    
    Console::println("");
    Console::setForeground(Color::cyan());
    Console::println("SertOS Shell v1.0");
    Console::println("Type 'help' for available commands.");
    Console::setForeground(Color::white());
    Console::println("");
    
    while (sRunning) {
        printPrompt();
        
        if (Keyboard::readLine(cmdLine, MAX_COMMAND_LENGTH)) {
            if (cmdLine[0] != '\0') {
                executeCommand(cmdLine);
            }
        }
    }
}

void Shell::executeCommand(const char* cmdLine) {
    char* argv[MAX_ARGS];
    int argc = parseCommand(cmdLine, argv);
    
    if (argc == 0) return;
    
    const Command* cmd = findCommand(argv[0]);
    if (cmd) {
        cmd->handler(argc, argv);
    } else {
        shellSetForeground(Color::red());
        shellPrint("Unknown command: ");
        shellPrintln(argv[0]);
        shellSetForeground(Color::white());
    }
}

void Shell::printPrompt() {
    Console::setForeground(Color::green());
    Console::print("sertos");
    Console::setForeground(Color::white());
    Console::print(":");
    Console::setForeground(Color::blue());
    Console::print(SertFs::currentDirectory());
    Console::setForeground(Color::white());
    Console::print("$ ");
}

int Shell::parseCommand(const char* cmdLine, char** argv) {
    int argc = 0;
    const char* p = cmdLine;
    
    while (*p && static_cast<usize>(argc) < MAX_ARGS) {
        while (*p && isWhitespace(*p)) p++;
        
        if (!*p) break;
        
        usize i = 0;
        bool inQuote = false;
        char quoteChar = 0;
        
        while (*p && i < MAX_ARG_LENGTH - 1) {
            if (!inQuote && (*p == '"' || *p == '\'')) {
                inQuote = true;
                quoteChar = *p;
                p++;
                continue;
            }
            
            if (inQuote && *p == quoteChar) {
                inQuote = false;
                p++;
                continue;
            }
            
            if (!inQuote && isWhitespace(*p)) {
                break;
            }
            
            sArgBuffer[argc][i++] = *p++;
        }
        
        sArgBuffer[argc][i] = '\0';
        argv[argc] = sArgBuffer[argc];
        argc++;
    }
    
    return argc;
}

const Command* Shell::findCommand(const char* name) {
    for (usize i = 0; i < sCommandCount; i++) {
        if (strCompare(sCommands[i].name, name) == 0) {
            return &sCommands[i];
        }
    }
    return nullptr;
}

void Shell::cmdHelp(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    shellPrintln("Available commands:");
    shellPrintln("");
    
    for (usize i = 0; i < sCommandCount; i++) {
        shellSetForeground(Color::yellow());
        shellPrint("  ");
        shellPrint(sCommands[i].name);
        shellSetForeground(Color::white());
        
        usize nameLen = strLen(sCommands[i].name);
        for (usize j = nameLen; j < 12; j++) {
            shellPrint(" ");
        }
        
        shellPrintln(sCommands[i].description);
    }
    shellPrintln("");
}

void Shell::cmdClear(int argc, char** argv) {
    (void)argc;
    (void)argv;
    shellClear();
}

void Shell::cmdEcho(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) shellPrint(" ");
        shellPrint(argv[i]);
    }
    shellPrintln("");
}

void Shell::cmdPwd(int argc, char** argv) {
    (void)argc;
    (void)argv;
    shellPrintln(SertFs::currentDirectory());
}

void Shell::cmdCd(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "/";
    
    if (!SertFs::changeDirectory(path)) {
        shellSetForeground(Color::red());
        shellPrint("cd: ");
        shellPrint(path);
        shellPrintln(": No such directory");
        shellSetForeground(Color::white());
    }
}

void Shell::cmdLs(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : ".";
    
    DirHandle dir = SertFs::openDir(path);
    if (!dir.valid) {
        shellSetForeground(Color::red());
        shellPrint("ls: ");
        shellPrint(path);
        shellPrintln(": No such directory");
        shellSetForeground(Color::white());
        return;
    }
    
    DirEntry entry;
    while (SertFs::readDir(&dir, &entry)) {
        if (entry.type == FileType::Directory) {
            shellSetForeground(Color::blue());
        } else {
            shellSetForeground(Color::white());
        }
        shellPrint(entry.name);
        
        if (entry.type == FileType::Directory) {
            shellPrint("/");
        }
        shellPrintln("");
    }
    
    shellSetForeground(Color::white());
    SertFs::closeDir(&dir);
}

void Shell::cmdMkdir(int argc, char** argv) {
    if (argc < 2) {
        shellPrintln("Usage: mkdir <directory>");
        return;
    }
    
    if (!SertFs::createDirectory(argv[1])) {
        shellSetForeground(Color::red());
        shellPrint("mkdir: ");
        shellPrint(argv[1]);
        shellPrintln(": Failed to create directory");
        shellSetForeground(Color::white());
    }
}

void Shell::cmdTouch(int argc, char** argv) {
    if (argc < 2) {
        shellPrintln("Usage: touch <file>");
        return;
    }
    
    if (SertFs::exists(argv[1])) {
        return;
    }
    
    if (!SertFs::createFile(argv[1])) {
        shellSetForeground(Color::red());
        shellPrint("touch: ");
        shellPrint(argv[1]);
        shellPrintln(": Failed to create file");
        shellSetForeground(Color::white());
    }
}

void Shell::cmdRm(int argc, char** argv) {
    if (argc < 2) {
        shellPrintln("Usage: rm <file|directory>");
        return;
    }
    
    if (!SertFs::remove(argv[1])) {
        shellSetForeground(Color::red());
        shellPrint("rm: ");
        shellPrint(argv[1]);
        shellPrintln(": Failed to remove (file not found or directory not empty)");
        shellSetForeground(Color::white());
    }
}

void Shell::cmdCat(int argc, char** argv) {
    if (argc < 2) {
        shellPrintln("Usage: cat <file>");
        return;
    }
    
    FileHandle file = SertFs::open(argv[1], SERTFS_O_READ);
    if (!file.valid) {
        shellSetForeground(Color::red());
        shellPrint("cat: ");
        shellPrint(argv[1]);
        shellPrintln(": No such file");
        shellSetForeground(Color::white());
        return;
    }
    
    char buffer[256];
    i64 bytesRead;
    
    while ((bytesRead = SertFs::read(&file, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        shellPrint(buffer);
    }
    
    shellPrintln("");
    SertFs::close(&file);
}

void Shell::cmdWrite(int argc, char** argv) {
    if (argc < 3) {
        shellPrintln("Usage: write <file> <text>");
        return;
    }
    
    FileHandle file = SertFs::open(argv[1], SERTFS_O_WRITE | SERTFS_O_CREATE | SERTFS_O_TRUNCATE);
    if (!file.valid) {
        shellSetForeground(Color::red());
        shellPrint("write: ");
        shellPrint(argv[1]);
        shellPrintln(": Failed to open file");
        shellSetForeground(Color::white());
        return;
    }
    
    for (int i = 2; i < argc; i++) {
        if (i > 2) {
            SertFs::write(&file, " ", 1);
        }
        SertFs::write(&file, argv[i], strLen(argv[i]));
    }
    SertFs::write(&file, "\n", 1);
    
    SertFs::close(&file);
}

void Shell::cmdMv(int argc, char** argv) {
    if (argc < 3) {
        shellPrintln("Usage: mv <source> <destination>");
        return;
    }
    
    if (!SertFs::rename(argv[1], argv[2])) {
        shellSetForeground(Color::red());
        shellPrint("mv: ");
        shellPrintln("Failed to move/rename");
        shellSetForeground(Color::white());
    }
}

void Shell::cmdCp(int argc, char** argv) {
    if (argc < 3) {
        shellPrintln("Usage: cp <source> <destination>");
        return;
    }
    
    FileHandle src = SertFs::open(argv[1], SERTFS_O_READ);
    if (!src.valid) {
        shellSetForeground(Color::red());
        shellPrint("cp: ");
        shellPrint(argv[1]);
        shellPrintln(": No such file");
        shellSetForeground(Color::white());
        return;
    }
    
    FileHandle dst = SertFs::open(argv[2], SERTFS_O_WRITE | SERTFS_O_CREATE | SERTFS_O_TRUNCATE);
    if (!dst.valid) {
        shellSetForeground(Color::red());
        shellPrint("cp: ");
        shellPrint(argv[2]);
        shellPrintln(": Failed to create destination");
        shellSetForeground(Color::white());
        SertFs::close(&src);
        return;
    }
    
    char buffer[512];
    i64 bytesRead;
    
    while ((bytesRead = SertFs::read(&src, buffer, sizeof(buffer))) > 0) {
        SertFs::write(&dst, buffer, bytesRead);
    }
    
    SertFs::close(&src);
    SertFs::close(&dst);
}

void Shell::cmdStat(int argc, char** argv) {
    if (argc < 2) {
        shellPrintln("Usage: stat <path>");
        return;
    }
    
    FileInfo info;
    if (!SertFs::getInfo(argv[1], &info)) {
        shellSetForeground(Color::red());
        shellPrint("stat: ");
        shellPrint(argv[1]);
        shellPrintln(": No such file or directory");
        shellSetForeground(Color::white());
        return;
    }
    
    shellPrint("  Name: ");
    shellPrintln(info.name);
    
    shellPrint("  Type: ");
    switch (info.type) {
        case FileType::Regular:
            shellPrintln("Regular file");
            break;
        case FileType::Directory:
            shellPrintln("Directory");
            break;
        default:
            shellPrintln("Unknown");
            break;
    }
    
    shellPrint("  Size: ");
    shellPrintDec(info.size);
    shellPrintln(" bytes");
    
    shellPrint("  Permissions: ");
    shellPrintHex(info.permissions);
    shellPrintln("");
}

void Shell::cmdTree(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : ".";
    
    char resolved[MAX_PATH];
    if (path[0] == '/') {
        usize i = 0;
        while (path[i] && i < MAX_PATH - 1) {
            resolved[i] = path[i];
            i++;
        }
        resolved[i] = '\0';
    } else {
        pathJoin(SertFs::currentDirectory(), path, resolved);
    }
    
    shellPrintln(resolved);
    printTree(resolved, 0);
}

void Shell::printTree(const char* path, int depth) {
    DirHandle dir = SertFs::openDir(path);
    if (!dir.valid) return;
    
    DirEntry entry;
    while (SertFs::readDir(&dir, &entry)) {
        if (entry.name[0] == '.' && (entry.name[1] == '\0' || 
            (entry.name[1] == '.' && entry.name[2] == '\0'))) {
            continue;
        }
        
        for (int i = 0; i < depth; i++) {
            shellPrint("    ");
        }
        shellPrint("|-- ");
        
        if (entry.type == FileType::Directory) {
            shellSetForeground(Color::blue());
            shellPrint(entry.name);
            shellPrintln("/");
            shellSetForeground(Color::white());
            
            char childPath[MAX_PATH];
            pathJoin(path, entry.name, childPath);
            printTree(childPath, depth + 1);
        } else {
            shellPrintln(entry.name);
        }
    }
    
    SertFs::closeDir(&dir);
}

void Shell::cmdMem(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    shellPrintln("Memory Information:");
    shellPrint("  Total Pages: ");
    shellPrintDec(memory::PMM::totalPages());
    shellPrintln("");
    
    shellPrint("  Free Pages:  ");
    shellPrintDec(memory::PMM::freePages());
    shellPrintln("");
    
    shellPrint("  Used Pages:  ");
    shellPrintDec(memory::PMM::totalPages() - memory::PMM::freePages());
    shellPrintln("");
    
    shellPrint("  Free Memory: ");
    shellPrintDec(memory::PMM::freeMemory() / MB);
    shellPrintln(" MB");
}

void Shell::cmdDf(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    if (!SertFs::isMounted()) {
        shellSetForeground(Color::red());
        shellPrintln("No filesystem mounted");
        shellSetForeground(Color::white());
        return;
    }
    
    shellPrintln("Filesystem Information:");
    
    u64 total = SertFs::totalSpace();
    u64 free = SertFs::freeSpace();
    u64 used = total - free;
    
    shellPrint("  Total:  ");
    shellPrintDec(total / MB);
    shellPrintln(" MB");
    
    shellPrint("  Used:   ");
    shellPrintDec(used / MB);
    shellPrintln(" MB");
    
    shellPrint("  Free:   ");
    shellPrintDec(free / MB);
    shellPrintln(" MB");
    
    if (total > 0) {
        shellPrint("  Usage:  ");
        shellPrintDec((used * 100) / total);
        shellPrintln("%");
    }
}

void Shell::cmdDisk(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    shellPrintln("Disk Information:");
    
    u8 driveCount = disk::ATA::driveCount();
    shellPrint("  Detected drives: ");
    shellPrintDec(driveCount);
    shellPrintln("");
    
    for (u8 i = 0; i < driveCount; i++) {
        disk::AtaDrive* drive = disk::ATA::getDrive(i);
        if (drive && drive->present) {
            shellPrintln("");
            shellPrint("  Drive ");
            shellPrintDec(i);
            shellPrintln(":");
            
            shellPrint("    Model: ");
            shellPrintln(drive->model);
            
            shellPrint("    Capacity: ");
            shellPrintDec(disk::ATA::capacity(i) / MB);
            shellPrintln(" MB");
            
            shellPrint("    LBA48: ");
            shellPrintln(drive->supportsLba48 ? "Yes" : "No");
        }
    }
}

}
