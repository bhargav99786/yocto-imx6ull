#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <math.h>

static volatile int g_running = 1;

void sig_handler(int sig) {
    (void)sig;
    g_running = 0;
}

// Framebuffer structures
typedef struct {
    int fd;
    uint8_t *fbp;
    long screensize;
    int width;
    int height;
    int bpp;
    int line_length;
} fb_ctx_t;

static fb_ctx_t g_fb = { -1, NULL, 0, 0, 0, 0, 0 };

static int init_fb(fb_ctx_t *fb) {
    fb->fd = open("/dev/fb0", O_RDWR);
    if (fb->fd < 0) {
        return -1;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &finfo) < 0 ||
        ioctl(fb->fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        close(fb->fd);
        fb->fd = -1;
        return -1;
    }

    fb->width = vinfo.xres;
    fb->height = vinfo.yres;
    fb->bpp = vinfo.bits_per_pixel;
    fb->line_length = finfo.line_length;
    fb->screensize = finfo.smem_len ? finfo.smem_len : (vinfo.yres * finfo.line_length);

    fb->fbp = (uint8_t *)mmap(0, fb->screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);
    if (fb->fbp == MAP_FAILED) {
        fb->fbp = NULL;
        close(fb->fd);
        fb->fd = -1;
        return -1;
    }

    return 0;
}

static void close_fb(fb_ctx_t *fb) {
    if (fb->fbp && fb->fbp != MAP_FAILED) {
        munmap(fb->fbp, fb->screensize);
        fb->fbp = NULL;
    }
    if (fb->fd >= 0) {
        close(fb->fd);
        fb->fd = -1;
    }
}

static inline void put_pixel(fb_ctx_t *fb, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= fb->width || y < 0 || y >= fb->height || !fb->fbp) return;

    if (fb->bpp == 32) {
        uint32_t *pixel = (uint32_t *)(fb->fbp + y * fb->line_length + x * 4);
        *pixel = (r << 16) | (g << 8) | b;
    } else if (fb->bpp == 16) {
        uint16_t *pixel = (uint16_t *)(fb->fbp + y * fb->line_length + x * 2);
        *pixel = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }
}

static void draw_rect(fb_ctx_t *fb, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
    for (int x = x0; x <= x1; x++) {
        put_pixel(fb, x, y0, r, g, b);
        put_pixel(fb, x, y1, r, g, b);
    }
    for (int y = y0; y <= y1; y++) {
        put_pixel(fb, x0, y, r, g, b);
        put_pixel(fb, x1, y, r, g, b);
    }
}

static void fill_rect(fb_ctx_t *fb, int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            put_pixel(fb, x, y, r, g, b);
        }
    }
}

static void draw_touch_crosshair(fb_ctx_t *fb, int cx, int cy, uint8_t r, uint8_t g, uint8_t b) {
    int radius = 18;
    for (int i = -radius; i <= radius; i++) {
        put_pixel(fb, cx + i, cy, r, g, b);
        put_pixel(fb, cx + i, cy + 1, r, g, b);
        put_pixel(fb, cx + i, cy - 1, r, g, b);
        put_pixel(fb, cx, cy + i, r, g, b);
        put_pixel(fb, cx + 1, cy + i, r, g, b);
        put_pixel(fb, cx - 1, cy + i, r, g, b);
    }
    // Small solid circle center
    for (int dy = -4; dy <= 4; dy++) {
        for (int dx = -4; dx <= 4; dx++) {
            if (dx*dx + dy*dy <= 16) {
                put_pixel(fb, cx + dx, cy + dy, 255, 255, 255);
            }
        }
    }
}

