#include <iostream>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i ++) {
        int fileDescriptor = open(argv[i], O_RDONLY);
        if (fileDescriptor == -1) {
            std::cout << "wcat: cannot open file" << std::endl;
            return EXIT_FAILURE;
        }

        int ret;
        char buffer[4096];
        while ((ret = read(fileDescriptor, buffer, sizeof(buffer))) > 0) {
            if (ret == -1) {
                std::cout << "error reading from " << argv[i] << std::endl;
                return EXIT_FAILURE;
            }
            int writeRes = write(STDOUT_FILENO, buffer, ret);
            if (writeRes == -1) {
                std::cout << "error writing to stdout" << std::endl;
                return EXIT_FAILURE;
            }
        }
        int closeRes = close(fileDescriptor);
        if (closeRes == -1) {
            std::cout << "error closing " << argv[i] << std::endl;
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

// g++ wcat.cpp -o wcat -Wall -Werror