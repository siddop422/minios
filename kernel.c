#include <stdint.h>
#include <stddef.h>

/* ---------- freestanding libc bits ---------- */
void *memset(void *d, int c, size_t n){ uint8_t *p=d; while(n--) *p++=(uint8_t)c; return d; }
void *memcpy(void *d, const void *s, size_t n){ uint8_t*a=d; const uint8_t*b=s; while(n--) *a++=*b++; return d; }
void *memmove(void *d, const void *s, size_t n){
    uint8_t*a=d; const uint8_t*b=s;
    if(a<b){ while(n--) *a++=*b++; } else { while(n--){ a[n]=b[n]; } }
    return d;
}
static int streq(const char*a,const char*b){ while(*a&&*a==*b){a++;b++;} return *a==*b; }
static int slen(const char*s){ int n=0; while(*s++)n++; return n; }
static char *scpy(char*d,const char*s){ char*r=d; while((*d++=*s++)); return r; }

/* ---------- port I/O ---------- */
static inline void outb(uint16_t p, uint8_t v){ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t inb(uint16_t p){ uint8_t r; __asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p)); return r; }

/* ---------- multiboot2 ---------- */
typedef struct { uint32_t type; uint32_t size; } mb2_tag_t;
typedef struct {
    uint32_t type, size;
    uint64_t addr;
    uint32_t pitch, width, height;
    uint8_t  bpp, fb_type;
    uint16_t reserved;
    uint8_t  red_pos, red_mask, green_pos, green_mask, blue_pos, blue_mask;
} mb2_fb_t;

static mb2_tag_t *find_fb_tag(uint32_t mbi_addr){
    uint8_t *p = (uint8_t*)(uintptr_t)mbi_addr;
    uint32_t total = *(uint32_t*)p;
    uint8_t *end = p + total;
    p += 8;
    while(p < end){
        mb2_tag_t *t = (mb2_tag_t*)p;
        if(t->type == 0) break;
        if(t->type == 8) return t;
        p += (t->size + 7) & ~7u;
    }
    return NULL;
}

/* ---------- framebuffer ---------- */
static uint8_t  *g_fb;
static uint32_t g_pitch, g_width, g_height;
static uint8_t  g_bpp;

static inline uint32_t pack(uint8_t r, uint8_t g, uint8_t b){
    return ((uint32_t)r<<16) | ((uint32_t)g<<8) | b;
}

static void fb_putpixel(int x, int y, uint32_t c){
    if(x < 0 || y < 0 || (uint32_t)x >= g_width || (uint32_t)y >= g_height) return;
    uint8_t *p = g_fb + y*g_pitch + x*(g_bpp/8);
    if(g_bpp == 32){ *(uint32_t*)p = c; }
    else if(g_bpp == 24){ p[0]=c&0xFF; p[1]=(c>>8)&0xFF; p[2]=(c>>16)&0xFF; }
    else if(g_bpp == 16){
        uint8_t r=(c>>16)&0xFF, g=(c>>8)&0xFF, b=c&0xFF;
        *(uint16_t*)p = ((r>>3)<<11)|((g>>2)<<5)|(b>>3);
    }
}

static uint32_t fb_getpixel(int x, int y){
    if(x < 0 || y < 0 || (uint32_t)x >= g_width || (uint32_t)y >= g_height) return 0;
    uint8_t *p = g_fb + y*g_pitch + x*(g_bpp/8);
    if(g_bpp == 32) return *(uint32_t*)p;
    if(g_bpp == 24) return p[0] | (p[1]<<8) | (p[2]<<16);
    if(g_bpp == 16){
        uint16_t v = *(uint16_t*)p;
        return (((v>>11)&0x1F)<<3)<<16 | (((v>>5)&0x3F)<<2)<<8 | ((v&0x1F)<<3);
    }
    return 0;
}

static void fb_fill(int x, int y, int w, int h, uint32_t c){
    if(x < 0){ w += x; x = 0; }
    if(y < 0){ h += y; y = 0; }
    if((int)((uint32_t)x + w) > (int)g_width)  w = g_width  - x;
    if((int)((uint32_t)y + h) > (int)g_height) h = g_height - y;
    if(w <= 0 || h <= 0) return;
    if(g_bpp == 32){
        for(int j = 0; j < h; j++){
            uint32_t *row = (uint32_t*)(g_fb + (y+j)*g_pitch) + x;
            for(int i = 0; i < w; i++) row[i] = c;
        }
    } else {
        for(int j = 0; j < h; j++)
            for(int i = 0; i < w; i++)
                fb_putpixel(x+i, y+j, c);
    }
}

