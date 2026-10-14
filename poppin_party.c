#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "poppin_party_logo.h"

#define DEFAULT_COLUMNS 80
#define DEFAULT_ROWS 24
#define CELL_WIDTH_RATIO 2

static int is_stdout_terminal(void)
{
#ifdef _WIN32
    return _isatty(_fileno(stdout));
#else
    return isatty(STDOUT_FILENO);
#endif
}

static void get_terminal_size(int *columns, int *rows)
{
    *columns = DEFAULT_COLUMNS;
    *rows = DEFAULT_ROWS;

#ifdef _WIN32
    {
        CONSOLE_SCREEN_BUFFER_INFO info;
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        if (output != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(output, &info)) {
            *columns = (int)(info.srWindow.Right - info.srWindow.Left + 1);
            *rows = (int)(info.srWindow.Bottom - info.srWindow.Top + 1);
        }
    }
#else
    {
        struct winsize size;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0 && size.ws_row > 0) {
            *columns = (int)size.ws_col;
            *rows = (int)size.ws_row;
        }
    }
#endif
}

static int enable_color(int *restore_mode, unsigned long *old_mode,
                        unsigned int *old_code_page)
{
    const char *no_color = getenv("NO_COLOR");
    const char *term = getenv("TERM");

    *restore_mode = 0;
    if (!is_stdout_terminal() || (no_color != NULL && no_color[0] != '\0') ||
        (term != NULL && strcmp(term, "dumb") == 0)) {
        return 0;
    }

#ifdef _WIN32
    {
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode;
        UINT code_page;

        if (output == INVALID_HANDLE_VALUE || !GetConsoleMode(output, &mode)) {
            return 0;
        }
        if (!SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            return 0;
        }

        code_page = GetConsoleOutputCP();
        *old_mode = (unsigned long)mode;
        *old_code_page = (unsigned int)code_page;
        *restore_mode = 1;
        SetConsoleOutputCP(CP_UTF8);
    }
#else
    (void)old_mode;
    (void)old_code_page;
    (void)restore_mode;
#endif
    return 1;
}

static void restore_console(int restore_mode, unsigned long old_mode,
                            unsigned int old_code_page)
{
#ifdef _WIN32
    if (restore_mode) {
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleMode(output, (DWORD)old_mode);
        SetConsoleOutputCP((UINT)old_code_page);
    }
#else
    (void)restore_mode;
    (void)old_mode;
    (void)old_code_page;
#endif
}

static void decode_logo(uint8_t *pixels)
{
    size_t at = 0;
    size_t run;

    for (run = 0; run < LOGO_RUN_COUNT && at < LOGO_PIXEL_COUNT; ++run) {
        uint16_t count = logo_runs[run].count;
        uint16_t i;
        for (i = 0; i < count && at < LOGO_PIXEL_COUNT; ++i) {
            pixels[at++] = logo_runs[run].value;
        }
    }
}

static void cell_appearance(const uint8_t *pixels, int x, int y, int width,
                            int height, char *glyph, unsigned int *color)
{
    static const char density[] = " .:-=+*#%@";
    double left = (double)x * LOGO_WIDTH / width;
    double right = (double)(x + 1) * LOGO_WIDTH / width;
    double top = (double)y * LOGO_HEIGHT / height;
    double bottom = (double)(y + 1) * LOGO_HEIGHT / height;
    double total_area = (right - left) * (bottom - top);
    double covered = 0.0;
    double color_weight[LOGO_PALETTE_SIZE] = {0.0};
    int px, py;
    int best_color = 0;
    double best_weight = 0.0;

    for (py = (int)top; py < (int)bottom + 1 && py < LOGO_HEIGHT; ++py) {
        double y0 = top > py ? top : (double)py;
        double y1 = bottom < py + 1 ? bottom : (double)(py + 1);
        double wy = y1 - y0;
        if (wy <= 0.0) continue;

        for (px = (int)left; px < (int)right + 1 && px < LOGO_WIDTH; ++px) {
            double x0 = left > px ? left : (double)px;
            double x1 = right < px + 1 ? right : (double)(px + 1);
            double area = (x1 - x0) * wy;
            uint8_t value;
            unsigned int alpha, palette;
            if (area <= 0.0) continue;

            value = pixels[(size_t)py * LOGO_WIDTH + (size_t)px];
            alpha = value >> 4;
            palette = value & 0x0f;
            if (alpha == 0 || palette == 0 || palette >= LOGO_PALETTE_SIZE) continue;

            covered += area * alpha / 15.0;
            color_weight[palette] += area * alpha;
        }
    }

    for (px = 1; px < LOGO_PALETTE_SIZE; ++px) {
        if (color_weight[px] > best_weight) {
            best_weight = color_weight[px];
            best_color = px;
        }
    }

    if (total_area <= 0.0 || covered <= 0.0 || best_color == 0) {
        *glyph = ' ';
        *color = 0;
        return;
    }

    {
        double coverage = covered / total_area;
        int index = (int)(coverage * (sizeof(density) - 1) + 0.5);
        if (index < 1) index = 1;
        if (index >= (int)sizeof(density) - 1) index = (int)sizeof(density) - 2;
        *glyph = density[index];
        *color = (unsigned int)best_color;
    }
}

