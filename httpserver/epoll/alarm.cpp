#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <queue>
#include <vector>
using namespace std;

void print(int)
{
    cout << "alarm!\n";
    alarm(2);
}

int main()
{
    // signal(SIGALRM,print);
    
    // alarm(2);
    // while(1);
    vector<int> que;
    for(int i = 0;i <50000;++i)
    {
        que.push_back(i);
        cout << &que <<endl;
    }
}