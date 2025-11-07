#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

#include <termios.h>
#include <signal.h>

#define BAR2_SIZE (64 * 1024)

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define SCREEN_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)

#define REG_OUTPUT      0x00
#define REG_CURSOR_POS  0x04
#define REG_TEXT_COLOR  0x08
#define REG_BG_COLOR    0x0C

#define CLEAR_REQUEST_FLAG     0x0F01

#define SCREEN_HISTORY 10  // Храним 100 строк истории
#define TOTAL_HEIGHT (SCREEN_HEIGHT + SCREEN_HISTORY)

#define REG_INTERRUPT_ENABLE  0x14

typedef struct {
    char ch;
    int text_color;
    int bg_color;
} screen_cell;

screen_cell screen[TOTAL_HEIGHT][SCREEN_WIDTH];
int start_line = 0;  // Первая отображаемая строка в буфере
int total_lines = 0; // Общее количество заполненных строк
int cursor_pos = 0;  // Абсолютная позиция курсора в буфере
int text_color = 7; // Белый по умолчанию
int bg_color = 0;   // Черный по умолчанию

// Глобальные переменные для обработки сигналов
static struct termios old_termios;

// Функция для восстановления старого режима терминала
void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    printf("\033[?25h\n"); // Показываем курсор
}

// Функция для установки неблокирующего ввода
void set_nonblocking_terminal() {
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &old_termios);
    new_termios = old_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    printf("\033[?25l"); // Скрываем курсор терминала
}

// Обработчик сигналов для корректного восстановления терминала
void signal_handler(int sig) {
    restore_terminal();
    exit(0);
}

// Функция для обработки нажатий клавиш
int handle_keypress() {
    struct timeval tv;
    fd_set fds;

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
        char key[3];

        int bytes = read(STDIN_FILENO, key, sizeof(key));

        if (bytes > 0) {
            // Записываем в лог для отладки
            FILE *log = fopen("keylog.txt", "a");
            if (log) {
                fprintf(log, "Key pressed: %d bytes: ", bytes);
                for (int i = 0; i < bytes; i++) {
                    fprintf(log, "0x%02X ", (unsigned char)key[i]);
                }
                fprintf(log, "\n");
                fclose(log);
            }

            if (bytes == 1) {
                if (key[0] == 'q' || key[0] == 'Q') {
                    return -1;
                }
            } else if (bytes == 3) {
                FILE *log = fopen("keylog.txt", "a");
                if (log) {
                    fprintf(log, " KEY  -> %d %d %d\n",  key[0], key[1], key[2]);
                    fclose(log);
                }
                // Проверяем последовательность стрелочек
                if (key[0] == 0x1B && key[1] == 0x5B) {
                    switch (key[2]) {
                        case 0x41: // Стрелка вверх
                            // Логируем успешное нажатие
                            log = fopen("keylog.txt", "a");
                            if (log) {
                                fprintf(log, "UP\n",  start_line);
                                fclose(log);
                            }

                            if (start_line > 0) {
                                start_line--;
                                // Логируем успешное нажатие
                                log = fopen("keylog.txt", "a");
                                if (log) {
                                    fprintf(log, "UP ARROW: start_line  -> %d\n",  start_line);
                                    fclose(log);
                                }
                                return 1;
                            }
                            break;
                        case 0x42: // Стрелка вниз
                            if (start_line < total_lines - SCREEN_HEIGHT && total_lines > SCREEN_HEIGHT) {
                                log = fopen("keylog.txt", "a");
                                if (log) {
                                    fprintf(log, "DOWN\n",  start_line);
                                    fclose(log);
                                }
                                start_line++;
                                // Логируем успешное нажатие
                                log = fopen("keylog.txt", "a");
                                if (log) {
                                    fprintf(log, "DOWN ARROW: start_line -> %d\n",  start_line);
                                    fclose(log);
                                }
                                return 1;
                            }
                            break;
                    }
                }
            }
        }
    }
    return 0;
}

