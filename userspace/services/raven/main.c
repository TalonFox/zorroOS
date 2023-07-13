#include <System/Thread.h>
#include <System/Syscall.h>
#include <System/SharedMemory.h>
#include <Filesystem/Filesystem.h>
#include <Filesystem/MQueue.h>
#include <Common/Alloc.h>
#include <Common/String.h>
#include <Media/QOI.h>
#include <Media/Image.h>
#include <Media/StackBlur.h>
#include <Common/Spinlock.h>
#include "kbd.h"
#include "mouse.h"
#define _RAVEN_IMPL
#include "raven.h"

#define MIN(__x, __y) ((__x) < (__y) ? (__x) : (__y))
#define MAX(__x, __y) ((__x) > (__y) ? (__x) : (__y))

FBInfo fbInfo;
MQueue* msgQueue = NULL;

Window* winHead = NULL;
Window* winTail = NULL;
Window* winFocus = NULL;
Window* iconHead = NULL;
Window* iconTail = NULL;
char windowLock = 0;
uint64_t nextWinID = 1;
const uint32_t cursorBuf[10*16] = {
    0xffffffff,0xffffffff,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
    0xffffffff,0xff000000,0xffffffff,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
    0xffffffff,0xff000000,0xff000000,0xffffffff,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
    0xffffffff,0xff000000,0xff000000,0xff000000,0xffffffff,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
    0xffffffff,0xff000000,0xff000000,0xff000000,0xff000000,0xffffffff,0x00000000,0x00000000,0x00000000,0x00000000,
    0xffffffff,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xffffffff,0x00000000,0x00000000,0x00000000,
    0xffffffff,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xffffffff,0x00000000,0x00000000,
    0xffffffff,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xffffffff,0x00000000,
    0xffffffff,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xffffffff,
    0xffffffff,0xff000000,0xff000000,0xff000000,0xff000000,0xff000000,0xffffffff,0xffffffff,0xffffffff,0xffffffff,
    0xffffffff,0xff000000,0xff000000,0xffffffff,0xff000000,0xff000000,0xffffffff,0x00000000,0x00000000,0x00000000,
    0xffffffff,0xff000000,0xffffffff,0x00000000,0xffffffff,0xff000000,0xff000000,0xffffffff,0x00000000,0x00000000,
    0xffffffff,0xffffffff,0x00000000,0x00000000,0xffffffff,0xff000000,0xff000000,0xffffffff,0x00000000,0x00000000,
    0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0xffffffff,0xff000000,0xff000000,0xffffffff,0x00000000,
    0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0xffffffff,0xff000000,0xff000000,0xffffffff,0x00000000,
    0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0xffffffff,0xffffffff,0xffffffff,0x00000000,
};
Window backgroundWin = (Window){
    .prev = NULL,
    .next = NULL,
    .id = 0,
    .x = 0,
    .y = 0,
    .w = 0,
    .h = 0,
    .flags = FLAG_NOMOVE | FLAG_OPAQUE,
    .shmID = 0,
    .backBuf = NULL,
    .frontBuf = NULL,
    .owner = 0,
};
Window cursorWin = (Window){
    .prev = NULL,
    .next = NULL,
    .id = 0,
    .x = 0,
    .y = 0,
    .w = 10,
    .h = 16,
    .flags = FLAG_NOMOVE,
    .shmID = 0,
    .backBuf = NULL,
    .frontBuf = (uint32_t*)&cursorBuf,
    .owner = 0,
};

uint32_t BlendPixel(uint32_t px1, uint32_t px2) {
    uint32_t m1 = (px2 & 0xFF000000) >> 24;
    uint32_t m2 = 255 - m1;
    uint32_t r2 = m2 * (px1 & 0x00FF00FF);
    uint32_t g2 = m2 * (px1 & 0x0000FF00);
    uint32_t r1 = m1 * (px2 & 0x00FF00FF);
    uint32_t g1 = m1 * (px2 & 0x0000FF00);
    uint32_t result = (0x0000FF00 & ((g1 + g2) >> 8)) | (0x00FF00FF & ((r1 + r2) >> 8));
    return result;
}

