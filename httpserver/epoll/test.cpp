#include "http.h"
#include "time_heap.h"
#include "work.h"
#include <signal.h>
#include <assert.h>
#include "../log_system/log.h"

Log log;
int main()
{
    create_work_threads();
    while(1);
}