/* r2_window - v0.0 - public domain vector, windowing library
    no warranty implied; use at your own risk

    Built in the style of: https://github.com/nothings/stb

    A lot the linux code was referenced from the examples in the
    tinygl library: https://bellard.org/TinyGL/

    This is written with game development in mind.

    Do this:
       #define R2_WINDOWS_IMPLEMENTATION
    before you include this file in *one* C or C++ file
    to create the implementation.

    // i.e. it should look like this:
    #include ...
    #include ...
    #include ...
    #define R2_WINDOW_IMPLEMENTATION
    #include "r2_windows.h"

    You can then include without the define to just use the types

LICENSE
    See end of file for license information.
*/

#ifndef R2_WINDOW
#define R2_WINDOW

#ifdef __cplusplus
extern "C"
{
#endif

/* headers */
#define R2_WINDOW_ERROR 0
#define R2_WINDOW_NO_ERROR 1
#include <stdio.h>

    struct Window

    int open_window(int width, int height, const char *name);

/* == Unix (X11) =============================== */
#if defined(unix) || defined(__unix) || defined(__unix__)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdlib.h>
#endif
/* == Unix (X11) =============================== */

/* == MacOS ==================================== */
#if defined(__APPLE__) || defined(__MACH__)
#include <objc/runtime.h>
#include <objc/message.h>
#include <Carbon/Carbon.h>
#include <stdio.h>
#endif
/* == MacOS ==================================== */

/* == Windows ================================== */
#if defined(_WIN32) || defined(__CYGWIN__)
#endif
/* == Windows ================================== */




/* headers */

/* implementation */
#ifdef R2_WINDOW_IMPLEMENTATION

/* ============================================= */
/* == Unix (X11) =============================== */
/* ============================================= */
#if defined(unix) || defined(__unix) || defined(__unix__)
#define RGBA 4
#define DOUBLEBUFFER 5

    static int attributeList[] = {RGBA, DOUBLEBUFFER, None};

    Display *dpy;
    Window win;

    XVisualInfo *choose_visual(Display *dspy, int screen, int *attribList)
    {
        XVisualInfo vinfo;
        int n;

        /* only RGBA rendering - all of the if's here are ok */
        if (XMatchVisualInfo(dspy, screen, 16, TrueColor, &vinfo))
        {
            /* 16 bit visual */
        }
        else if (XMatchVisualInfo(dspy, screen, 24, TrueColor, &vinfo))
        {
            /* 24 bit visual */
        }
        else if (XMatchVisualInfo(dspy, screen, 32, TrueColor, &vinfo))
        {
            /* 32 bit visual */
        }
        else if (XMatchVisualInfo(dspy, screen, 8, PseudoColor, &vinfo))
        {
            /* 8 bit visual */
        }
        else
        {
            /* no suitable visual */
            return NULL;
        }

        return XGetVisualInfo(dspy, VisualAllMask, &vinfo, &n);
    }

    // wait for an event that is for us
    static Bool wait_for_notify(Display *d, XEvent *e, char *arg)
    {
        return (e->type == MapNotify) && (e->xmap.window == (Window)arg);
    }

    int open_window(int width, int height, const char *name)
    {
        XVisualInfo *vi;

        dpy = XOpenDisplay(NULL);
        if (dpy == NULL)
        {
            fprintf(stderr, "Could not open X display\n");
            return R2_WINDOW_ERROR;
        }
        vi = choose_visual(dpy, DefaultScreen(dpy), attributeList);
        if (vi == NULL)
        {
            fprintf(stderr, "No suitable visual\n");
            return R2_WINDOW_ERROR;
        }

        Colormap cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);

        XSetWindowAttributes swa;
        XSizeHints hint;

        hint.x = 0;
        hint.y = 0;
        hint.width = width;
        hint.height = height;
        hint.flags = PPosition | PSize;

        swa.colormap = cmap;
        swa.border_pixel = 0;
        swa.event_mask = StructureNotifyMask;

        win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, width, height, 0, vi->depth, InputOutput,
                            vi->visual, CWBorderPixel | CWColormap | CWEventMask, &swa);

        XSetStandardProperties(dpy, win, name, name, None, NULL, 0, &hint);
        XMapWindow(dpy, win);

        // Handle Events
        XEvent event;
        XEvent xev;
        KeySym keysym;
        XComposeStatus status;
        char buf[80]; // key press as a string

        XIfEvent(dpy, &event, wait_for_notify, (char *)win);
        XSelectInput(dpy, win, KeyPressMask | StructureNotifyMask | ExposureMask);

        // while (1)
        // {
        //     if (XPending(dpy) > 0)
        //     {
        //         XNextEvent(dpy, &xev);
        //         switch (xev.type)
        //         {
        //         case KeyPress: {
        //             XLookupString((XKeyEvent *)&xev, buf, 80, &keysym, &status);
        //             // switch(keysym) {
        //             printf("%ld %s\n", keysym, buf);
        //             // }
        //         }
        //         }
        //     }
        // }

        return R2_WINDOW_NO_ERROR;
    }
#endif
/* ============================================= */
/* == Unix (X11) =============================== */
/* ============================================= */

/* ============================================= */
/* == MacOS ==================================== */
/* ============================================= */
#if defined(__APPLE__) || defined(__MACH__)
#define cls objc_getClass
#define sel sel_getUid
#define msg ((id (*)(id, SEL))objc_msgSend)
#define msg_int ((id (*)(id, SEL, int))objc_msgSend)
#define msg_id  ((id (*)(id, SEL, id))objc_msgSend)
#define msg_ptr ((id (*)(id, SEL, void*))objc_msgSend)
#define msg_cls ((id (*)(Class, SEL))objc_msgSend)
#define msg_cls_chr ((id (*)(Class, SEL, char*))objc_msgSend)

