
#include "common.h"
#include "simroot.h"
#include "configuration.h"

#include "cpu/isa.h"

#include "launch/launchv2.hpp"
#include "launch/test_pgtable.hpp"

#define TEST(x) printf("Execute: %s\n", #x); if(!x) printf("Test %s failed\n", #x);

#define OPERATION(op, name, statement) {if(op.compare(name)==0) { { statement } return;}};

#define ASSERT_ARGS(vec, num, info) {if(vec.size() != num){ printf("Args %s:  \"%s\"\n", #vec,  info); return;}};

#define ASSERT_MORE_ARGS(vec, num, info) {if(vec.size() < num){ printf("Args %s:  \"%s\"\n", #vec,  info); return;}};

#define PRINT_ARGS(vec) { std::cout << #vec": "; for(auto &e : vec) std::cout << e << " , "; std::cout << std::endl;};

void execution();

using std::string;
using std::pair;
using std::vector;
using std::make_pair;


INITIALIZE_EASYLOGGINGPP

struct {
    string operation;
    std::vector<string> configs;
    std::vector<string> strings;
    std::vector<string> workload;
    std::vector<int> integers;
    std::vector<float> floats;
} parsed_args;

int main(int argc, char* argv[]) {
    //------------------------- Init ------------------------
    srand(get_current_time_us());

    auto print_help_and_exit = [=]()->void {
        printf("Usage: %s operation [[-c configs] [-s str_args] [-i int_args] [-f float_args] ...] [-w workload argvs]\n", argv[0]);
        exit(0);
    };

    if(argc < 2) {
        print_help_and_exit();
    }
    parsed_args.operation = argv[1];

    for(int i = 2; i < argc; i+=2) {
        char *cur = argv[i];
        if(cur[0] != '-') {
            print_help_and_exit();
        }
        if(i + 1 >= argc) {
            print_help_and_exit();
        }
        if(cur[1] == 'w') {
            for(int j = i+1; j < argc; j++) {
                parsed_args.workload.push_back(argv[j]);
            }
            break;
        }
        switch (cur[1])
        {
        case 'c':
            parsed_args.configs.push_back(argv[i+1]);
            break;
        case 's':
            parsed_args.strings.push_back(argv[i+1]);
            break;
        case 'i':
            parsed_args.integers.push_back(atoi(argv[i+1]));
            break;
        case 'f':
            parsed_args.floats.push_back(atof(argv[i+1]));
            break;
        default:
            print_help_and_exit();
        }
    }

    if(parsed_args.configs.empty()) {
        std::cout << "No config file specified, use default." << std::endl;
        conf::load_ini_file("conf/default.ini");
    }
    else {
        for(auto &s : parsed_args.configs) {
            conf::load_ini_file(s);
        }
    }

    el::Configurations defaultConf;
    defaultConf.setToDefault();
    defaultConf.set(el::Level::Info, el::ConfigurationType::Format, "%datetime %level %msg");
    el::Loggers::reconfigureLogger("default", defaultConf);

    execution();

    return 0;
}

void execution() {
    string &op = parsed_args.operation;
    vector<int> &I = parsed_args.integers;
    vector<float> &F = parsed_args.floats;
    vector<string> &S = parsed_args.strings;
    vector<string> &W = parsed_args.workload;
    PRINT_ARGS(I);
    PRINT_ARGS(F);
    PRINT_ARGS(S);
    PRINT_ARGS(W);


    OPERATION(op, "mpv2", {
        ASSERT_MORE_ARGS(W, 1, "elf_path")
        TEST(mpv2(W));
    });


    OPERATION(op, "test_pgtable", {
        TEST(test_pgtable());
    });


    OPERATION(op, "test_simroot", {
        TEST(test::test_simroot());
    });

    OPERATION(op, "test_decoder_rv64", {
        TEST(test::test_decoder_rv64());
    });
    
    OPERATION(op, "test_ini_file", {
        TEST(test::test_ini_file());
    });

}