void Redraw(int x, int y, int w, int h) {
    SpinlockAcquire(&windowLock);
    Window* win = &backgroundWin;
    int max_x, max_y;
    max_x = x+w;
    max_y = y+h;
    int bytes = fbInfo.bpp/8;
    while(win != NULL) {
        if(!(max_x <= win->x || max_y <= win->y || x >= (win->x+win->w) || y >= (win->y+win->h))) {
          int fX1, fX2, fY1, fY2;
          fX1 = MAX(x,win->x);
          fX2 = MIN(max_x,win->x+(win->w-1));
          fY1 = MAX(y,win->y);
          fY2 = MIN(max_y,win->y+(win->h-1));
          if((win->flags & FLAG_ACRYLIC)) {
            StackBlur(fbInfo.back,fbInfo.pitch/(fbInfo.bpp/8),4,MAX(1,fX1),MIN(fbInfo.width-1,fX2),MAX(1,fY1),MIN(fbInfo.height-1,fY2));
          }
          for(int i=fY1; i <= fY2; i++) {
            if(i < 0) {
                continue;
            } else if(i >= fbInfo.height) {
                break;
            }
            for(int j=fX1; j <= fX2; j++) {
                if(j < 0) {
                    continue;
                } else if(j >= fbInfo.width) {
                    break;
                }
                uint32_t pixel = win->frontBuf[((i - win->y)*win->w)+(j-win->x)];
                if ((pixel & 0xFF000000) == 0xFF000000 || (win->flags & FLAG_OPAQUE) != 0) {
                    fbInfo.back[(i*(fbInfo.pitch/bytes))+j] = pixel;
                } else if (((pixel & 0xFF000000) != 0x00000000 && (win->flags & FLAG_OPAQUE) == 0) || (win->flags & FLAG_ACRYLIC)) {
                    uint32_t result = BlendPixel(fbInfo.back[(i*(fbInfo.pitch/bytes))+j],pixel);
                    fbInfo.back[(i*(fbInfo.pitch/bytes))+j] = result;
                }
            }
          }
        }
        if(win == &backgroundWin) {
            if(iconHead != NULL) {
                win = iconHead;
            } else {
                if(winHead == NULL) {
                    win = &cursorWin;
                } else {
                win = winHead;
                }
            }
        } else if(win == iconTail) {
            if(winHead == NULL) {
                win = &cursorWin;
            } else {
                win = winHead;
            }
        } else if(win == winTail) {
            win = &cursorWin;
        } else {
            win = win->next;
        }
    }
    for(int i=y; i < y+h; i++) {
        if(i < 0) {
            continue;
        } else if(i >= fbInfo.height) {
            break;
        }
        memcpy(&fbInfo.addr[(i*(fbInfo.pitch/bytes))+x],&fbInfo.back[(i*(fbInfo.pitch/bytes))+x],w*4);
    }
    SpinlockRelease(&windowLock);
}

void MoveWinToFront(Window* win) {
    SpinlockAcquire(&windowLock);
    if(winTail == win) {
        SpinlockRelease(&windowLock);
        return;
    }
    if(winHead == win) {
        winHead = win->next;
    }
    if(win->prev != NULL) {
        ((Window*)win->prev)->next = win->next;
    }
    if(win->next != NULL) {
        ((Window*)win->next)->prev = win->prev;
    }
    winTail->next = win;
    win->prev = winTail;
    win->next = NULL;
    winTail = win;
    if(winHead == NULL) {
        winHead = win;
    }
    SpinlockRelease(&windowLock);
}

Window* GetWindowByID(int64_t id) {
    Window* index = winTail;
    while(index != NULL) {
        if(index->id == id) {
            return index;
        }
        index = index->prev;
    }
    return NULL;
}

void invertPixel(int x, int y) {
    if(x >= 0 && x < fbInfo.width && y >= 0 && y < fbInfo.height) {
        fbInfo.addr[(y*(fbInfo.pitch/(fbInfo.bpp/8)))+x] = ~fbInfo.addr[(y*(fbInfo.pitch/(fbInfo.bpp/8)))+x];
    }
}

void renderInvertOutline(int x, int y, int w, int h) {
    for(int i=x+5; i < x+(w-5); i++) {
        invertPixel(i,y);
        invertPixel(i,y+(h-1));
    }
    for(int i=y+5; i < y+(h-5); i++) {
        invertPixel(x,i);
        invertPixel(x+(w-1),i);
    }
    // Top Left
    invertPixel(x+3,y+1);
    invertPixel(x+4,y+1);
    invertPixel(x+2,y+2);
    invertPixel(x+1,y+3);
    invertPixel(x+1,y+4);
    // Top Right
    invertPixel((x+w)-5,y+1);
    invertPixel((x+w)-4,y+1);
    invertPixel((x+w)-3,y+2);
    invertPixel((x+w)-2,y+3);
    invertPixel((x+w)-2,y+4);
    // Bottom Left
    invertPixel(x+3,(y+h)-2);
    invertPixel(x+4,(y+h)-2);
    invertPixel(x+2,(y+h)-3);
    invertPixel(x+1,(y+h)-4);
    invertPixel(x+1,(y+h)-5);
    // Bottom Right
    invertPixel((x+w)-5,(y+h)-2);
    invertPixel((x+w)-4,(y+h)-2);
    invertPixel((x+w)-3,(y+h)-3);
    invertPixel((x+w)-2,(y+h)-4);
    invertPixel((x+w)-2,(y+h)-5);
}

