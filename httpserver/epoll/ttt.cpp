#include <iostream>
#include "heap.h"
#include "work.h"
#include "sys/time.h"
#include <assert.h>
using namespace std;

struct b
{
    int* a;
    b(const b& t)
    {
        cout << "copy cons called\n";
        a = t.a;
        *a = *t.a;
    }
    b(b&& t)
    {
        cout << " move called\n"; 
        
        a = t.a;
        *a = *t.a;
        t.a = nullptr;
    }
    b()
    {
        a = new int;
        *a = 2;
    }
    ~b()
    {
        
        if(a != nullptr)
        {
            cout << "des called and a:" << *a << endl;
            delete a;
        }
        cout << "des\n";
    }
    b& get()
    {
        return *this;
    }
    void operator()()
    {
        cout << "() fun\n";
    }
};
struct a
{
    vector<b> v;
    b& get()
    {
        return v[0];
    }
};
int main()
{
    // b a;
    // cout << "a's a is " << *a.a << "这个a的address is " << a.a << endl;
    // b c(move(a));
    // cout << "c's a is " << *c.a << "这个a的address is " << c.a << endl;
    // b d(move(c.get()));
    // cout << "d's a is " << *d.a << "这个a的address is " << d.a << endl;
    // b tmp;
    // cout << "address: " << tmp.a << endl;
    // a g;
    // g.v.push_back(tmp);
    // cout << "address: " << g.get().a << endl;
    // b a;
    // b c = move(a);
    // b()();
    // class swap
    // {
    //     public :
    //         void operator()(int &a,int &b)
    //         {
    //             int tmp = a;
    //             a = b;
    //             b = tmp;

    //         }
    // };
    // Heap<int,Swap> heap;
    // heap.add(3);
    // heap.add(1);
    // heap.add(6);
    // heap.add(5);
    // int l = heap.length();
    // for(int i = 0;i < l;++i)
    // {
    //     cout << heap.get_front() << endl;
    //     heap.pop();
    // }

    unordered_map<int,string> m;
    m.insert({1,"123"});
    m.insert({2,"34re"});
    m.insert({4,"qwe12"});
    for(auto it = m.begin();it != m.end();++it)
    {
        cout << it->second << endl;
    }
}