/* ---------- 8x8 font (public domain, dhepper/font8x8) ---------- */
static const uint8_t g_font[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, /* !   */
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, /* "   */
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, /* #   */
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, /* $   */
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, /* %   */
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, /* &   */
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, /* '   */
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, /* (   */
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, /* )   */
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, /* *   */
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, /* +   */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, /* ,   */
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, /* -   */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, /* .   */
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, /* /   */
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, /* 0   */
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, /* 1   */
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, /* 2   */
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, /* 3   */
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, /* 4   */
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, /* 5   */
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, /* 6   */
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, /* 7   */
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, /* 8   */
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, /* 9   */
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, /* :   */
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, /* ;   */
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, /* <   */
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, /* =   */
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, /* >   */
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, /* ?   */
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, /* @   */
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, /* A   */
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, /* B   */
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, /* C   */
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, /* D   */
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, /* E   */
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, /* F   */
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, /* G   */
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, /* H   */
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* I   */
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, /* J   */
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, /* K   */
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, /* L   */
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, /* M   */
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, /* N   */
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, /* O   */
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, /* P   */
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, /* Q   */
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, /* R   */
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, /* S   */
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* T   */
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, /* U   */
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, /* V   */
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, /* W   */
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, /* X   */
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, /* Y   */
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, /* Z   */
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, /* [   */
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, /* \   */
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, /* ]   */
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, /* ^   */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, /* _   */
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, /* `   */
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, /* a   */
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, /* b   */
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, /* c   */
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00}, /* d   */
    {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00}, /* e   */
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00}, /* f   */
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, /* g   */
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, /* h   */
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, /* i   */
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, /* j   */
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, /* k   */
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* l   */
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, /* m   */
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, /* n   */
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, /* o   */
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, /* p   */
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, /* q   */
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, /* r   */
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, /* s   */
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, /* t   */
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, /* u   */
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, /* v   */
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, /* w   */
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, /* x   */
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, /* y   */
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, /* z   */
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, /* {   */
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, /* |   */
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, /* }   */
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}  /* ~   */
};

static void fb_char(int x, int y, char ch, uint32_t fg, uint32_t bg, int scale){
    if(ch < 0x20 || ch > 0x7E) ch = '?';
    const uint8_t *g = g_font[(uint8_t)ch - 0x20];
    for(int row = 0; row < 8; row++){
        uint8_t bits = g[row];
        for(int col = 0; col < 8; col++){
            uint32_t c = (bits & (1u << col)) ? fg : bg;
            if(scale == 1) fb_putpixel(x + col, y + row, c);
            else            fb_fill(x + col*scale, y + row*scale, scale, scale, c);
        }
    }
}
static void fb_text(int x, int y, const char *s, uint32_t fg, uint32_t bg, int scale){
    while(*s){ fb_char(x, y, *s, fg, bg, scale); x += 8*scale; s++; }
}

/* ---------- colors ---------- */
#define C_DESK_TOP    0x1a1a2e
#define C_DESK_BOT    0x16213e
#define C_WIN_BG      0x1f2937
#define C_TITLE       0x374151
#define C_BORDER      0x4b5563
#define C_TEXT        0xe5e7eb
#define C_TEXT_DIM    0x9ca3af
#define C_ACCENT      0x60a5fa
#define C_INPUT_BG    0x111827
#define C_TASKBAR     0x0f172a
#define C_SHADOW      0x0a0a14
#define C_RED         0xFF5F57
#define C_YELLOW      0xFEBC2E
#define C_GREEN       0x28C840

/* ---------- layout ---------- */
#define TITLE_H 32
static int win_x, win_y, win_w, win_h;
static int con_x, con_y, con_w, con_h;
static int input_x, input_y, input_w, input_h;

static void compute_layout(void){
    win_w = g_width  - 200; if(win_w > 640) win_w = 640;
    win_h = g_height - 160; if(win_h > 460) win_h = 460;
    win_x = (g_width  - win_w) / 2;
    win_y = (g_height - 40 - win_h) / 2;
    con_x = win_x + 2;
    con_y = win_y + TITLE_H + 2;
    con_w = win_w - 4;
    con_h = win_h - TITLE_H - 4;
    input_x = con_x + 20;
    input_w = con_w - 40;
    input_h = 36;
    input_y = con_y + con_h - input_h - 10;
}

