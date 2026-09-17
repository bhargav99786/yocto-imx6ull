#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

#pragma pack(push, 1)
typedef struct {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
} BMPHeader;

typedef struct {
    uint32_t size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bpp;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_ppm;
    int32_t  y_ppm;
    uint32_t clr_used;
    uint32_t clr_important;
} BMPInfoHeader;
#pragma pack(pop)

static inline uint16_t rgb_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

int main(int argc, char *argv[]) {
    const char *img_path = (argc > 1) ? argv[1] : "/etc/loading.bmp";
    const char *fb_dev = (argc > 2) ? argv[2] : "/dev/fb0";

    int img_fd = open(img_path, O_RDONLY);
    if (img_fd < 0) {
        // Fallback check
        img_path = "/etc/loading_32bpp.raw";
        img_fd = open(img_path, O_RDONLY);
        if (img_fd < 0) {
            fprintf(stderr, "fbshow: failed to open splash file\n");
            return 1;
        }
    }

    int fb_fd = open(fb_dev, O_RDWR);
    if (fb_fd < 0) {
        perror("fbshow: open fb");
        close(img_fd);
        return 1;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0 ||
        ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        perror("fbshow: ioctl fb");
        close(fb_fd);
        close(img_fd);
        return 1;
    }

    // Unblank screen
    ioctl(fb_fd, FBIOBLANK, FB_BLANK_UNBLANK);

    size_t screensize = finfo.line_length * vinfo.yres;
    uint8_t *fbp = (uint8_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fbp == MAP_FAILED) {
        perror("fbshow: mmap fb");
        close(fb_fd);
        close(img_fd);
        return 1;
    }

    // Read header to determine if BMP or RAW
    BMPHeader bmph;
    if (read(img_fd, &bmph, sizeof(bmph)) == sizeof(bmph) && bmph.type == 0x4D42) {
        BMPInfoHeader info;
        if (read(img_fd, &info, sizeof(info)) == sizeof(info) && (info.bpp == 24 || info.bpp == 32)) {
            int src_w = info.width;
            int src_h = info.height;
            int bottom_up = 1;
            if (src_h < 0) {
                bottom_up = 0;
                src_h = -src_h;
            }

            int row_padded = ((src_w * (info.bpp / 8) + 3) & (~3));
            uint8_t *row_buf = (uint8_t *)malloc(row_padded);
            if (!row_buf) {
                munmap(fbp, screensize);
                close(fb_fd);
                close(img_fd);
                return 1;
            }

            lseek(img_fd, bmph.offset, SEEK_SET);

            int draw_w = (src_w < (int)vinfo.xres) ? src_w : (int)vinfo.xres;
            int draw_h = (src_h < (int)vinfo.yres) ? src_h : (int)vinfo.yres;
            int off_x = ((int)vinfo.xres - draw_w) / 2;
            int off_y = ((int)vinfo.yres - draw_h) / 2;

            for (int r = 0; r < src_h; r++) {
                if (read(img_fd, row_buf, row_padded) != row_padded)
                    break;

                int y = bottom_up ? (src_h - 1 - r) : r;
                if (y >= draw_h) continue;

                int dst_y = off_y + y;
                uint8_t *dst_row = fbp + dst_y * finfo.line_length;

                for (int x = 0; x < draw_w; x++) {
                    int dst_x = off_x + x;
                    int src_idx = x * (info.bpp / 8);
                    uint8_t b = row_buf[src_idx];
                    uint8_t g = row_buf[src_idx + 1];
                    uint8_t r_val = row_buf[src_idx + 2];

                    if (vinfo.bits_per_pixel == 32) {
                        uint32_t *dst_pixel = (uint32_t *)(dst_row + dst_x * 4);
                        // Write BGRA/BGRX or RGBX based on red offset
                        if (vinfo.red.offset == 16) {
                            *dst_pixel = (b) | (g << 8) | (r_val << 16);
                        } else {
                            *dst_pixel = (r_val) | (g << 8) | (b << 16);
                        }
                    } else if (vinfo.bits_per_pixel == 16) {
                        uint16_t *dst_pixel = (uint16_t *)(dst_row + dst_x * 2);
                        *dst_pixel = rgb_to_rgb565(r_val, g, b);
                    }
                }
            }
            free(row_buf);
            munmap(fbp, screensize);
            close(fb_fd);
            close(img_fd);
            return 0;
        }
    }

    // If not BMP or unsupported BMP, check if raw file matches screen size
    lseek(img_fd, 0, SEEK_SET);
    size_t bytes_read = read(img_fd, fbp, screensize);
    (void)bytes_read;

    munmap(fbp, screensize);
    close(fb_fd);
    close(img_fd);
    return 0;
}