static void sample_pixel(const uint8_t *pixels, int x, int y, int width,
                         int height, unsigned int *color)
{
    if (width == LOGO_WIDTH && height == LOGO_HEIGHT) {
        uint8_t value = pixels[(size_t)y * LOGO_WIDTH + (size_t)x];
        unsigned int alpha = value >> 4;
        unsigned int palette = value & 0x0f;
        *color = (alpha >= 5 && palette > 0 && palette < LOGO_PALETTE_SIZE)
                 ? palette : 0;
        return;
    }

    double left = (double)x * LOGO_WIDTH / width;
    double right = (double)(x + 1) * LOGO_WIDTH / width;
    double top = (double)y * LOGO_HEIGHT / height;
    double bottom = (double)(y + 1) * LOGO_HEIGHT / height;
    double total_area = (right - left) * (bottom - top);
    double covered = 0.0;
    double color_weight[LOGO_PALETTE_SIZE] = {0.0};
    int px, py, best_color = 0;
    double best_weight = 0.0;

    for (py = (int)top; py < (int)bottom + 1 && py < LOGO_HEIGHT; ++py) {
        double y0 = top > py ? top : (double)py;
        double y1 = bottom < py + 1 ? bottom : (double)(py + 1);
        double wy = y1 - y0;
        if (wy <= 0.0) continue;
        for (px = (int)left; px < (int)right + 1 && px < LOGO_WIDTH; ++px) {
            double x0 = left > px ? left : (double)px;
            double x1 = right < px + 1 ? right : (double)(px + 1);
            double area = (x1 - x0) * wy;
            uint8_t value;
            unsigned int alpha, palette;
            if (area <= 0.0) continue;
            value = pixels[(size_t)py * LOGO_WIDTH + (size_t)px];
            alpha = value >> 4;
            palette = value & 0x0f;
            if (alpha == 0 || palette == 0 || palette >= LOGO_PALETTE_SIZE) continue;
            covered += area * alpha / 15.0;
            color_weight[palette] += area * alpha;
        }
    }

    for (px = 1; px < LOGO_PALETTE_SIZE; ++px) {
        if (color_weight[px] > best_weight) {
            best_weight = color_weight[px];
            best_color = px;
        }
    }
    *color = (total_area > 0.0 && covered / total_area >= 0.30)
             ? (unsigned int)best_color : 0;
}

static void get_console_rgb(unsigned int palette, unsigned int *red,
                            unsigned int *green, unsigned int *blue)
{
    const uint8_t *rgb = logo_palette[palette];
    *red = rgb[0];
    *green = rgb[1];
    *blue = rgb[2];

    /* Lift the dark outline so it remains visible on a black terminal. */
    if (palette == 1) {
        *red = 132;
        *green = 91;
        *blue = 72;
    }
}

static void append_bytes(char *buffer, size_t capacity, size_t *length,
                         const char *bytes, size_t count)
{
    if (count <= capacity - *length) {
        memcpy(buffer + *length, bytes, count);
        *length += count;
    }
}

static void append_color(char *buffer, size_t capacity, size_t *length,
                         int foreground, unsigned int palette)
{
    int written;
    if (palette == 0) {
        const char *reset = foreground ? "\033[39m" : "\033[49m";
        append_bytes(buffer, capacity, length, reset, 5);
        return;
    }

    {
        unsigned int red, green, blue;
        get_console_rgb(palette, &red, &green, &blue);
        written = snprintf(buffer + *length, capacity - *length,
                           foreground ? "\033[38;2;%u;%u;%um"
                                      : "\033[48;2;%u;%u;%um",
                           red, green, blue);
    }
    if (written > 0 && (size_t)written < capacity - *length) {
        *length += (size_t)written;
    }
}