/* ---------- cursor bitmap 12x18 ---------- */
#define CUR_W 12
#define CUR_H 18
static const uint8_t cur_bmp[CUR_H][CUR_W] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,1,1,1,1,1,0,0},
    {1,2,1,2,2,1,0,0,0,0,0,0},
    {1,1,0,1,2,2,1,0,0,0,0,0},
    {0,0,0,1,2,2,1,0,0,0,0,0},
    {0,0,0,0,1,2,2,1,0,0,0,0},
    {0,0,0,0,1,2,2,1,0,0,0,0},
    {0,0,0,0,0,1,1,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
};

static int mouse_x, mouse_y;
static int cur_drawn_x = -1, cur_drawn_y = -1;
static uint32_t cur_save[CUR_H][CUR_W];

static void cursor_erase(void){
    if(cur_drawn_x < 0) return;
    for(int j = 0; j < CUR_H; j++)
        for(int i = 0; i < CUR_W; i++)
            fb_putpixel(cur_drawn_x+i, cur_drawn_y+j, cur_save[j][i]);
    cur_drawn_x = -1;
}
static void cursor_draw(void){
    for(int j = 0; j < CUR_H; j++){
        for(int i = 0; i < CUR_W; i++){
            cur_save[j][i] = fb_getpixel(mouse_x+i, mouse_y+j);
            int v = cur_bmp[j][i];
            if(v == 1) fb_putpixel(mouse_x+i, mouse_y+j, 0x000000);
            else if(v == 2) fb_putpixel(mouse_x+i, mouse_y+j, 0xFFFFFF);
        }
    }
    cur_drawn_x = mouse_x;
    cur_drawn_y = mouse_y;
}

/* ---------- state ---------- */
static char input_buf[64];
static int  input_len = 0;
static char status_msg[128] = "Welcome to MiniOS v2.0 GUI!";
static int  need_redraw_input  = 0;
static int  need_redraw_status = 0;
static int  mouse_moved        = 0;

/* ---------- keyboard ---------- */
static const char kbd_map[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    0,'*',0,' '
};
static const char kbd_map_shift[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',0,
    0,'*',0,' '
};
static int kbd_shift = 0, kbd_caps = 0;

static void run_command(void);
static void kbd_byte(uint8_t sc){
    if(sc == 0x2A || sc == 0x36){ kbd_shift = 1; return; }
    if(sc == 0xAA || sc == 0xB6){ kbd_shift = 0; return; }
    if(sc == 0x3A){ kbd_caps = !kbd_caps; return; }
    if(sc & 0x80) return;
    char c = kbd_shift ? kbd_map_shift[sc] : kbd_map[sc];
    if(c >= 'a' && c <= 'z' && kbd_caps) c -= 32;
    if(!c) return;

    if(c == '\b'){
        if(input_len > 0){ input_len--; input_buf[input_len] = 0; need_redraw_input = 1; }
    } else if(c == '\n'){
        run_command();
        need_redraw_input = 1;
    } else if(c >= 32 && c < 127){
        if(input_len < (int)sizeof(input_buf) - 1){
            input_buf[input_len++] = c;
            input_buf[input_len] = 0;
            need_redraw_input = 1;
        }
    }
}

/* ---------- mouse ---------- */
static void ps2_flush(void){ for(int i=0;i<10000 && (inb(0x64)&1); i++) inb(0x60); }
static void ps2_wait_write(void){ for(int i=0;i<100000 && (inb(0x64)&2); i++); }
static void ps2_wait_read(void){  for(int i=0;i<100000 && !(inb(0x64)&1); i++); }

static void mouse_init(void){
    outb(0x64, 0xA8);                            /* enable aux port */
    outb(0x64, 0xD4); ps2_wait_write(); outb(0x60, 0xF6); ps2_wait_read(); inb(0x60);
    outb(0x64, 0xD4); ps2_wait_write(); outb(0x60, 0xF4); ps2_wait_read(); inb(0x60);
    ps2_flush();
}

static uint8_t mbuf[3];
static int     midx = 0;

static void mouse_byte(uint8_t b){
    if(midx == 0 && !(b & 0x08)) return;         /* resync on first byte */
    mbuf[midx++] = b;
    if(midx < 3) return;
    midx = 0;

    int dx = mbuf[1];
    int dy = mbuf[2];
    if(mbuf[0] & 0x10) dx -= 256;                /* sign bit for X */
    if(mbuf[0] & 0x20) dy -= 256;                /* sign bit for Y */
    mouse_x += dx;
    mouse_y -= dy;                               /* PS/2 Y is inverted */

    if(mouse_x < 0) mouse_x = 0;
    if(mouse_y < 0) mouse_y = 0;
    if(mouse_x > (int)g_width  - CUR_W) mouse_x = g_width  - CUR_W;
    if(mouse_y > (int)g_height - CUR_H) mouse_y = g_height - CUR_H;
    mouse_moved = 1;
}