static void render_test_ui(fb_ctx_t *fb, int q1_hit, int q2_hit, int q3_hit, int q4_hit) {
    if (!fb->fbp) return;

    int w = fb->width;
    int h = fb->height;
    int mid_x = w / 2;
    int mid_y = h / 2;

    // Background dark theme
    fill_rect(fb, 0, 0, w - 1, h - 1, 15, 20, 28);

    // Section 1: Top-Left
    fill_rect(fb, 4, 4, mid_x - 4, mid_y - 4, q1_hit ? 20 : 10, q1_hit ? 60 : 30, q1_hit ? 30 : 20);
    draw_rect(fb, 4, 4, mid_x - 4, mid_y - 4, q1_hit ? 76 : 38, q1_hit ? 175 : 85, q1_hit ? 80 : 40);

    // Section 2: Top-Right
    fill_rect(fb, mid_x + 4, 4, w - 5, mid_y - 4, q2_hit ? 20 : 15, q2_hit ? 45 : 25, q2_hit ? 80 : 45);
    draw_rect(fb, mid_x + 4, 4, w - 5, mid_y - 4, q2_hit ? 33 : 16, q2_hit ? 150 : 75, q2_hit ? 243 : 120);

    // Section 3: Bottom-Left
    fill_rect(fb, 4, mid_y + 4, mid_x - 4, h - 5, q3_hit ? 70 : 35, q3_hit ? 60 : 30, q3_hit ? 10 : 5);
    draw_rect(fb, 4, mid_y + 4, mid_x - 4, h - 5, q3_hit ? 255 : 128, q3_hit ? 193 : 96, q3_hit ? 7 : 4);

    // Section 4: Bottom-Right
    fill_rect(fb, mid_x + 4, mid_y + 4, w - 5, h - 5, q4_hit ? 60 : 30, q4_hit ? 20 : 10, q4_hit ? 60 : 30);
    draw_rect(fb, mid_x + 4, mid_y + 4, w - 5, h - 5, q4_hit ? 233 : 116, q4_hit ? 30 : 15, q4_hit ? 99 : 50);

    // Center dividing crosshairs
    for (int x = 0; x < w; x++) {
        put_pixel(fb, x, mid_y, 80, 100, 120);
    }
    for (int y = 0; y < h; y++) {
        put_pixel(fb, mid_x, y, 80, 100, 120);
    }
}

// Auto-detect touchscreen device
static int find_touchscreen_device(char *out_path, size_t max_len) {
    for (int i = 0; i < 10; i++) {
        char devpath[64];
        snprintf(devpath, sizeof(devpath), "/dev/input/event%d", i);
        int fd = open(devpath, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        char name[128] = {0};
        ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name);

        unsigned long evbits[1] = {0};
        ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), evbits);

        // Check for EV_ABS (bit 3)
        if (evbits[0] & (1 << EV_ABS)) {
            unsigned long absbits[2] = {0};
            ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits);

            // Check for ABS_X or ABS_MT_POSITION_X (0x35 = 53)
            int has_abs_x = (absbits[0] & (1 << ABS_X)) != 0;
            int has_mt_x = (absbits[53 / 32] & (1 << (53 % 32))) != 0;

            if (has_abs_x || has_mt_x || strstr(name, "touch") || strstr(name, "Touch") || strstr(name, "Goodix")) {
                strncpy(out_path, devpath, max_len - 1);
                close(fd);
                return 0;
            }
        }
        close(fd);
    }
    return -1;
}

static void print_abs_info(int fd, int code, const char *label) {
    struct input_absinfo abs;
    if (ioctl(fd, EVIOCGABS(code), &abs) >= 0) {
        printf("   %-22s : min=%-4d  max=%-4d  res=%-3d  fuzz=%-2d\n",
               label, abs.minimum, abs.maximum, abs.resolution, abs.fuzz);
    }
}

