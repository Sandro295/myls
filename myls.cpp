#include <cstdlib>
#include <cstring>
#include <exception>
#include <format>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <chrono>

struct Flags {
    bool lflag{0};
};

static const char * user_name(uid_t uid) {
	struct passwd *passwd = getpwuid(uid);
    return passwd ? passwd->pw_name : nullptr;
}

void printPermissions(const std::filesystem::perms p) {
    using std::filesystem::perms;
    auto show = [=](char op, perms perm) {
        std::cout << (perms::none == (perm & p) ? '-' : op);
    };
    show('r', perms::owner_read);
    show('w', perms::owner_write);
    show('x', perms::owner_exec);
    show('r', perms::group_read);
    show('w', perms::group_write);
    show('x', perms::group_exec);
    show('r', perms::others_read);
    show('w', perms::others_write);
    show('x', perms::others_exec);
    std::cout << ' ';
}

char getFileType(const std::filesystem::file_type filetype) {
    using namespace std::filesystem;
    switch (filetype) {
        case file_type::directory:
            return 'd';
        case file_type::fifo:
            return 'p';
        case file_type::block:
            return 'b';
        case file_type::socket:
            return 's';
        case file_type::symlink:
            return 'l';
        case file_type::character:
            return 'c';
        case file_type::not_found:
            throw std::runtime_error{"lost a file, hmm"};
        default:
            return '-';
    }
}

auto secToDate() {
    return 0;
}

void walkDirectory(const std::string& dirPath, Flags flags) {
    if (!flags.lflag) {
        uint32_t fileCount{0};
        for(auto _ : std::filesystem::directory_iterator(dirPath)) {
            ++fileCount;
        }
        const auto sep = fileCount > 10 ? "\n" : "  ";
        for(const auto fit : std::filesystem::directory_iterator(dirPath)) {
            std::cout << fit.path().filename().string() << sep;
        }
        std::cout << std::endl;
        return;
    }
    
      /* Consider a time to be recent if it is within the past six
         months.  A Gregorian year has 365.2425 * 24 * 60 * 60 ==
         31556952 seconds on the average.  Write this value as an
         integer constant to avoid floating point hassles.  */
      //six_months_ago.tv_sec = current_time.tv_sec - 31556952 / 2;
      //six_months_ago.tv_nsec = current_time.tv_nsec;
    auto print = [&](auto value, auto width, auto sep) {
        std::cout << std::setw(width) << value << sep;
    };
    for (auto fit : std::filesystem::directory_iterator(dirPath)) {
        struct stat sb;
        if (auto cname = fit.path().c_str(); stat(cname, &sb) == -1) {
              std::cout << strerror(errno) << '\n';
              std::cout << fit.path().filename().string() << "\n";
              continue;
        };
        auto statcpp = fit.status();
        std::cout << getFileType(statcpp.type());
        printPermissions(statcpp.permissions());
        print(sb.st_nlink, 5, " ");
        if (auto name = user_name(sb.st_uid); name) {
            std::cout << name << " ";
        }
        if (auto name = user_name(sb.st_gid); name) {
            std::cout << name << " ";
        }
        print(sb.st_size, 7, " ");
        auto wtime = std::filesystem::last_write_time(fit);
        std::cout << std::format("{:%H:%M} ", wtime);
        std::cout << fit.path().filename().string() << " ";
        if (fit.is_symlink()) {
            std::cout << "-> " << std::filesystem::read_symlink(fit).string();
        }
        std::cout << "\n";
    }
    std::cout << std::endl;
}

int main(int argc, char** argv) {
    Flags flags;
    int c;
    opterr = 0;
    while ((c = getopt(argc, argv, "l")) != -1) {
        switch (c) {
            case 'l':
                flags.lflag = true;
                break;
        }
    }

    try {
        if (auto i = optind; i == argc) {
            walkDirectory(".", flags);
        } else {
            for (; i < argc; ++i) {
                std::cout << argv[i] << ":\n";
                walkDirectory(argv[i], flags);
            }
        }
    } catch (std::exception& ex) {
        std::cout << ex.what() << std::endl;
    }
    return 0;
}