// ANSI цвета для отображения в терминале
const char* ansi_colors[] = {
    "\033[30m", "\033[34m", "\033[32m", "\033[36m", "\033[31m", "\033[35m", "\033[33m", "\033[37m",
    "\033[90m", "\033[94m", "\033[92m", "\033[96m", "\033[91m", "\033[95m", "\033[93m", "\033[97m"
};

const char* ansi_bg_colors[] = {
    "\033[40m", "\033[44m", "\033[42m", "\033[46m", "\033[41m", "\033[45m", "\033[43m", "\033[47m",
    "\033[100m", "\033[104m", "\033[102m", "\033[106m", "\033[101m", "\033[105m", "\033[103m", "\033[107m"
};

void clear_screen() {
    for (int y = 0; y < TOTAL_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            screen[y][x].ch = ' ';
            screen[y][x].text_color = text_color;
            screen[y][x].bg_color = bg_color;
        }
    }
    start_line = 0;
    total_lines = 0;
    cursor_pos = 0;
    text_color = 7;
    bg_color = 0;
}

void display_screen() {
    printf("\033[1;1H"); // Курсор в начало видимого экрана

    // Рисуем видимую часть экрана
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        int buffer_line = start_line + y;

        if (buffer_line < total_lines && buffer_line < TOTAL_HEIGHT) {
            for (int x = 0; x < SCREEN_WIDTH; x++) {
                screen_cell cell = screen[buffer_line][x];
                printf("%s%s%c",
                       ansi_colors[cell.text_color],
                       ansi_bg_colors[cell.bg_color],
                       cell.ch);
            }
        } else {
            // Пустая строка
            printf("\033[0m");
            for (int x = 0; x < SCREEN_WIDTH; x++) {
                printf(" ");
            }
        }
        if (y < SCREEN_HEIGHT - 1) printf("\n");
    }

    // Показываем индикатор скроллинга в правом нижнем углу
    printf("\033[%d;%dH\033[0m", SCREEN_HEIGHT, 1);
    if (start_line > 0) {
        printf("_up_");
    }
    if (start_line < total_lines - SCREEN_HEIGHT) {
        printf("_down_");
    }

    fflush(stdout);
}