/* ---------- UI drawing ---------- */
static uint32_t lerp(uint32_t a, uint32_t b, int t, int tmax){
    int r1=(a>>16)&0xFF, g1=(a>>8)&0xFF, b1=a&0xFF;
    int r2=(b>>16)&0xFF, g2=(b>>8)&0xFF, b2=b&0xFF;
    int r = r1 + (r2-r1)*t/tmax;
    int g = g1 + (g2-g1)*t/tmax;
    int bv = b1 + (b2-b1)*t/tmax;
    return ((uint32_t)r<<16)|((uint32_t)g<<8)|bv;
}

static void draw_desktop(void){
    for(uint32_t y = 0; y < g_height; y++){
        uint32_t c = lerp(C_DESK_TOP, C_DESK_BOT, y, g_height);
        fb_fill(0, y, g_width, 1, c);
    }
}

static void draw_taskbar(void){
    int tb_y = g_height - 40;
    fb_fill(0, tb_y, g_width, 40, C_TASKBAR);
    fb_fill(0, tb_y, g_width, 1, C_BORDER);
    fb_text(16, tb_y + 14, "MiniOS", C_TEXT, C_TASKBAR, 1);
    /* resolution at right */
    char res[16]; res[0]=0;
    int n = g_width, m = g_height, p = 0;
    char tmp[8]; int t = 0;
    while(n){ tmp[t++] = '0' + n%10; n /= 10; }
    while(t) res[p++] = tmp[--t];
    res[p++] = 'x';
    t = 0; while(m){ tmp[t++] = '0' + m%10; m /= 10; }
    while(t) res[p++] = tmp[--t];
    res[p] = 0;
    fb_text(g_width - 16 - (int)slen(res)*8, tb_y + 14, res, C_TEXT_DIM, C_TASKBAR, 1);
}

static void draw_window(void){
    /* shadow */
    fb_fill(win_x + 6, win_y + win_h, win_w, 6, C_SHADOW);
    fb_fill(win_x + win_w, win_y + 6, 6, win_h, C_SHADOW);
    /* body */
    fb_fill(win_x, win_y, win_w, win_h, C_WIN_BG);
    /* border */
    fb_fill(win_x, win_y, win_w, 1, C_BORDER);
    fb_fill(win_x, win_y + win_h - 1, win_w, 1, C_BORDER);
    fb_fill(win_x, win_y, 1, win_h, C_BORDER);
    fb_fill(win_x + win_w - 1, win_y, 1, win_h, C_BORDER);
    /* title bar */
    fb_fill(win_x + 1, win_y + 1, win_w - 2, TITLE_H, C_TITLE);
    /* traffic lights */
    int tx = win_x + win_w - 70;
    int ty = win_y + 1 + (TITLE_H - 8) / 2;
    fb_fill(tx,      ty, 8, 8, C_RED);
    fb_fill(tx + 14, ty, 8, 8, C_YELLOW);
    fb_fill(tx + 28, ty, 8, 8, C_GREEN);
    /* title */
    fb_text(win_x + 14, win_y + 1 + (TITLE_H - 8) / 2, "MiniOS Terminal", C_TEXT, C_TITLE, 1);
}

static void draw_content_static(void){
    int cx = con_x + 20;
    int cy = con_y + 20;

    fb_text(cx, cy, "Welcome to MiniOS", C_TEXT, C_WIN_BG, 2);
    cy += 40;
    fb_text(cx, cy, "A tiny 32-bit OS with a real GUI", C_TEXT_DIM, C_WIN_BG, 1);
    cy += 22;

    fb_fill(con_x + 10, cy, con_w - 20, 1, C_BORDER);
    cy += 16;

    fb_text(cx, cy, "Try these commands", C_ACCENT, C_WIN_BG, 1);
    cy += 22;

    const char *cmds[] = {
        "help    - list all commands",
        "about   - about MiniOS",
        "clear   - clear the status line",
        "reboot  - restart the machine",
    };
    for(int i = 0; i < 4; i++){
        fb_text(cx + 8, cy, cmds[i], C_TEXT, C_WIN_BG, 1);
        cy += 20;
    }

    cy += 8;
    fb_fill(con_x + 10, cy, con_w - 20, 1, C_BORDER);
    cy += 16;
    fb_text(cx, cy, "Status", C_ACCENT, C_WIN_BG, 1);
}