void DoBoxAnimation(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2, char expand) {
    for(int i=0; i < 32; i++) {
        int x = (x1 * (32 - i) + x2 * i) >> 5;
        int y = (y1 * (32 - i) + y2 * i) >> 5;
        int w = (w1 * (32 - i) + w2 * i) >> 5;
        int h = (h1 * (32 - i) + h2 * i) >> 5;
        renderInvertOutline(x,y,w,h);
        if(expand)
            Eep(50000/(i+1));
        else
            Eep(50000/(32-i));
        renderInvertOutline(x,y,w,h);
    }
}

void* iconPack;
PSFHeader* unifont;

Window* AddIcon(int x, int y, const char* icon, const char* emblem, uint32_t emblemColor, const char* name) {
    Window* ic = malloc(sizeof(Window));
    ic->x = x;
    ic->y = y;
    ic->w = 8*strlen(name);
    ic->h = 32+16;
    ic->backBuf = malloc(ic->w*ic->h*4);
    ic->frontBuf = malloc(ic->w*ic->h*4);
    ic->prev = iconTail;
    if(iconTail != NULL) {
        iconTail->next = ic;
    }
    ic->next = NULL;
    iconTail = ic;
    if(iconHead == NULL)
        iconHead = ic;
    GraphicsContext* gfx = Graphics_NewContext(ic->frontBuf,ic->w,ic->h);
    Graphics_RenderIcon(gfx,iconPack,icon,(ic->w/2)-16,0,32,32,0xffbcd7e8);
    if(emblem != NULL) {
        Graphics_RenderIcon(gfx,iconPack,emblem,(ic->w/2)-16,32-12,16,12,emblemColor);
    }
    Graphics_RenderString(gfx,0,32,0xffffffff,unifont,1,name);
    gfx->buf = ic->backBuf;
    Graphics_RenderIcon(gfx,iconPack,icon,(ic->w/2)-16,0,32,32,0xff000000 | BlendPixel(0xbcd7e8,0x80000000));
    if(emblem != NULL) {
        Graphics_RenderIcon(gfx,iconPack,emblem,(ic->w/2)-16,32-12,16,12,0xff000000 | BlendPixel(emblemColor & 0xffffff,0x80000000));
    }
    Graphics_DrawRect(gfx,0,32,ic->w,16,0x80000000);
    Graphics_RenderString(gfx,0,32,0xffffffff,unifont,1,name);
    free(gfx);
    return icon;
}

void LoadBackground(const char* name) {
    qoi_desc desc;
    void* bgImage = qoi_read(name,&desc,4);
    if(bgImage == NULL) {
        RyuLog("RAVEN WARN: Failed to load image ");
        RyuLog(name);
        RyuLog("!\n");
        return;
    }
    Image_ABGRToARGB((uint32_t*)bgImage,desc.width*desc.height);
    Image_ScaleNearest((uint32_t*)bgImage,backgroundWin.frontBuf,desc.width,desc.height,backgroundWin.w,backgroundWin.h);
    free(bgImage);
    Redraw(0,0,fbInfo.width,fbInfo.height);
}

