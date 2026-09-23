/*
 * hmi-sleep-daemon.c
 * Touch-Wake Display Sleep Monitor for MarknStamp i.MX6ULL HMI
 *
 * Monitors touchscreen input events non-exclusively.
 * When idle for SLEEP_TIMEOUT_SEC, blanks fb0 and powers down LCD backlight.
 * Wakes up immediately upon detecting touch input.
 * Dynamically reloads timeout from /etc/hmi-sleep.conf.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <linux/input.h>

#define CONF_FILE "/etc/hmi-sleep.conf"
#define DEFAULT_TIMEOUT_SEC 120
#define DEFAULT_BRIGHTNESS 7

static volatile int running = 1;
static int is_blanked = 0;
static int saved_brightness = DEFAULT_BRIGHTNESS;
static time_t conf_mtime = 0;
static int sleep_timeout_sec = DEFAULT_TIMEOUT_SEC;

static void sig_handler(int sig)
{
    (void)sig;
    running = 0;
}

static void write_sysfs_int(const char *path, int val)
{
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d\n", val);
        fclose(f);
    }
}

static int read_sysfs_int(const char *path, int def)
{
    FILE *f = fopen(path, "r");
    if (f) {
        int val = def;
        if (fscanf(f, "%d", &val) == 1) {
            fclose(f);
            return val;
        }
        fclose(f);
    }
    return def;
}

static void set_display_blank(int blank)
{
    DIR *dir;
    struct dirent *ent;
    char path[512];

    if (blank == is_blanked)
        return;

    dir = opendir("/sys/class/backlight");
    if (dir) {
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.')
                continue;

            snprintf(path, sizeof(path), "/sys/class/backlight/%s/brightness", ent->d_name);
            if (blank) {
                int curr = read_sysfs_int(path, DEFAULT_BRIGHTNESS);
                if (curr > 0)
                    saved_brightness = curr;
                write_sysfs_int(path, 0);
            } else {
                write_sysfs_int(path, (saved_brightness > 0) ? saved_brightness : DEFAULT_BRIGHTNESS);
            }

            snprintf(path, sizeof(path), "/sys/class/backlight/%s/bl_power", ent->d_name);
            write_sysfs_int(path, blank ? 1 : 0);
        }
        closedir(dir);
    }

    write_sysfs_int("/sys/class/graphics/fb0/blank", blank ? 1 : 0);
    is_blanked = blank;
    printf("[hmi-sleep] Display state: %s\n", blank ? "SLEEP (blanked)" : "AWAKE (unblanked)");
    fflush(stdout);
}

static void check_config(void)
{
    struct stat st;
    if (stat(CONF_FILE, &st) != 0)
        return;

    if (st.st_mtime != conf_mtime) {
        conf_mtime = st.st_mtime;
        FILE *f = fopen(CONF_FILE, "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                char *p = line;
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '#' || *p == '\n' || *p == '\0') continue;

                if (strncmp(p, "SLEEP_TIMEOUT_SEC=", 18) == 0) {
                    sleep_timeout_sec = atoi(p + 18);
                } else if (strncmp(p, "TIMEOUT=", 8) == 0) {
                    sleep_timeout_sec = atoi(p + 8);
                }
            }
            fclose(f);
            printf("[hmi-sleep] Loaded config: sleep timeout = %d seconds (%s)\n",
                   sleep_timeout_sec, sleep_timeout_sec <= 0 ? "DISABLED" : "ENABLED");
            fflush(stdout);
        }
    }
}

static int open_touchscreen(char *out_path, size_t max_len)
{
    if (access("/dev/input/touchscreen0", R_OK) == 0) {
        int fd = open("/dev/input/touchscreen0", O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
            snprintf(out_path, max_len, "/dev/input/touchscreen0");
            return fd;
        }
    }

    /* Fallback: scan event devices for touch */
    char devpath[64];
    for (int i = 0; i < 10; i++) {
        snprintf(devpath, sizeof(devpath), "/dev/input/event%d", i);
        if (access(devpath, R_OK) == 0) {
            int fd = open(devpath, O_RDONLY | O_NONBLOCK);
            if (fd >= 0) {
                char name[128] = {0};
                if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0) {
                    if (strcasestr(name, "goodix") || strcasestr(name, "touch") || strcasestr(name, "ts")) {
                        snprintf(out_path, max_len, "%s", devpath);
                        return fd;
                    }
                }
                close(fd);
            }
        }
    }

    if (access("/dev/input/event0", R_OK) == 0) {
        int fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
            snprintf(out_path, max_len, "/dev/input/event0");
            return fd;
        }
    }

    return -1;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    printf("[hmi-sleep] Starting HMI touch-wake sleep daemon...\n");
    fflush(stdout);

    check_config();
    set_display_blank(0);

    char ts_path[64] = {0};
    int ts_fd = -1;

    while (running && ts_fd < 0) {
        ts_fd = open_touchscreen(ts_path, sizeof(ts_path));
        if (ts_fd < 0) {
            sleep(1);
        }
    }

    if (ts_fd < 0) {
        fprintf(stderr, "[hmi-sleep] Error: Unable to open touch input device.\n");
        return 1;
    }

    printf("[hmi-sleep] Monitoring touch device: %s\n", ts_path);
    fflush(stdout);

    struct timeval last_active;
    gettimeofday(&last_active, NULL);

    while (running) {
        check_config();

        struct timeval now;
        gettimeofday(&now, NULL);

        int poll_timeout_ms = 1000;
        if (sleep_timeout_sec > 0 && !is_blanked) {
            long elapsed = now.tv_sec - last_active.tv_sec;
            long remaining = sleep_timeout_sec - elapsed;
            if (remaining <= 0) {
                set_display_blank(1);
                poll_timeout_ms = 1000;
            } else {
                poll_timeout_ms = (int)(remaining * 1000);
                if (poll_timeout_ms > 1000)
                    poll_timeout_ms = 1000;
            }
        }

        struct pollfd pfd;
        pfd.fd = ts_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int ret = poll(&pfd, 1, poll_timeout_ms);
        gettimeofday(&now, NULL);

        if (ret > 0 && (pfd.revents & POLLIN)) {
            struct input_event ev[16];
            ssize_t n = read(ts_fd, ev, sizeof(ev));
            if (n > 0) {
                int touch_detected = 0;
                size_t count = n / sizeof(struct input_event);
                for (size_t i = 0; i < count; i++) {
                    if (ev[i].type == EV_KEY || ev[i].type == EV_ABS) {
                        touch_detected = 1;
                        break;
                    }
                }

                if (touch_detected) {
                    if (is_blanked) {
                        set_display_blank(0);
                    }
                    last_active = now;
                }
            } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                close(ts_fd);
                ts_fd = -1;
                while (running && ts_fd < 0) {
                    sleep(1);
                    ts_fd = open_touchscreen(ts_path, sizeof(ts_path));
                }
            }
        } else if (ret == 0) {
            if (sleep_timeout_sec > 0 && !is_blanked) {
                long elapsed = now.tv_sec - last_active.tv_sec;
                if (elapsed >= sleep_timeout_sec) {
                    set_display_blank(1);
                }
            }
        }
    }

    set_display_blank(0);
    if (ts_fd >= 0)
        close(ts_fd);

    printf("[hmi-sleep] Daemon exiting cleanly.\n");
    return 0;
}
