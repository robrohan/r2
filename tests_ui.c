#include <stdio.h>

#define R2_WINDOW_IMPLEMENTATION
#include "r2_window.h"

int main(void)
{
    printf("hi!\n");
    open_window(300, 300, "testing");

    while(1) 
    {
        
    }

    printf("closed\n");
}