static void draw_status_area(void){
    int x = con_x + 30;
    int y = con_y + 20 + 40 + 22 + 16 + 22 + 4*20 + 8 + 16 + 22;
    fb_fill(con_x + 20, y, con_w - 40, 18, C_WIN_BG);
    fb_text(x, y, status_msg, C_TEXT_DIM, C_WIN_BG, 1);
}

static void draw_input_area(void){
    /* border + bg */
    fb_fill(input_x, input_y, input_w, input_h, C_BORDER);
    fb_fill(input_x + 1, input_y + 1, input_w - 2, input_h - 2, C_INPUT_BG);
    /* prompt */
    fb_text(input_x + 10, input_y + 14, ">", C_ACCENT, C_INPUT_BG, 1);
    /* typed text */
    fb_text(input_x + 30, input_y + 14, input_buf, C_TEXT, C_INPUT_BG, 1);
    /* caret */
    fb_fill(input_x + 30 + input_len*8 + 1, input_y + 12, 2, 12, C_ACCENT);
}

static void redraw_all(void){
    draw_desktop();
    draw_window();
    draw_content_static();
    draw_status_area();
    draw_input_area();
    draw_taskbar();
    cursor_draw();
}

/* ---------- commands ---------- */
static void run_command(void){
    input_buf[input_len] = 0;
    if(input_len == 0) return;

    if(streq(input_buf, "help")){
        scpy(status_msg, "help | about | clear | reboot");
    } else if(streq(input_buf, "about")){
        scpy(status_msg, "MiniOS v2.0 - 32-bit GUI, UEFI + BIOS, tiny");
    } else if(streq(input_buf, "clear")){
        status_msg[0] = 0;
    } else if(streq(input_buf, "reboot")){
        while(inb(0x64) & 2);
        outb(0x64, 0xFE);
    } else {
        scpy(status_msg, "Unknown: ");
        int l = slen(status_msg);
        scpy(status_msg + l, input_buf);
    }
    input_len = 0;
    input_buf[0] = 0;
    need_redraw_status = 1;
}

/* ---------- fallback for no framebuffer ---------- */
static void vga_puts(const char *s){
    volatile uint16_t *v = (volatile uint16_t*)0xB8000;
    int i = 0;
    while(*s && i < 80*25){ v[i++] = 0x0F00 | (uint8_t)*s; s++; }
}
static void vga_puthex(uint32_t x){
    const char *h = "0123456789ABCDEF";
    vga_puts("0x");
    for(int i = 28; i >= 0; i -= 4){
        volatile uint16_t *v = (volatile uint16_t*)0xB8000;
        static int pos = 2;
        v[pos++] = 0x0F00 | h[(x>>i)&0xF];
    }
}

/* ---------- entry ---------- */
void kernel_main(uint32_t magic, uint32_t mbi_addr){
    if(magic != 0x36D76289){ for(;;) __asm__ volatile("hlt"); }

    mb2_tag_t *tag = find_fb_tag(mbi_addr);
    if(!tag){
        vga_puts("MiniOS GUI needs a linear framebuffer. Firmware didn't provide one.");
        for(;;) __asm__ volatile("hlt");
    }
    mb2_fb_t *fb = (mb2_fb_t*)tag;
    if(fb->addr == 0 || fb->bpp < 16){
        vga_puts("MiniOS GUI needs a 16/24/32-bit linear framebuffer. Got: ");
        vga_puthex((uint32_t)fb->bpp);
        for(;;) __asm__ volatile("hlt");
    }

    g_fb     = (uint8_t*)(uintptr_t)fb->addr;
    g_pitch  = fb->pitch;
    g_width  = fb->width;
    g_height = fb->height;
    g_bpp    = fb->bpp;

    mouse_x = g_width  / 2;
    mouse_y = g_height / 2;
    if(mouse_x > (int)g_width  - CUR_W) mouse_x = g_width  - CUR_W;
    if(mouse_y > (int)g_height - CUR_H) mouse_y = g_height - CUR_H;

    mouse_init();
    compute_layout();
    redraw_all();

    for(;;){
        uint8_t status = inb(0x64);
        if(status & 1){
            uint8_t data = inb(0x60);
            if(status & 0x20) mouse_byte(data);
            else              kbd_byte(data);
        } else {
            __asm__ volatile("pause");
        }

        if(mouse_moved || need_redraw_input || need_redraw_status){
            cursor_erase();
            if(need_redraw_input){  draw_input_area();  need_redraw_input  = 0; }
            if(need_redraw_status){ draw_status_area(); need_redraw_status = 0; }
            cursor_draw();
            mouse_moved = 0;
        }
    }
}
