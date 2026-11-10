#ifndef _X11_XCURSOR_XCURSOR_H_
#define _X11_XCURSOR_XCURSOR_H_

/*
 * Minimal Xcursor stub for environments where libXcursor development headers
 * are not installed. GLFW loads the real Xcursor functions dynamically via
 * dlopen at runtime (see x11_init.c / PFN_Xcursor*), so only the type
 * declarations needed to compile the type signatures are required here.
 * libXcursor.so.1 is present on the system at runtime.
 */

#include <X11/Xlib.h>

typedef unsigned int XcursorPixel;

typedef struct _XcursorImage {
    unsigned short version;
    unsigned short size;
    unsigned short width;
    unsigned short height;
    unsigned short xhot;
    unsigned short yhot;
    XcursorPixel *pixels;
    unsigned int delay;
} XcursorImage;

#endif /* _X11_XCURSOR_XCURSOR_H_ */
