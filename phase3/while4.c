#include <stdio.h>
#include <unistd.h>

int main(void)
{
    while(1)
    {
        sleep(1);
        printf("5");       
        fflush(stdout);

    }
    return (0);
}