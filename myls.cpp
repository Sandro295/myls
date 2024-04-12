#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <unistd.h>


void walkDirectory(const std::string& dirPath, bool lflag = false) {
    if (!lflag) {
        for(auto fit : std::filesystem::directory_iterator(dirPath)) {
            std::cout << fit.path().filename().string() << "  ";
        }
        std::cout << std::endl;
    }
}

int main(int argc, char** argv) {
    bool lflag = 0;
    int c;
    opterr = 0;
    while ((c = getopt(argc, argv, "l")) != -1) {
        switch (c) {
            case 'l':
                lflag = true;
                break;
        }
    }

    std::cout << "lflag = " << lflag << "\n";
    if (auto i = optind; i == argc) {
        walkDirectory(".", lflag);
    } else {
        for (i = argc - optind; i > 0; --i) {
            std::cout << argv[i] << ":\n";
            walkDirectory(argv[i], lflag);
            std::cout << "\n";
        }
        std::cout << std::endl;
    }
    return 0;
}