void process_character(char c) {
    int x = cursor_pos % SCREEN_WIDTH;
    int y = cursor_pos / SCREEN_WIDTH;

    FILE *log = fopen("log.txt", "a");
    if (log) {
        fprintf(log, "START: char=0x%02x('%c'), cursor_pos=%d, x=%d, y=%d, total_lines=%d, start_line=%d\n",
                c, (c >= 32 && c <= 126) ? c : ' ', cursor_pos, x, y, total_lines, start_line);
        fclose(log);
    }

    switch (c) {
        case 0x0C:
            if (total_lines > 0) {
                // Увеличиваем start_line, чтобы показать, что есть история выше
                if (start_line < total_lines - SCREEN_HEIGHT) {
                    start_line++;
                }
            }
            for (int i = 0; i < TOTAL_HEIGHT - 1; i++) {
                memcpy(screen[i], screen[i + 1], sizeof(screen_cell) * SCREEN_WIDTH);
            }
            // Очищаем последнюю строку
            for (int j = 0; j < SCREEN_WIDTH; j++) {
                screen[TOTAL_HEIGHT - 1][j].ch = ' ';
                screen[TOTAL_HEIGHT - 1][j].text_color = text_color;
                screen[TOTAL_HEIGHT - 1][j].bg_color = bg_color;
            }
            // Корректируем позиции
            if (y > 0) y--;
            cursor_pos = y * SCREEN_WIDTH + x;

            if (total_lines < TOTAL_HEIGHT) {
                total_lines++;
            }

            // Устанавливаем start_line так, чтобы показывать историю
            if (total_lines > SCREEN_HEIGHT) {
                start_line = total_lines - SCREEN_HEIGHT;
            }

            log = fopen("log.txt", "a");
            if (log) {
                fprintf(log, "FORM FEED: total_lines=%d, start_line=%d\n", total_lines, start_line);
                fclose(log);
            }
            break;
        case '\n':
            y++;
            cursor_pos = y * SCREEN_WIDTH;
            break;

        case '\r':
            x = 0;
            cursor_pos = y * SCREEN_WIDTH;
            break;

        default:
            // Обычный символ
            if (y < TOTAL_HEIGHT && x < SCREEN_WIDTH) {
                screen[y][x].ch = c;
                screen[y][x].text_color = text_color;
                screen[y][x].bg_color = bg_color;

                cursor_pos++;
            }
            break;
    }

    // Обновляем x и y после всех изменений cursor_pos
    x = cursor_pos % SCREEN_WIDTH;
    y = cursor_pos / SCREEN_WIDTH;

    if (y >= total_lines) {
        total_lines = y + 1;

        FILE *log = fopen("log.txt", "a");
        if (log) {
            fprintf(log, "UPD total_lines: %d -> %d (y=%d)\n", total_lines - 1, total_lines, y);
            fclose(log);
        }
    }

    // Автоматический скроллинг при заполнении экрана
    if (total_lines > SCREEN_HEIGHT) {
        start_line = total_lines - SCREEN_HEIGHT;

        FILE *log = fopen("log.txt", "a");
        if (log) {
            fprintf(log, "AUTO SCROLL: start_line -> %d (total_lines=%d)\n",
                    start_line, total_lines);
            fclose(log);
        }
    }

    // Проверка переполнения буфера
    if (y >= TOTAL_HEIGHT) {
        // сдвигаем всё вверх
        for (int i = 0; i < TOTAL_HEIGHT - 1; i++) {
            memcpy(screen[i], screen[i + 1], sizeof(screen_cell) * SCREEN_WIDTH);
        }
        // Очищаем последнюю строку
        for (int j = 0; j < SCREEN_WIDTH; j++) {
            screen[TOTAL_HEIGHT - 1][j].ch = ' ';
            screen[TOTAL_HEIGHT - 1][j].text_color = text_color;
            screen[TOTAL_HEIGHT - 1][j].bg_color = bg_color;
        }

        // Корректируем позиции
        y = TOTAL_HEIGHT - 1;
        cursor_pos = y * SCREEN_WIDTH + x;
        total_lines = TOTAL_HEIGHT;

        // Скроллинг
        if (start_line > 0) {
            start_line = total_lines - SCREEN_HEIGHT;
        }
    }
    // Записываем конечное состояние
    log = fopen("log.txt", "a");
    if (log) {
        fprintf(log, "END:   cursor_pos=%d, x=%d, y=%d, total_lines=%d, start_line=%d\n\n",
                cursor_pos, x, y, total_lines, start_line);
        fclose(log);
    }

}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <bar2.bin>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("Failed to open bar2.bin");
        return 1;
    }

    void* bar2_map = mmap(NULL, BAR2_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (bar2_map == MAP_FAILED) {
        perror("Failed to mmap bar2.bin");
        close(fd);
        return 1;
    }

    // Проверить, что файл достаточно большой
    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    if (file_size < BAR2_SIZE) {
        printf("Error: bar2.bin is too small (%ld bytes), need %d bytes\n",
               file_size, BAR2_SIZE);
        close(fd);
        return 1;
    }

    uint8_t* bytes = (uint8_t*)bar2_map;

    int interrupt_enabled = 0;

    // Инициализация регистров
    bytes[REG_OUTPUT] = 0;
    *((uint32_t*)(bytes + REG_CURSOR_POS)) = 0;
    *((uint32_t*)(bytes + REG_TEXT_COLOR)) = 7;
    *((uint32_t*)(bytes + REG_BG_COLOR)) = 0;
    bytes[CLEAR_REQUEST_FLAG] = 0;

    // Установка неблокирующего терминала
    set_nonblocking_terminal();
    atexit(restore_terminal);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("VGA emulator started. Use arrow keys to scroll, 'q' to quit.\n");
    fflush(stdout);

    clear_screen();

    // printf("\033[2J"); // \033[2J - Очистка терминала \033[H - курсор в домашнюю позицию

    // Инициализируем пустой экран
    display_screen();

    printf("VGA emulator started. Waiting for data...\n");

    int needs_redisplay = 0;
    int frame_count = 0;

    // Очищаем логи при старте
    FILE *log = fopen("keylog.txt", "w");
    if (log) {
        fprintf(log, "=== VGA EMULATOR STARTED ===\n");
        fprintf(log, "start_line=%d, total_lines=%d\n", start_line, total_lines);
        fclose(log);
    }
    while (1) {
        needs_redisplay = 0;
         frame_count++;

        // Обработка нажатий клавиш
        int key_result = handle_keypress();
        if (key_result == -1) {
            break; // Выход по 'q'
        } else if (key_result == 1) {
            needs_redisplay = 1;
            // Логируем что нужно перерисовать
            // FILE *log = fopen("keylog.txt", "a");
            // if (log) {
            //     fprintf(log, "FRAME %d: NEEDS REDISPLAY (key press)\n", frame_count);
            //     fclose(log);
            // }
        }

        // Проверяем флаг очистки
        if (bytes[CLEAR_REQUEST_FLAG] == 1) {
            clear_screen();
            bytes[CLEAR_REQUEST_FLAG] = 0;  // Сбрасываем флаг
            needs_redisplay = 1;
        }

        interrupt_enabled = *((uint32_t*)(bytes + REG_INTERRUPT_ENABLE)) & 1;

        // Читаем символ из регистров
        uint8_t current_char = bytes[REG_OUTPUT];
        uint32_t current_cursor = *((uint32_t*)(bytes + REG_CURSOR_POS));
        uint32_t current_text_color = *((uint32_t*)(bytes + REG_TEXT_COLOR));
        uint32_t current_bg_color = *((uint32_t*)(bytes + REG_BG_COLOR));

        // Обновляем цвета если изменились
        if (current_text_color != text_color || current_bg_color != bg_color) {
            text_color = current_text_color & 0xF;
            bg_color = current_bg_color & 0x7;
            needs_redisplay = 1;
        }

        // Обрабатываем новый символ
        if (current_char != 0) {
            process_character(current_char);
            bytes[REG_OUTPUT] = 0;  // Сбрасываем регистр

            // Устанавливаем флаг прерывания если включено
            if (interrupt_enabled) {
                *((uint32_t*)(bytes + REG_INTERRUPT_ENABLE)) = 1;
            }

            needs_redisplay = 1;
        }

        if (*((uint32_t*)(bytes + REG_INTERRUPT_ENABLE)) == 1) {
            *((uint32_t*)(bytes + REG_INTERRUPT_ENABLE)) = 0;
        }

        // Обновляем позицию курсора
        if (current_cursor != cursor_pos) {
            cursor_pos = current_cursor;
            if (cursor_pos >= TOTAL_HEIGHT * SCREEN_WIDTH) {
                cursor_pos = (TOTAL_HEIGHT - 1) * SCREEN_WIDTH + (cursor_pos % SCREEN_WIDTH);
            }
            needs_redisplay = 1;
        }

        // Если нашли изменения, обновляем экран
        if (needs_redisplay != 0) {
            // FILE *log = fopen("keylog.txt", "a");
            // if (log) {
            //     fprintf(log, "FRAME %d: CALLING DISPLAY_SCREEN\n", frame_count);
            //     fclose(log);
            // }
            display_screen();
        }

        usleep(1); // 1ms задержка
    }

    munmap(bar2_map, BAR2_SIZE); // - освобождение виртуальной памяти mmap
    close(fd);
    return 0;
}
