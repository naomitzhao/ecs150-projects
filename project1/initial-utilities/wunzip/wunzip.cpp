#include <iostream>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc == 1) {
        std::cout << "wunzip: file1 [file2 ...]" << std::endl;
        return EXIT_FAILURE;
    }

    int ret;
    char buffer[4095];
    char writeBuf[4096];

    for (int i = 1; i < argc; i ++) {
        int fileDescriptor = open(argv[i], O_RDONLY);
        if (fileDescriptor == -1) {
            std::cout << "wunzip: cannot open file" << std::endl;
            return EXIT_FAILURE;
        }

        while ((ret = read(fileDescriptor, buffer, sizeof(buffer))) > 0) {
            if (ret == -1) {
                std::cout << "error reading from argv[i]" << std::endl;
                return EXIT_FAILURE;
            }
            for (int j = 0; j < ret; j += 5) {
                int count = buffer[j] + buffer[j + 1] * 256 + buffer[j + 2] * 65536 + buffer[j + 3] * 16777216;

                int bufferFill = count;
                if (bufferFill > int(sizeof(writeBuf))) {
                    bufferFill = sizeof(writeBuf);
                }
                for (int k = 0; k < bufferFill; k++) {
                    writeBuf[k] = buffer[j + 4];
                }
                int k = 0;
                while (k + int(sizeof(writeBuf)) < count) {
                    int writeRes = write(STDOUT_FILENO, writeBuf, sizeof(writeBuf));
                    if (writeRes == -1) {
                        std::cout << "error writing to stdout" << std::endl;
                        return EXIT_FAILURE;
                    }
                    k += sizeof(writeBuf);
                }
                int writeRes = write(STDOUT_FILENO, writeBuf, count - k);
                if (writeRes == -1) {
                    std::cout << "error writing to stdout" << std::endl;
                    return EXIT_FAILURE;
                }
            }
        }
        int closeRes = close(fileDescriptor);
        if (closeRes == -1) {
            std::cout << "error closing " << argv[i] << std::endl;
        }
    }

    return EXIT_SUCCESS;
}