#include <iostream>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string>
#include <vector>
#include <cstring>

void printVector(std::vector<std::string> vec) {
    std::cout << "{";
    for (int i = 0; i < (int) vec.size(); i ++) {
        std::cout << vec[i];
        if (i != (int) vec.size() - 1) std::cout << ", ";
    }
    std::cout << "}" << std::endl;
}

bool accessExists(std::string path) {
    if (access(path.c_str(), X_OK) == 0) return true;
    return false;
}

bool fileExists(std::string path) {
    if (access(path.c_str(), R_OK) == 0) return true;
    return false;
}

void errorMessage(bool redirect) {
    char error_message[30] = "An error has occurred\n";
    if (redirect) {
        write(STDOUT_FILENO, error_message, strlen(error_message));
    } else {
        write(STDERR_FILENO, error_message, strlen(error_message)); 
    }
}

void execUnderlyingCmd(std::string path, std::vector<std::string> args, bool isRedirect) {

    int stdout_fd = dup(STDOUT_FILENO);

    if (isRedirect) {
        std::string redirectPath = args[int(args.size()) - 1];
        int fd;

        if (fileExists(redirectPath)) {
            fd = open(redirectPath.c_str(), O_WRONLY);
        } else {
            fd = open(redirectPath.c_str(), O_CREAT | O_WRONLY);
        }
        
        dup2(fd, STDOUT_FILENO);
    }

    int diff = 0;
    if (isRedirect) diff = -2;

    char* execArgs[int(args.size()) + 1 + diff];
    // std::cout << "args: ";
    for (int i = 0; i < int(args.size()) + diff; i ++) {
        execArgs[i] = (char*) args[i].c_str();
        // std::cout << args[i] << " ";
    }
    // std::cout << std::endl;
    execArgs[args.size() + diff] = NULL;
    execvp(path.c_str(), execArgs);

    if (isRedirect) dup2(stdout_fd, STDOUT_FILENO);
    return;
}

void execExit() {
    exit(0);
}

void execCd(std::string dir) {
    chdir(dir.c_str());
}

void execBuiltIn(std::vector<std::string> args, std::vector<std::string> &paths) {
    std::string cmd = args[0];
    bool isRedirect = args.size() > 2 && args[args.size() - 2] == ">";

    if (cmd == "exit") {
        if ((int) args.size() != 1) {
            errorMessage(isRedirect);
        }
        execExit();
    } else if (cmd == "cd") {
        if ((int) args.size() != 2) {
            errorMessage(isRedirect);
        } else {
            execCd(args[1]);
        }
    } else if (cmd == "path") {
        paths.clear();
        for (int i = 1; i < (int) args.size(); i ++) {
            if (args[i][int(args[i].size()) - 1] != '/') args[i] += '/';
            paths.push_back(args[i]);
        }
    }
}

void execCmd(std::vector<std::string> args, std::vector<std::string> &paths) {
    std::string cmd = args[0];
    bool isRedirect = args.size() > 2 && args[args.size() - 2] == ">";
    
    bool done = false;
    for (int i = 0; i < (int) paths.size(); i ++) {
        std::string execPath = paths[i] + cmd;
        if (accessExists(execPath)) {
            execUnderlyingCmd(execPath, args, isRedirect);
            done = true;
            break;
        }
    }
    if (!done) {
        errorMessage(isRedirect);
    }
}

void execCmds(std::vector<std::vector<std::string>> parallels, std::vector<std::string> &paths) {
    if ((int) parallels.size() == 1 && (parallels[0][0] == "exit" || parallels[0][0] == "cd" || parallels[0][0] == "path")) {
        execBuiltIn(parallels[0], paths);
        return;
    }
    std::vector<pid_t> children;
    for (int i = 0; i < (int) parallels.size(); i ++) {
        int pid = fork();
        if (pid != 0) {  // this is parent
            children.push_back(pid);
            continue;
        }
        // this is child
        execCmd(parallels[i], paths);
        exit(EXIT_SUCCESS);
    }

    for (int i = 0; i < (int) children.size(); i ++) {
        waitpid(children[i], NULL, 0);
    }

    return;
}

std::vector<std::string> splitByDelimiters(std::string s, int &redirectionIdx) {
    int left = 0;
    std::vector<std::string> result = {};

    for (int right = 0; right < (int) s.size(); right ++) {
        if (isspace(s[right]) || s[right] == '>') {
            if (left < right) {
                result.push_back(s.substr(left, right - left));
            }
            if (s[right] == '>' && redirectionIdx == -1) {
                result.push_back(">");
                redirectionIdx = (int) result.size();
            }
            left = right + 1;
        } 
    }
    if (left != (int) s.size()) {
        result.push_back(s.substr(left, int (s.size()) - left));
    }

    return result;
}

std::vector<std::vector<std::string>> parseInput(std::string s) {
    int left = 0;
    std::vector<std::vector<std::string>> result;
    std::vector<std::string> currCmd;
    bool seenRedirect = false;

    for (int right = 0; right < (int) s.size(); right ++) {
        if (isspace(s[right]) || s[right] == '>' || s[right] == '&') {
            if (left < right) {
                currCmd.push_back(s.substr(left, right - left));
            }
            if (s[right] == '>') {
                if (seenRedirect) {
                    throw("too many redirects");
                }
                currCmd.push_back(">");
                seenRedirect = true;
            }
            if (s[right] == '&' && currCmd.size()) {
                if (seenRedirect && (currCmd[currCmd.size() - 2] != ">" || currCmd[0] == ">")) {
                    throw("bad redirect syntax");
                }
                std::vector<std::string> completedCmd = currCmd;
                result.push_back(completedCmd);
                currCmd.clear();
                seenRedirect = false;
            }
            left = right + 1;
        } 
    }
    if (left != (int) s.size()) {
        currCmd.push_back(s.substr(left, int (s.size()) - left));
    }

    if (currCmd.size() && (currCmd[(int) currCmd.size() - 1] == ">" || (seenRedirect && (currCmd[currCmd.size() - 2] != ">" || currCmd[0] == ">")))) throw("bad redirect syntax");

    if ((int) currCmd.size() != 0) {
        result.push_back(currCmd);
    }
    
    return result;
}

void parseOrThrow(std::vector<std::vector<std::string>> &parallels, std::string userIn) {
    try {
        parallels = parseInput(userIn);
    } catch(char const* err) {
        errorMessage(false);
    }
}

int main(int argc, char* argv[]) {
    std::vector<std::string> paths = {"/bin/"};

    if (argc > 2) {
        errorMessage(false);
        return EXIT_FAILURE;
    }

    bool inBatchMode = false;

    // batch mode
    if (argc == 2) {
        inBatchMode = true;
        int fd = open(argv[1], O_RDONLY);
        if (fd == -1) {
            errorMessage(false);
            return (EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
    }

    std::string userIn = "";
    if (!inBatchMode) std::cout << "wish>";
    while (std::getline(std::cin, userIn)) {

        std::vector<std::vector<std::string>> parallels;
        parseOrThrow(parallels, userIn);
        if ((int) parallels.size() == 0) {
            if (!inBatchMode) std::cout << "wish>";
            continue;
        }
        parseOrThrow(parallels, userIn);

        execCmds(parallels, paths);
        if (!inBatchMode) std::cout << "wish>";
    }

    return EXIT_SUCCESS;
}