int main(int argc, const char* argv[]) {
    msgQueue = MQueue_Bind("/dev/mqueue/Raven");
    iconPack = Graphics_LoadIconPack("/System/Icons/IconPack");
    unifont = Graphics_LoadFont("/System/Fonts/unifont.psf");
    AddIcon(32,32,"Device/HardDrive","Emblem/zorroOS",0xff3a8cd1,"Root");
    //AddIcon(72,32,"User/Trash",NULL,0,"Trash");
    OpenedFile fbFile;
    if(Open("/dev/fb0",O_RDWR,&fbFile) < 0) {
        RyuLog("Failed to open /dev/fb0!\n");
        return 1;
    }
    fbFile.IOCtl(&fbFile,0x100,&fbInfo);
    backgroundWin.w = fbInfo.width;
    backgroundWin.h = fbInfo.height;
    backgroundWin.frontBuf = (uint32_t*)MMap(NULL,fbInfo.width*fbInfo.height*4,3,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    fbInfo.back = (uint32_t*)MMap(NULL,fbInfo.pitch*fbInfo.height,3,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    fbInfo.addr = MMap(NULL,fbInfo.pitch*fbInfo.height,3,MAP_SHARED,fbFile.fd,0);
    fbFile.Close(&fbFile);
    void* kbdStack = MMap(NULL,0x8000,3,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    uintptr_t kbdThr = NewThread("Raven Keyboard Thread",&KeyboardThread,(void*)(((uintptr_t)kbdStack)+0x8000));
    void* mouseStack = MMap(NULL,0x8000,3,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    uintptr_t mouseThr = NewThread("Raven Mouse Thread",&MouseThread,(void*)(((uintptr_t)mouseStack)+0x8000));
    LoadBackground("/System/Wallpapers/Mountain.qoi");
    while(1) {
        int64_t teamID;
        RavenPacket* packet = MQueue_RecieveFromClient(msgQueue,&teamID,NULL);
        switch(packet->type) {
            case RAVEN_OKEE_BYEEEE: {
                SpinlockAcquire(&windowLock);
                Window* win = winTail;
                while(win != NULL) {
                    if(win->owner == teamID) {
                        if(win == winFocus) {
                            winFocus = NULL;
                        }
                        void* prev = win->prev;
                        void* creator = win->creator;
                        int x = win->x;
                        int y = win->y;
                        int w = win->w;
                        int h = win->h;
                        if(win->prev != NULL) {
                            ((Window*)win->prev)->next = win->next;
                        }
                        if(win->next != NULL) {
                            ((Window*)win->next)->prev = win->prev;
                        }
                        if(win == winTail) {
                            winTail = win->prev;
                        }
                        if(win == winHead) {
                            winHead = win->next;
                        }
                        free(win->frontBuf);
                        MUnMap(win->backBuf,(win->w*win->h)*4);
                        DestroySharedMemory(win->shmID);
                        free(win);
                        SpinlockRelease(&windowLock);
                        Redraw(x,y,w,h);
                        if(creator != NULL) {
                            Window* cwin = (Window*)creator;
                            DoBoxAnimation(x,y,w,h,cwin->x,cwin->y,cwin->w,cwin->h,0);
                        }
                        SpinlockAcquire(&windowLock);
                        win = prev;
                    } else {
                        win = win->prev;
                    }
                }
                SpinlockRelease(&windowLock);
                break;
            }
            case RAVEN_CREATE_WINDOW: {
                SpinlockAcquire(&windowLock);
                Window* win = malloc(sizeof(Window));
                win->id = nextWinID++;
                win->owner = teamID;
                if(winTail != NULL) {
                    winTail->next = win;
                }
                win->prev = winTail;
                win->next = NULL;
                win->w = packet->create.w;
                win->h = packet->create.h;
                win->flags = packet->create.flags;
                win->x = (fbInfo.width/2)-(packet->create.w/2);
                win->y = (fbInfo.height/2)-(packet->create.h/2);
                win->shmID = NewSharedMemory((win->w*win->h)*4);
                win->backBuf = MapSharedMemory(win->shmID);
                win->frontBuf = malloc((win->w*win->h)*4);
                win->creator = iconHead;
                winTail = win;
                if(winHead == NULL) {
                    winHead = win;
                }
                RavenCreateWindowResponse response;
                response.id = win->id;
                response.backBuf = win->shmID;
                MQueue_SendToClient(msgQueue,teamID,&response,sizeof(RavenCreateWindowResponse));
                SpinlockRelease(&windowLock);
                Redraw(win->x,win->y,win->w,win->h);
                break;
            }
            case RAVEN_MOVE_WINDOW: {
                int oldX, oldY;
                SpinlockAcquire(&windowLock);
                Window* win = GetWindowByID(packet->move.id);
                oldX = win->x;
                oldY = win->y;
                win->x = packet->move.x;
                win->y = packet->move.y;
                SpinlockRelease(&windowLock);
                Redraw(oldX,oldY,win->w,win->h);
                Redraw(win->x,win->y,win->w,win->h);
                break;
            }
            case RAVEN_GET_RESOLUTION: {
                RavenGetResolutionResponse response;
                response.w = fbInfo.width;
                response.h = fbInfo.height;
                MQueue_SendToClient(msgQueue,teamID,&response,sizeof(RavenGetResolutionResponse));
                break;
            }
            case RAVEN_FLIP_BUFFER: {
                SpinlockAcquire(&windowLock);
                Window* win = GetWindowByID(packet->flipBuffer.id);
                if(win != NULL) {
                    for(int i=packet->flipBuffer.y; i < packet->flipBuffer.y+packet->flipBuffer.h; i++) {
                        memcpy(&win->frontBuf[(i*win->w)+packet->flipBuffer.x],&win->backBuf[(i*win->w)+packet->flipBuffer.x],packet->flipBuffer.w*4);
                    }
                }
                SpinlockRelease(&windowLock);
                Redraw(win->x+packet->flipBuffer.x,win->y+packet->flipBuffer.y,packet->flipBuffer.w,packet->flipBuffer.h);
                break;
            }
            default: {
                break;
            }
        }
        free(packet);
    }
    return 0;
}