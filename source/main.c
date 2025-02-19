#define FONTSTASH_IMPLEMENTATION
#define SI_ALLOCATOR_UNDEFINE


#include <assert.h>
#include <stb_image.h>
#include <RGFW.h>
#include <draw.h>
#include <xss.h>
#include <sili.h>

#include <unistd.h>

/*
TODO:

- screenshot active window
- screenshot selected rectangle
- show mouse
- select font already installed on the system

CLI

POST:

make sure I only link the parts of sili I actually need

debloat text renderer

version control using rlgl to support older opengl versions

save last use options
color options
*/

bool rectCollidePoint(rect r, point p) {
    return (p.x >= r.x &&  p.x <= r.x + r.w && p.y >= r.y && p.y <= r.y + r.h);
}

typedef enum {
    none = 0,
    hovered,
    pressed,
} buttonStatus;

typedef struct button {
    char* text;
    rect r;
    buttonStatus s;
    bool toggle; /* for toggle buttons */
} button;

void updateButton(button* b, RGFW_Event e) {
    switch (e.type) {
        case RGFW_mouseButtonPressed:
            if (rectCollidePoint(b->r, (point){e.x, e.y})) {
                if (b->s != pressed)
                    b->toggle = !b->toggle;
                b->s = pressed;
            }
            break;
        case RGFW_mouseButtonReleased:
            if (b->s == pressed && rectCollidePoint(b->r, (point){e.x, e.y}))
                b->s = hovered;
            else
                b->s = none;
            break;
        case RGFW_mousePosChanged:
            if (rectCollidePoint(b->r, (point){e.x, e.y}))
                b->s = hovered;
            else
                b->s = none;
            break;
        default: break;
    }
}

