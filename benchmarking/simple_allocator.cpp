#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
using namespace std;

typedef long long ll;

int main()
{
    const ll n = 4.5e6;  // MB
    const ll sz = 1024;  // bytes
    const ll report = 5e5;
    for (ll i = 0; i < n; i++) {
        char * tmp = (char*)malloc(sz);
        for (int j = 0; j < sz; j++) {
            tmp[j] = 'a';
        }
        if (i % report == 0) {
            printf("Total bytes = %lld\n", (i+1)*sz);
            sleep(1);
        }
    }
    return 0;
}
