#include "../../include/shell/shell.hpp"
#include "../../include/graphics/console.hpp"
#include "../../include/input/keyboard.hpp"
#include "../../include/fs/sertfs.hpp"
#include "../../include/disk/ata.hpp"
#include "../../include/memory/pmm.hpp"

namespace sertos::shell {

using namespace graphics;
using namespace input;
using namespace fs;

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

namespace {

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

void Shell::initialize() {
    sRunning = false;
    sInitialized = true;
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
        Console::setForeground(Color::red());
        Console::print("Unknown command: ");
        Console::println(argv[0]);
        Console::setForeground(Color::white());
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
    
    Console::println("Available commands:");
    Console::println("");
    
    for (usize i = 0; i < sCommandCount; i++) {
        Console::setForeground(Color::yellow());
        Console::print("  ");
        Console::print(sCommands[i].name);
        Console::setForeground(Color::white());
        
        usize nameLen = strLen(sCommands[i].name);
        for (usize j = nameLen; j < 12; j++) {
            Console::print(" ");
        }
        
        Console::println(sCommands[i].description);
    }
    Console::println("");
}

void Shell::cmdClear(int argc, char** argv) {
    (void)argc;
    (void)argv;
    Console::clear();
}

void Shell::cmdEcho(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) Console::print(" ");
        Console::print(argv[i]);
    }
    Console::println("");
}

void Shell::cmdPwd(int argc, char** argv) {
    (void)argc;
    (void)argv;
    Console::println(SertFs::currentDirectory());
}

void Shell::cmdCd(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "/";
    
    if (!SertFs::changeDirectory(path)) {
        Console::setForeground(Color::red());
        Console::print("cd: ");
        Console::print(path);
        Console::println(": No such directory");
        Console::setForeground(Color::white());
    }
}

void Shell::cmdLs(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : ".";
    
    DirHandle dir = SertFs::openDir(path);
    if (!dir.valid) {
        Console::setForeground(Color::red());
        Console::print("ls: ");
        Console::print(path);
        Console::println(": No such directory");
        Console::setForeground(Color::white());
        return;
    }
    
    DirEntry entry;
    while (SertFs::readDir(&dir, &entry)) {
        if (entry.type == FileType::Directory) {
            Console::setForeground(Color::blue());
        } else {
            Console::setForeground(Color::white());
        }
        Console::print(entry.name);
        
        if (entry.type == FileType::Directory) {
            Console::print("/");
        }
        Console::println("");
    }
    
    Console::setForeground(Color::white());
    SertFs::closeDir(&dir);
}

void Shell::cmdMkdir(int argc, char** argv) {
    if (argc < 2) {
        Console::println("Usage: mkdir <directory>");
        return;
    }
    
    if (!SertFs::createDirectory(argv[1])) {
        Console::setForeground(Color::red());
        Console::print("mkdir: ");
        Console::print(argv[1]);
        Console::println(": Failed to create directory");
        Console::setForeground(Color::white());
    }
}

void Shell::cmdTouch(int argc, char** argv) {
    if (argc < 2) {
        Console::println("Usage: touch <file>");
        return;
    }
    
    if (SertFs::exists(argv[1])) {
        return;
    }
    
    if (!SertFs::createFile(argv[1])) {
        Console::setForeground(Color::red());
        Console::print("touch: ");
        Console::print(argv[1]);
        Console::println(": Failed to create file");
        Console::setForeground(Color::white());
    }
}

void Shell::cmdRm(int argc, char** argv) {
    if (argc < 2) {
        Console::println("Usage: rm <file|directory>");
        return;
    }
    
    if (!SertFs::remove(argv[1])) {
        Console::setForeground(Color::red());
        Console::print("rm: ");
        Console::print(argv[1]);
        Console::println(": Failed to remove (file not found or directory not empty)");
        Console::setForeground(Color::white());
    }
}