static void append_pixel_pair(char *buffer, size_t capacity, size_t *length,
                              unsigned int *current_fg, unsigned int *current_bg,
                              unsigned int top, unsigned int bottom)
{
    static const char full_block[] = "\xE2\x96\x88";
    static const char upper_half[] = "\xE2\x96\x80";
    static const char lower_half[] = "\xE2\x96\x84";
    unsigned int foreground = 0, background = 0;
    const char *glyph;
    size_t glyph_length = 3;

    if (top == 0 && bottom == 0) {
        glyph = " ";
        glyph_length = 1;
    } else if (top != 0 && top == bottom) {
        foreground = top;
        glyph = full_block;
    } else if (top != 0 && bottom != 0) {
        foreground = top;
        background = bottom;
        glyph = upper_half;
    } else if (top != 0) {
        foreground = top;
        glyph = upper_half;
    } else {
        foreground = bottom;
        glyph = lower_half;
    }

    if (foreground != *current_fg) {
        append_color(buffer, capacity, length, 1, foreground);
        *current_fg = foreground;
    }
    if (background != *current_bg) {
        append_color(buffer, capacity, length, 0, background);
        *current_bg = background;
    }
    append_bytes(buffer, capacity, length, glyph, glyph_length);
}

static void print_logo(const uint8_t *pixels, int columns, int rows, int color_enabled)
{
    int max_width = columns > 2 ? columns - 2 : columns;
    int max_rows = rows > 1 ? rows - 1 : rows;
    int width_by_height = (max_rows * CELL_WIDTH_RATIO * LOGO_WIDTH) / LOGO_HEIGHT;
    int width = max_width;
    int height, pixel_height, left_padding;
    int y, x;
    size_t capacity;
    char *line_buffer;
    static const char reset[] = "\033[0m";

    if (width > LOGO_WIDTH) width = LOGO_WIDTH;
    if (width > width_by_height) width = width_by_height;
    if (width < 1) width = 1;

    pixel_height = (width * LOGO_HEIGHT + LOGO_WIDTH - 1) / LOGO_WIDTH;
    if (color_enabled) {
        height = (pixel_height + 1) / 2;
    } else {
        height = (width * LOGO_HEIGHT + CELL_WIDTH_RATIO * LOGO_WIDTH - 1) /
                 (CELL_WIDTH_RATIO * LOGO_WIDTH);
    }
    if (height < 1) height = 1;
    left_padding = columns > width ? (columns - width) / 2 : 0;

    /* At most two RGB escape sequences and one UTF-8 glyph per terminal cell. */
    capacity = (size_t)width * 48u + (size_t)left_padding + 32u;
    line_buffer = (char *)malloc(capacity);
    if (line_buffer == NULL) {
        fputs("Unable to allocate the terminal output buffer.\n", stderr);
        return;
    }

    for (y = 0; y < height; ++y) {
        size_t length = 0;
        if (color_enabled) {
            append_bytes(line_buffer, capacity, &length, reset, sizeof(reset) - 1);
        }
        for (x = 0; x < left_padding; ++x) {
            line_buffer[length++] = ' ';
        }

        if (color_enabled) {
            unsigned int current_fg = 0, current_bg = 0;
            for (x = 0; x < width; ++x) {
                unsigned int top_color, bottom_color = 0;
                sample_pixel(pixels, x, y * 2, width, pixel_height, &top_color);
                if (y * 2 + 1 < pixel_height) {
                    sample_pixel(pixels, x, y * 2 + 1, width, pixel_height,
                                 &bottom_color);
                }
                append_pixel_pair(line_buffer, capacity, &length,
                                  &current_fg, &current_bg,
                                  top_color, bottom_color);
            }
            append_bytes(line_buffer, capacity, &length, reset, sizeof(reset) - 1);
        } else {
            for (x = 0; x < width; ++x) {
                char glyph;
                unsigned int palette;
                cell_appearance(pixels, x, y, width, height, &glyph, &palette);
                (void)palette;
                line_buffer[length++] = glyph;
            }
        }
        line_buffer[length++] = '\n';
        fwrite(line_buffer, 1, length, stdout);
    }
    free(line_buffer);
}

int main(void)
{
    uint8_t *pixels;
    int columns, rows;
    int restore_mode;
    unsigned long old_mode = 0;
    unsigned int old_code_page = 0;
    int color_enabled;

    pixels = (uint8_t *)malloc((size_t)LOGO_PIXEL_COUNT);
    if (pixels == NULL) {
        fputs("Unable to allocate the high-resolution logo buffer.\n", stderr);
        return 1;
    }

    decode_logo(pixels);
    get_terminal_size(&columns, &rows);
    color_enabled = enable_color(&restore_mode, &old_mode, &old_code_page);
    /* Avoid flushing every rendered line on terminals configured as line-buffered. */
    setvbuf(stdout, NULL, _IOFBF, 1024u * 1024u);
    print_logo(pixels, columns, rows, color_enabled);
    fflush(stdout);
    restore_console(restore_mode, old_mode, old_code_page);
    free(pixels);
    return 0;
}