int main(int argc, char **argv) {
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    char devpath[128] = {0};
    if (argc > 1) {
        strncpy(devpath, argv[1], sizeof(devpath) - 1);
    } else {
        if (find_touchscreen_device(devpath, sizeof(devpath)) < 0) {
            strncpy(devpath, "/dev/input/event0", sizeof(devpath) - 1);
        }
    }

    printf("\n\033[1;36m======================================================================\033[0m\n");
    printf("\033[1;36m          EMBEDDED TOUCHSCREEN HARDWARE DIAGNOSTIC & TEST UTILITY     \033[0m\n");
    printf("\033[1;36m======================================================================\033[0m\n");

    int fd = open(devpath, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "\033[1;31m[ERROR] Failed to open %s: %s\033[0m\n", devpath, strerror(errno));
        printf("Available /dev/input devices:\n");
        system("ls -l /dev/input/event* 2>/dev/null || ls -l /dev/input/ 2>/dev/null");
        return 1;
    }

    char dev_name[128] = "Unknown";
    ioctl(fd, EVIOCGNAME(sizeof(dev_name) - 1), dev_name);

    struct input_id id;
    ioctl(fd, EVIOCGID, &id);

    printf("\033[1;32m[DEVICE DETECTED]\033[0m\n");
    printf("   Path     : %s\n", devpath);
    printf("   Name     : %s\n", dev_name);
    printf("   Bus/ID   : Bus=0x%04x Vendor=0x%04x Product=0x%04x Version=0x%04x\n",
           id.bustype, id.vendor, id.product, id.version);

    printf("\n\033[1;32m[AXIS RESOLUTION & BOUNDS]\033[0m\n");
    print_abs_info(fd, ABS_X, "ABS_X");
    print_abs_info(fd, ABS_Y, "ABS_Y");
    print_abs_info(fd, ABS_MT_POSITION_X, "ABS_MT_POSITION_X");
    print_abs_info(fd, ABS_MT_POSITION_Y, "ABS_MT_POSITION_Y");

    // Check if framebuffer exists
    int fb_ok = (init_fb(&g_fb) == 0);
    if (fb_ok) {
        printf("\n\033[1;32m[FRAMEBUFFER ACTIVE]\033[0m /dev/fb0: %dx%d @ %dbpp\n",
               g_fb.width, g_fb.height, g_fb.bpp);
        printf("   Live touch visual grid active on display!\n");
        render_test_ui(&g_fb, 0, 0, 0, 0);
    } else {
        printf("\n\033[1;33m[FRAMEBUFFER NOTE]\033[0m /dev/fb0 not accessible or busy (Console mode active).\n");
    }

    printf("\n\033[1;33m[QUADRANT SCHEME (Display 1024x600)]\033[0m\n");
    printf("  +--------------------------------+--------------------------------+\n");
    printf("  |  \033[1;32mSECTION 1: TOP-LEFT\033[0m           |  \033[1;34mSECTION 2: TOP-RIGHT\033[0m          |\n");
    printf("  |  X: 0 -> 511,  Y: 0 -> 299     |  X: 512 -> 1023, Y: 0 -> 299   |\n");
    printf("  +--------------------------------+--------------------------------+\n");
    printf("  |  \033[1;33mSECTION 3: BOTTOM-LEFT\033[0m        |  \033[1;35mSECTION 4: BOTTOM-RIGHT\033[0m       |\n");
    printf("  |  X: 0 -> 511,  Y: 300 -> 599   |  X: 512 -> 1023, Y: 300 -> 599 |\n");
    printf("  +--------------------------------+--------------------------------+\n\n");
    printf("\033[1;37mTouch the screen in all 4 corners. Press Ctrl+C to finish & view report.\033[0m\n\n");
    fflush(stdout);

    int raw_x = -1, raw_y = -1;
    int prev_screen_x = -1, prev_screen_y = -1;
    int is_touching = 0;
    long touch_events_count = 0;
    long touch_sessions = 0;

    int min_raw_x = 99999, max_raw_x = -1;
    int min_raw_y = 99999, max_raw_y = -1;

    int q1_hits = 0, q2_hits = 0, q3_hits = 0, q4_hits = 0;

    struct input_event ev;
    while (g_running && read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X) raw_x = ev.value;
            if (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y) raw_y = ev.value;
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            if (ev.value == 1) {
                is_touching = 1;
                touch_sessions++;
            } else if (ev.value == 0) {
                is_touching = 0;
                printf("  --> \033[1;30m[TOUCH UP / RELEASE]\033[0m\n");
                fflush(stdout);
            }
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
            if (raw_x >= 0 && raw_y >= 0) {
                touch_events_count++;

                if (raw_x < min_raw_x) min_raw_x = raw_x;
                if (raw_x > max_raw_x) max_raw_x = raw_x;
                if (raw_y < min_raw_y) min_raw_y = raw_y;
                if (raw_y > max_raw_y) max_raw_y = raw_y;

                /*
                 * Standard transformation matrix: "0 1 0 1 0 0"
                 * Screen_X = (raw_y / 600.0) * 1024
                 * Screen_Y = (raw_x / 1024.0) * 600
                 */
                float u = (float)raw_x / 1024.0f;
                float v = (float)raw_y / 600.0f;

                int screen_x = (int)(v * 1024.0f);
                int screen_y = (int)(u * 600.0f);

                if (screen_x < 0) screen_x = 0;
                if (screen_x > 1023) screen_x = 1023;
                if (screen_y < 0) screen_y = 0;
                if (screen_y > 599) screen_y = 599;

                const char *section_name = "";
                const char *color_code = "";
                int q_num = 0;

                if (screen_x < 512 && screen_y < 300) {
                    section_name = "[ SECTION 1: TOP-LEFT     ]";
                    color_code = "\033[1;32m";
                    q1_hits++;
                    q_num = 1;
                } else if (screen_x >= 512 && screen_y < 300) {
                    section_name = "[ SECTION 2: TOP-RIGHT    ]";
                    color_code = "\033[1;34m";
                    q2_hits++;
                    q_num = 2;
                } else if (screen_x < 512 && screen_y >= 300) {
                    section_name = "[ SECTION 3: BOTTOM-LEFT  ]";
                    color_code = "\033[1;33m";
                    q3_hits++;
                    q_num = 3;
                } else {
                    section_name = "[ SECTION 4: BOTTOM-RIGHT ]";
                    color_code = "\033[1;35m";
                    q4_hits++;
                    q_num = 4;
                }

                // Print terminal log
                printf("RAW: (X=%4d, Y=%4d) -> SCREEN: (%4d, %4d) %s%s\033[0m\n",
                       raw_x, raw_y, screen_x, screen_y, color_code, section_name);
                fflush(stdout);

                // Update Framebuffer
                if (fb_ok && (abs(screen_x - prev_screen_x) > 3 || abs(screen_y - prev_screen_y) > 3)) {
                    render_test_ui(&g_fb, q1_hits > 0, q2_hits > 0, q3_hits > 0, q4_hits > 0);
                    draw_touch_crosshair(&g_fb, screen_x, screen_y, 0, 210, 255);
                    prev_screen_x = screen_x;
                    prev_screen_y = screen_y;
                }
            }
        }
    }

    close(fd);
    if (fb_ok) close_fb(&g_fb);

    // ================= SUMMARY REPORT =================
    printf("\n\033[1;36m======================================================================\033[0m\n");
    printf("\033[1;36m                      TOUCHSCREEN TEST SUMMARY                        \033[0m\n");
    printf("\033[1;36m======================================================================\033[0m\n");
    printf("  Total Touch Samples Received : %ld\n", touch_events_count);
    printf("  Observed Raw X Bounds        : %d -> %d\n", (min_raw_x == 99999 ? 0 : min_raw_x), (max_raw_x == -1 ? 0 : max_raw_x));
    printf("  Observed Raw Y Bounds        : %d -> %d\n", (min_raw_y == 99999 ? 0 : min_raw_y), (max_raw_y == -1 ? 0 : max_raw_y));
    printf("\n  \033[1mQUADRANT VERIFICATION:\033[0m\n");
    printf("    Section 1 [Top-Left]     : %s (%d hits)\033[0m\n", q1_hits ? "\033[1;32m[PASS] VERIFIED" : "\033[1;31m[FAIL] NOT DETECTED", q1_hits);
    printf("    Section 2 [Top-Right]    : %s (%d hits)\033[0m\n", q2_hits ? "\033[1;32m[PASS] VERIFIED" : "\033[1;31m[FAIL] NOT DETECTED", q2_hits);
    printf("    Section 3 [Bottom-Left]  : %s (%d hits)\033[0m\n", q3_hits ? "\033[1;32m[PASS] VERIFIED" : "\033[1;31m[FAIL] NOT DETECTED", q3_hits);
    printf("    Section 4 [Bottom-Right] : %s (%d hits)\033[0m\n", q4_hits ? "\033[1;32m[PASS] VERIFIED" : "\033[1;31m[FAIL] NOT DETECTED", q4_hits);

    int total_pass = (q1_hits > 0) + (q2_hits > 0) + (q3_hits > 0) + (q4_hits > 0);
    printf("\n  \033[1mOVERALL HEALTH VERDICT:\033[0m ");
    if (total_pass == 4) {
        printf("\033[1;32mEXCELLENT! 100%% Edge-to-Edge Touch Coverage Verified.\033[0m\n");
    } else if (total_pass > 0) {
        printf("\033[1;33mPARTIAL: %d/4 Quadrants Responded. Check digitizer cables/calibration.\033[0m\n", total_pass);
    } else {
        printf("\033[1;31mNO TOUCH DETECTED! Check I2C bus, interrupt line, or driver binding.\033[0m\n");
    }
    printf("\033[1;36m======================================================================\033[0m\n\n");

    return 0;
}