void Shell::cmdCat(int argc, char** argv) {
    if (argc < 2) {
        Console::println("Usage: cat <file>");
        return;
    }
    
    FileHandle file = SertFs::open(argv[1], O_READ);
    if (!file.valid) {
        Console::setForeground(Color::red());
        Console::print("cat: ");
        Console::print(argv[1]);
        Console::println(": No such file");
        Console::setForeground(Color::white());
        return;
    }
    
    char buffer[256];
    i64 bytesRead;
    
    while ((bytesRead = SertFs::read(&file, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        Console::print(buffer);
    }
    
    Console::println("");
    SertFs::close(&file);
}

void Shell::cmdWrite(int argc, char** argv) {
    if (argc < 3) {
        Console::println("Usage: write <file> <text>");
        return;
    }
    
    FileHandle file = SertFs::open(argv[1], O_WRITE | O_CREATE | O_TRUNCATE);
    if (!file.valid) {
        Console::setForeground(Color::red());
        Console::print("write: ");
        Console::print(argv[1]);
        Console::println(": Failed to open file");
        Console::setForeground(Color::white());
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
        Console::println("Usage: mv <source> <destination>");
        return;
    }
    
    if (!SertFs::rename(argv[1], argv[2])) {
        Console::setForeground(Color::red());
        Console::print("mv: ");
        Console::println("Failed to move/rename");
        Console::setForeground(Color::white());
    }
}

void Shell::cmdCp(int argc, char** argv) {
    if (argc < 3) {
        Console::println("Usage: cp <source> <destination>");
        return;
    }
    
    FileHandle src = SertFs::open(argv[1], O_READ);
    if (!src.valid) {
        Console::setForeground(Color::red());
        Console::print("cp: ");
        Console::print(argv[1]);
        Console::println(": No such file");
        Console::setForeground(Color::white());
        return;
    }
    
    FileHandle dst = SertFs::open(argv[2], O_WRITE | O_CREATE | O_TRUNCATE);
    if (!dst.valid) {
        Console::setForeground(Color::red());
        Console::print("cp: ");
        Console::print(argv[2]);
        Console::println(": Failed to create destination");
        Console::setForeground(Color::white());
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
        Console::println("Usage: stat <path>");
        return;
    }
    
    FileInfo info;
    if (!SertFs::getInfo(argv[1], &info)) {
        Console::setForeground(Color::red());
        Console::print("stat: ");
        Console::print(argv[1]);
        Console::println(": No such file or directory");
        Console::setForeground(Color::white());
        return;
    }
    
    Console::print("  Name: ");
    Console::println(info.name);
    
    Console::print("  Type: ");
    switch (info.type) {
        case FileType::Regular:
            Console::println("Regular file");
            break;
        case FileType::Directory:
            Console::println("Directory");
            break;
        default:
            Console::println("Unknown");
            break;
    }
    
    Console::print("  Size: ");
    Console::printDec(info.size);
    Console::println(" bytes");
    
    Console::print("  Permissions: ");
    Console::printHex(info.permissions);
    Console::println("");
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
    
    Console::println(resolved);
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
            Console::print("    ");
        }
        Console::print("|-- ");
        
        if (entry.type == FileType::Directory) {
            Console::setForeground(Color::blue());
            Console::print(entry.name);
            Console::println("/");
            Console::setForeground(Color::white());
            
            char childPath[MAX_PATH];
            pathJoin(path, entry.name, childPath);
            printTree(childPath, depth + 1);
        } else {
            Console::println(entry.name);
        }
    }
    
    SertFs::closeDir(&dir);
}

void Shell::cmdMem(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    Console::println("Memory Information:");
    Console::print("  Total Pages: ");
    Console::printDec(memory::PMM::totalPages());
    Console::println("");
    
    Console::print("  Free Pages:  ");
    Console::printDec(memory::PMM::freePages());
    Console::println("");
    
    Console::print("  Used Pages:  ");
    Console::printDec(memory::PMM::totalPages() - memory::PMM::freePages());
    Console::println("");
    
    Console::print("  Free Memory: ");
    Console::printDec(memory::PMM::freeMemory() / MB);
    Console::println(" MB");
}

void Shell::cmdDf(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    if (!SertFs::isMounted()) {
        Console::setForeground(Color::red());
        Console::println("No filesystem mounted");
        Console::setForeground(Color::white());
        return;
    }
    
    Console::println("Filesystem Information:");
    
    u64 total = SertFs::totalSpace();
    u64 free = SertFs::freeSpace();
    u64 used = total - free;
    
    Console::print("  Total:  ");
    Console::printDec(total / MB);
    Console::println(" MB");
    
    Console::print("  Used:   ");
    Console::printDec(used / MB);
    Console::println(" MB");
    
    Console::print("  Free:   ");
    Console::printDec(free / MB);
    Console::println(" MB");
    
    if (total > 0) {
        Console::print("  Usage:  ");
        Console::printDec((used * 100) / total);
        Console::println("%");
    }
}

void Shell::cmdDisk(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    Console::println("Disk Information:");
    
    u8 driveCount = disk::ATA::driveCount();
    Console::print("  Detected drives: ");
    Console::printDec(driveCount);
    Console::println("");
    
    for (u8 i = 0; i < driveCount; i++) {
        disk::AtaDrive* drive = disk::ATA::getDrive(i);
        if (drive && drive->present) {
            Console::println("");
            Console::print("  Drive ");
            Console::printDec(i);
            Console::println(":");
            
            Console::print("    Model: ");
            Console::println(drive->model);
            
            Console::print("    Capacity: ");
            Console::printDec(disk::ATA::capacity(i) / MB);
            Console::println(" MB");
            
            Console::print("    LBA48: ");
            Console::println(drive->supportsLba48 ? "Yes" : "No");
        }
    }
}

}
