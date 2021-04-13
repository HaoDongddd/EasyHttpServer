#include "heap.h"
#include <iostream>
int main()
{
    Heap<int> test;
    test.add(4);
    test.add(1);
    test.add(3);
    test.add(5);
    test.add(7);
    test.add(2);
    test.add(6);
    int l = test.length();
    for(int i = 0;i<l;++i)
    {
        cout<<test.get_front();
        test.pop();
    }
}