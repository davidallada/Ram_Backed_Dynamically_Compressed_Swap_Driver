#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
using namespace std;

string
run_command(string cmd)
{
    FILE *fp;
    char buffer[1024];
    string result = "";
    fp = popen(cmd.c_str(), "r");
    if (fp == NULL) {
        printf("failed to run free");
        exit(1);
    }
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        result += buffer;
    }
    pclose(fp);
    return result;
}

void
print_total_usage_info()
{
    string res = run_command("free");
    istringstream out (res);
    string tmp_s;
    int tmp_i, mem_total, mem_used, swap_total, swap_used;
    for (int i = 0; i < 7; i++) out >> tmp_s;
    out >> mem_total;
    out >> mem_used;
    for (int i = 0; i < 4; i++) out >> tmp_i;
    out >> tmp_s;
    out >> swap_total;
    out >> swap_used;

    double mem_percent = ((double)mem_used) / mem_total * 100;
    double swap_percent = ((double)swap_used) / swap_total * 100;
    cout << setprecision(2) \
        << "Mem: "  << mem_percent  << "%  " \
        << "Swap: " << swap_percent << "%\n";
}

int
main()
{
    while (1) {
        print_total_usage_info();
        this_thread::sleep_for(chrono::milliseconds(400));
    }
    return 0;
}
