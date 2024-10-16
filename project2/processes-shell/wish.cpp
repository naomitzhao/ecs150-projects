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

#include <typeinfo>

bool accessExists(std::string path) {
    if (access(path.c_str(), X_OK) == 0) return true;
    return false;
}

bool fileExists(std::string path) {
    if (access(path.c_str(), R_OK) == 0) return true;
    return false;
}

void execCmd(std::string path, std::vector<std::string> args, bool isRedirect) {
    // std::cout << "exec " << path << std::endl;
    int ret = fork();
    int diff = 0;
    if (isRedirect) diff = -1;
    if (ret < 0) {
        std::cerr << "error" << std::endl;
        return;
    } else if (ret == 0) {  // this is the child
        char* execArgs[int(args.size()) + 1 + diff];
        // std::cout << "args: ";
        for (int i = 0; i < int(args.size()) + diff; i ++) {
            execArgs[i] = (char*) args[i].c_str();
            // std::cout << args[i] << " ";
        }
        // std::cout << std::endl;
        execArgs[args.size() + diff] = NULL;
        execvp(path.c_str(), execArgs);
        return;
    } else {  // this is the parent
        waitpid(ret, NULL, 0);
    }
    
    return;
}

void execExit() {
    exit(0);
}

void execCd(std::string dir) {
    chdir(dir.c_str());
    // std::cout << "now in " << dir << std::endl;
}

void printVector(std::vector<std::string> vec) {
    for (int i = 0; i < (int) vec.size(); i ++) {
        std::cout << vec[i] << " ";
    }
    std::cout << std::endl;
}

std::vector<std::string> splitByDelimiters(std::string s, int &redirectionIdx) {
    int left = 0;
    std::vector<std::string> result = {};

    for (int right = 0; right < (int) s.size(); right ++) {
        if (s[right] == ' ' || s[right] == '>') {
            if (left < right) {
                result.push_back(s.substr(left, right - left));
            }
            if (s[right] == '>' && redirectionIdx == -1) {
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

void errorMessage(bool redirect) {
    char error_message[30] = "An error has occurred\n";
    if (redirect) {
        write(STDOUT_FILENO, error_message, strlen(error_message));
    } else {
        write(STDERR_FILENO, error_message, strlen(error_message)); 
    }
}

int main(int argc, char* argv[]) {
    std::vector<std::string> paths = {"/bin/"};

    if (argc > 2) {
        std::cout << "" << std::endl;
        return EXIT_FAILURE;
    }

    bool inBatchMode = false;

    // batch mode
    if (argc == 2) {
        inBatchMode = true;
        int fd = open(argv[1], O_RDONLY);
        if (fd == -1) {
            std::cerr << "error opening batch mode file" << std::endl;
            return (EXIT_FAILURE);
        }
        dup2(fd, STDIN_FILENO);
    }

    // std::unordered_set<std::string> built_ins{"exit", "cd", "path"};

    std::string userIn = "";
    if (!inBatchMode) std::cout << "wish>";
    while (std::getline(std::cin, userIn)) {

        int redirectionIdx = -1;
        bool isRedirect = false;
        std::vector<std::string> args = splitByDelimiters(userIn, redirectionIdx);
        // std::cout << "args: ";
        // printVector(args);
        int stdout_fd = dup(STDOUT_FILENO);
        
        if (redirectionIdx != -1) {
            if (redirectionIdx != int(args.size()) - 1 || redirectionIdx == 0) {
                errorMessage(isRedirect);
                continue;
            } else {
                isRedirect = true;
                std::string redirectPath = args[int(args.size()) - 1];
                int fd;
                // std::cout << "redirecting to " << redirectPath << std::endl;

                if (fileExists(redirectPath)) {
                    fd = open(redirectPath.c_str(), O_WRONLY);
                } else {
                    fd = open(redirectPath.c_str(), O_CREAT | O_WRONLY);
                }
                
                dup2(fd, STDOUT_FILENO);
            }
        }
        std::string cmd = args[0];

        // for (int i = 0; i < args.size(); i ++) {
        //     std::cout << args[i] << " ";
        // }
        // std::cout << std::endl;
        
        if (cmd == "exit") {
            if ((int) args.size() != 1) {
                errorMessage(isRedirect);
            }
            execExit();
        } else if (cmd == "cd") {
            // std::cout << args.size() << std::endl;
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
            // std::cout << "paths: ";
            // for (int i = 0; i < (int) paths.size(); i ++) {
            //     std::cout << paths[i] << " ";
            // }
            // std::cout << std::endl;
        }
        else {
            bool done = false;
            for (int i = 0; i < (int) paths.size(); i ++) {
                std::string execPath = paths[i] + cmd;
                if (accessExists(execPath)) {
                    execCmd(execPath, args, isRedirect);
                    done = true;
                    break;
                }
            }
            if (!done) {
                errorMessage(isRedirect);
            }
        }
        if (isRedirect) {
            dup2(stdout_fd, STDOUT_FILENO);
        }
        if (!inBatchMode) std::cout << "wish>";
    }

    return EXIT_SUCCESS;
}
