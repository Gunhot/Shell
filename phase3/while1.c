#include <stdio.h>
#include <unistd.h>

int main(void)
{
    while(1)
    {
        sleep(1);
        printf("2");  
        fflush(stdout);
    }
    return (0);
}