int main(int argc, char** argv) {
    int i;

    unsigned char delay = 0; /* 0 - 60 delay time for screenshot */

    const int buttonCount = 10;
    button buttons[] = { 
        /* region toggle buttons [only one can be on at at time] */
        {"Entire screen", 40, 20, 20, 20, 0, true},
        {"Active window", 40, 50, 20, 20},
        {"Select a region", 40, 80, 20, 20},
        /* option buttons */
        {"Capture cursor", 40, 140, 20, 20},
        {"Capture window border", 40, 170, 20, 20},
        /* delay buttons */
        {"0", 180, 20, 20, 20},
        {"_", 200, 20, 20, 20}, /* _ looks better than - */
        {"+", 220, 20, 20, 20},
        /* other */
        {"Cancel", 200, 178, 40, 20},
        {"OK", 250, 178, 40, 20}
    };
    
    RGFW_window* win = RGFW_createWindowPointer("screenshot-tool", 0, 0, 300, 200, RGFW_NO_RESIZE);

    if (si_path_exists("logo.png")) {
        int w, h, c;
        unsigned char* icon = stbi_load("logo.png", &w, &h, &c, 0);
        RGFW_window_setIcon(win, icon, w, h, c);
        free(icon);
    }
    else
        printf("Warning : logo.png not found\n");

    /* init rlgl for graphics */
    rlLoadExtensions((void*)RGFW_getProcAddress);
    rlglInit(win->w, win->h);

    color bg = {70, 70, 70, 255}; /* the window's background color */   
    rlClearColor(bg.r, bg.g, bg.b, bg.a);

    /* center window */
    int* screenSize = RGFW_window_screenSize(win);
    
    win->x = (screenSize[0] + win->w) / 4;
    win->y = (screenSize[1] + win->h) / 8;

    /* init text renderer */
    FONScontext* ctx = glfonsCreate(500, 500, 1);

    int font = fonsAddFont(ctx, "sans", "DejaVuSans.ttf");
    
    XWindowAttributes attrs;
    XGetWindowAttributes(win->display, DefaultRootWindow(win->display), &attrs);
    
    XGetWindowAttributes(win->display, win->window, &attrs);

    unsigned int boarder_height = attrs.y;

    rect screenshot = {0, 0, attrs.width, attrs.height};

    bool running = true;

    while (running) {
        /*
            check/handle events until there are none left to handle
            
            this is to prevent any event lag     
        */

        while(RGFW_window_checkEvent(win) != NULL) {
            for (i = 0; i < buttonCount; i++) {
                updateButton(&buttons[i], win->event);
                
                if (buttons[i].s != pressed)
                    continue;

                if (si_between(i, 6, 7)) {
                    delay += (i == 6) ? -1 : 1;

                    delay = (delay == 61) ? 0 : (delay == 255) ? 60 : delay;
                }

                else if (i == 8) {
                    running = false;
                
                    break;
                }

                else if (i == 9)
                    goto SCREENSHOT;
                
                else if (si_between(i, 0, 2))
                    for (i = 0; i < 3; i++)
                        buttons[i].toggle = (buttons[i].s == pressed);
            }

            switch (win->event.type) {
                case RGFW_quit:
                    running = false;
                    break;

                default: break;
            }
        }

        /* draw headers */
        drawText("Region", (circle){30, 0, 20}, (color){200, 200, 200, 255}, win, ctx, font);
        drawText("Options", (circle){30, 110, 20}, (color){200, 200, 200, 255}, win, ctx, font);
        drawText("Delay", (circle){180, 0, 20}, (color){200, 200, 200, 255}, win, ctx, font);

        buttons[5].text = si_u64_to_cstr(delay);

        /* draw buttons */
        for (i = 0; i < buttonCount; i++) {
            color col = (color){0, 0, 0, 255};

            if (buttons[i].s == hovered && !buttons[i].toggle && i > 5) 
                col = (color){50, 50, 50, 255};
            else if (buttons[i].s == pressed || (buttons[i].toggle && i < 5))
                col = (color){185, 42, 162, 255};

            if (i < 5)
                drawPolygon(buttons[i].r, 360, col, win);
            else
                drawRect(buttons[i].r, col, win);

            circle c = {buttons[i].r.x + (20 * (i < 5)) + 2, buttons[i].r.y + 5, 10};

            if (i == 6) 
                c = (circle){c.x + 4, c.y - 4, c.d}; /* adjust the _ */

            drawText(buttons[i].text, c, (color){200, 200, 200, 255}, win, ctx, font);
        }

        rlClearScreenBuffers();
        rlDrawRenderBatchActive();

        RGFW_window_swapBuffers(win);
        
        rlClearColor(bg.r, bg.g, bg.b, bg.a);
    }


    if (false) { SCREENSHOT:
        XUnmapWindow(win->display, win->window);
        XFlush(win->display);   

        /* wait for window to close and for panel to hide */
        XEvent e;
        while (e.type != UnmapNotify)
            XNextEvent(win->display, &e);

        if (!delay)
            usleep(700000);
        else
            sleep(delay);

        if (buttons[1].toggle) {
            int r;

            XGetInputFocus(win->display, &win->window, &r);

            assert(win->window != NULL);

            XWindowAttributes a;
            XGetWindowAttributes(win->display, win->window, &a);

            //boarder_height
            if (buttons[4].toggle)
                screenshot = (rect){a.x, a.y, a.width, a.height};
            else
                screenshot = (rect){a.x, a.y - boarder_height, a.width, a.height + boarder_height};
        }

        else if (buttons[2].toggle) {

        }

        XImage* img = XGetImage(win->display, DefaultRootWindow(win->display), 0, 0, screenshot.w, screenshot.h, AllPlanes, ZPixmap);
        siString filename = si_string_make("Screenshot_");

        time_t t = time(NULL);
        struct tm now = *localtime(&t);

        si_string_append(&filename, si_u64_to_cstr(now.tm_year + 1900));
        si_string_push(&filename, '_');
        si_string_append(&filename, si_u64_to_cstr(now.tm_mon + 1));
        si_string_push(&filename, '_');
        si_string_append(&filename, si_u64_to_cstr(now.tm_mday));
        si_string_push(&filename, '_');
        si_string_append(&filename, si_u64_to_cstr(now.tm_hour));
        si_string_push(&filename, '_');
        si_string_append(&filename, si_u64_to_cstr(now.tm_min));
        si_string_push(&filename, '_');
        si_string_append(&filename, si_u64_to_cstr(now.tm_sec));

        unsigned int index = 0;

        while (si_path_exists(filename)) {
            index++;

            int eindex = si_string_len(filename) - 1;

            si_string_push(&filename, '_');

            char* indexStr = si_u64_to_cstr(index);
            si_string_append(&filename, indexStr);

            if (si_path_exists(filename))
                si_string_erase(&filename, eindex, si_string_len(indexStr) + 1);
        }
        
        si_string_append(&filename, ".png");

        screenshot_to_stream(img, filename, PNG, screenshot.x, screenshot.y, screenshot.w, screenshot.h);

        XFree(img);
        win->window = NULL;
    }

    /* free any leftover data */
    fonsDeleteInternal(ctx);
    rlglClose();
    
    RGFW_window_close(win);
}