typedef enum NSApplicationActivationPolicy {
    NSApplicationActivationPolicyRegular   = 0,
    NSApplicationActivationPolicyAccessory = 1,
    NSApplicationActivationPolicyERROR     = 2,
} NSApplicationActivationPolicy;

typedef enum NSWindowStyleMask {
    NSWindowStyleMaskBorderless     = 0,
    NSWindowStyleMaskTitled         = 1 << 0,
    NSWindowStyleMaskClosable       = 1 << 1,
    NSWindowStyleMaskMiniaturizable = 1 << 2,
    NSWindowStyleMaskResizable      = 1 << 3,
} NSWindowStyleMask;

typedef enum NSBackingStoreType {
    NSBackingStoreBuffered = 2,
} NSBackingStoreType;

int open_window(int width, int height, const char *name)
{
    // Create an autorelease pool
    id pool = msg_cls(cls("NSAutoreleasePool"), sel("alloc"));
    pool = msg(pool, sel("init"));

    // Get shared application instance
    id app = msg_cls(cls("NSApplication"), sel("sharedApplication"));

    // Set application activation policy
    msg_int(app, sel("setActivationPolicy:"), NSApplicationActivationPolicyRegular);

    struct CGRect frameRect = {0, 0, width, height};

    // Allocate and initialize NSWindow
    id window = ((id (*)(id, SEL, struct CGRect, int, int, int))objc_msgSend)(
        msg_cls(cls("NSWindow"), sel("alloc")),
        sel("initWithContentRect:styleMask:backing:defer:"),
        frameRect,
        NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable,
        NSBackingStoreBuffered,
        false
    );

    // Set window title
    msg_id(window, sel("setTitle:"), msg_cls_chr(cls("NSString"), sel("stringWithUTF8String:"), name));

    // Make window key and order front
    msg_ptr(window, sel("makeKeyAndOrderFront:"), nil);

    // Activate app ignoring other apps
    msg_int(app, sel("activateIgnoringOtherApps:"), true);

    // Get NSString for default run loop mode
    id defaultMode = msg_cls_chr(cls("NSString"), sel("stringWithUTF8String:"), "kCFRunLoopDefaultMode");

    // Start the event loop
    while (1) {
        // Create an autorelease pool for the event loop
        id eventPool = msg_cls(cls("NSAutoreleasePool"), sel("alloc"));
        eventPool = msg(eventPool, sel("init"));

        // Get the next event
        id event = ((id (*)(id, SEL, unsigned long long, id, id, bool))objc_msgSend)(app,
            sel("nextEventMatchingMask:untilDate:inMode:dequeue:"),
            UINT64_MAX, nil, defaultMode, true);

        // Check if an event was received
        if (event) {
            // Print event description
            id description = msg(event, sel("description"));
            const char *eventDesc = ((const char* (*)(id, SEL))objc_msgSend)(description, sel("UTF8String"));
            printf("Event: %s\n", eventDesc);

            // Dispatch the event
            msg_id(app, sel("sendEvent:"), event);
            msg(app, sel("updateWindows"));
        }

        // Drain the autorelease pool for the event loop
        msg(eventPool, sel("drain"));
    }

    // Release allocated objects (to prevent memory leak)
    msg(window, sel("release"));
    msg(app, sel("release"));

    // Drain the main autorelease pool
    msg(pool, sel("drain"));

    return 0;
}

#endif
/* ============================================= */
/* == MacOS ==================================== */
/* ============================================= */

/* ============================================= */
/* == Windows ================================== */
/* ============================================= */
/* Both 32 bit and 64 bit */
#if defined(_WIN32) || defined(__CYGWIN__)
    // https://croakingkero.com/tutorials/opening_a_window_with_win32/
    #define UNICODE
    #define _UNICODE
    #include <windows.h>
    #include <stdbool.h>

    bool quit = false;

    LRESULT CALLBACK WindowProcessMessage(HWND, UINT, WPARAM, LPARAM);

    int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow) {
        WNDCLASS window_class = { 0 };
        const wchar_t window_class_name[] = L"My Window Class";
        window_class.lpszClassName = window_class_name;
        window_class.lpfnWndProc = WindowProcessMessage;
        window_class.hInstance = hInstance;
        
        RegisterClass(&window_class);
        
        HWND window_handle = CreateWindow(
            window_class_name, 
            L"Learn to Program Windows",
            WS_OVERLAPPEDWINDOW, 
            CW_USEDEFAULT, 
            CW_USEDEFAULT, 
            CW_USEDEFAULT, 
            CW_USEDEFAULT, NULL, NULL, hInstance, NULL);

        if(window_handle == NULL) { return -1; }
        
        ShowWindow(window_handle, nCmdShow);
        
        while(!quit) {
            MSG message;
            while(PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
            
            // Do game stuff here
        }
        return 0;
    }

    LRESULT CALLBACK WindowProcessMessage(HWND window_handle, UINT message, WPARAM wParam, LPARAM lParam) {
        switch(message) {
            case WM_QUIT:
            case WM_DESTROY: {
                quit = true;
            } break;
            
            default: { // Message not handled; pass on to default message handling function
                return DefWindowProc(window_handle, message, wParam, lParam);
            } break;
        }
        return 0;
    }
#endif
    /* ============================================= */
    /* == Windows ================================== */
    /* ============================================= */

#endif
    /* implementation */

#ifdef __cplusplus
}
#endif

#endif /* R2_WINDOW */

/*
   revision history:
    0.0   (2024-10-05) Initial bits
*/

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2024 Rob Rohan
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
