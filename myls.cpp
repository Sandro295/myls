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
#include <grp.h>
#include <chrono>

struct Flags {
    bool lflag{0};
};

static const char *user_name(uid_t uid) {
	struct passwd *passwd = getpwuid(uid);
    return passwd ? passwd->pw_name : nullptr;
}

static const char *group_name(gid_t gid) {
	struct group *group = getgrgid(gid);
    return group ? group->gr_name : nullptr;
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

auto secToDate(auto wts) {
      /* Consider a time to be recent if it is within the past six
         months.  A Gregorian year has 365.2425 * 24 * 60 * 60 ==
         31556952 seconds on the average.  Write this value as an
         integer constant to avoid floating point hassles.  */
    using namespace std::chrono;
    const auto current_time = round<minutes>(file_clock::now());
    const auto six_months_ago = current_time - 31556952s / 2;
    return wts > six_months_ago ? std::format("{:%b %e %H:%M}", wts) : std::format("{:%b %e %Y}", wts);
}

void printColorfulFilename(auto filename, auto filestats) {
    /* eval $(echo "no:global default;fi:normal file;di:directory;ln:symbolic link;pi:named pipe;so:socket;do:door;bd:block device;cd:character device;or:orphan symlink;mi:missing file;su:set uid;sg:set gid;tw:sticky other writable;ow:other writable;st:sticky;ex:executable;"|sed -e 's/:/="/g; s/\;/"\n/g')           
{      
  IFS=:     
  for i in $LS_COLORS     
  do        
    echo -e "\e[${i#*=}m$( x=${i%=*}; [ "${!x}" ] && echo "${!x}" || echo "$x" )\e[m" 
  done       
} */
    using namespace std::filesystem;
    using std::filesystem::perms;
    auto p = filestats.permissions();
    auto filetype = filestats.type();
    constexpr auto endc = "\033[0m";
    constexpr auto cyanc = "\x1B[36m";
    constexpr auto redc = "\033[31m";
    constexpr auto bluec = "\033[34m";
    constexpr auto greenc = "\033[32m";
    if (is_symlink(filestats)) {
        std::cout << cyanc << filename << endc;
        return;
    }
    if (is_directory(filestats)) {
        std::cout << bluec << filename << endc;
        return;
    }
    if ((perms::others_exec & p) != perms::none) {
        std::cout << greenc << filename << endc;
        return;
    }
//        case file_type::fifo:
//            return 'p';
//        case file_type::block:
//            return 'b';
//        case file_type::socket:
//            return 's';
//        case file_type::symlink:
//            return 'l';
//        case file_type::character:
//            return 'c';
//        case file_type::not_found:
//            throw std::runtime_error{"lost a file, hmm"};
    std::cout << filename;

}

void walkDirectory(const std::string& dirPath, Flags flags) {
    if (!flags.lflag) {
        uint32_t fileCount{0};
        for(auto _ : std::filesystem::directory_iterator(dirPath)) {
            ++fileCount;
        }
        const auto sep = fileCount > 25 ? "\n" : "  ";
        for(const auto fit : std::filesystem::directory_iterator(dirPath)) {
            auto fname = fit.path().filename().string();
            if (fname.find(' ') != std::string::npos) {
                printColorfulFilename(fit.path().filename(), fit.status());
                std::cout << sep;
            } else {
                printColorfulFilename(fit.path().filename().string(), fit.status());
                std::cout << sep;
            }
        }
        std::cout << "\n" << std::endl;
        return;
    }
    
    auto print = [](auto value, auto width, auto sep = " ") {
        std::cout << std::setw(width) << value << sep;
    };
    for (auto fit : std::filesystem::directory_iterator(dirPath)) {
        struct stat sb;
        if (auto cname = fit.path().c_str(); stat(cname, &sb) == -1) {
            std::cout << strerror(errno) << '\n';
            std::cout << fit.path().filename().string() << "\n";
            continue;
        };
        auto filename = fit.path().filename().string();
        if (filename.starts_with(".")) {
            continue;
        }
        auto statcpp = fit.symlink_status();
        std::cout << getFileType(statcpp.type());
        printPermissions(statcpp.permissions());
        print(sb.st_nlink, 5, " ");
        if (auto name = user_name(sb.st_uid); name) {
            print(name, 5, " ");
        }
        if (auto name = group_name(sb.st_gid); name) {
            print(name, 5, " ");
        }
        print(sb.st_size, 7, " ");
        auto wtime = std::filesystem::last_write_time(fit);
        print(secToDate(wtime), 12, " ");
        printColorfulFilename(filename, statcpp);
        std::cout << " ";
        if (fit.is_symlink()) {
            auto linkname = std::filesystem::read_symlink(fit).string();
            auto linkstats = std::filesystem::status(fit);
            std::cout << "-> ";
            printColorfulFilename(linkname, linkstats);
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
            case '?':
                std::cout << "dafuq u said\n";
                return 0;
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

    
    printf("\n");
    printf("\x1B[31mTexting1\033[0m\t\t");
    printf("\x1B[32mTexting2\033[0m\t\t");
    printf("\x1B[33mTexting3\033[0m\t\t");
    printf("\x1b[34mtexting4\033[0m\t\t");
    printf("\x1B[35mTexting5\033[0m\n");
    
    printf("\x1B[36mTexting6\033[0m\t\t");
    printf("\x1B[36mTexting7\033[0m\t\t");
    printf("\x1B[36mTexting8\033[0m\t\t");
    printf("\x1B[37mTexting9\033[0m\t\t");
    printf("\x1B[93mTexting10\033[0m\n");
    
    printf("\033[3;42;30mTexting11\033[0m\t\t");
    printf("\033[3;43;30mTexting12\033[0m\t\t");
    printf("\033[3;44;30mTexting13\033[0m\t\t");
    printf("\033[3;104;30mTexting14\033[0m\t\t");
    printf("\033[3;100;30mTexting15\033[0m\n");

    printf("\033[3;47;35mTexting16\033[0m\t\t");
    printf("\033[2;47;35mTexting17\033[0m\t\t");
    printf("\033[1;47;35mTexting18\033[0m\t\t");
    printf("\t\t");
    printf("\n");

    return 0;
}

