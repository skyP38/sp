#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

// IOCTL команды 
#define VGATEXT_IOCTL_BASE     0x56
#define VGATEXT_GET_CURSOR     _IOR(VGATEXT_IOCTL_BASE, 0, int)
#define VGATEXT_SET_CURSOR     _IOW(VGATEXT_IOCTL_BASE, 1, int)
#define VGATEXT_GET_TEXT_COLOR _IOR(VGATEXT_IOCTL_BASE, 2, int)
#define VGATEXT_SET_TEXT_COLOR _IOW(VGATEXT_IOCTL_BASE, 3, int)
#define VGATEXT_GET_BG_COLOR   _IOR(VGATEXT_IOCTL_BASE, 4, int)
#define VGATEXT_SET_BG_COLOR   _IOW(VGATEXT_IOCTL_BASE, 5, int)
#define VGATEXT_CLEAR_SCREEN   _IO(VGATEXT_IOCTL_BASE, 6)

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define SCREEN_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)

void print_usage(const char *program_name) {
    printf("Usage: %s <command> [args...]\n", program_name);
    printf("Commands:\n");
    printf("  clear                    - Clear the screen\n");
    printf("  set_cursor <pos>         - Set cursor position (0-%d)\n", SCREEN_SIZE - 1);
    printf("  get_cursor               - Get current cursor position\n");
    printf("  set_text_color <color>   - Set text color (0-15)\n");
    printf("  get_text_color           - Get current text color\n");
    printf("  set_bg_color <color>     - Set background color (0-15)\n");
    printf("  get_bg_color             - Get current background color\n");
    printf("  write <text>             - Write text to screen\n");
    printf("  demo                     - Run demonstration\n");
}

int main(int argc, char *argv[]) {
    int fd;
    int value;
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Открываем устройство
    fd = open("/dev/vgatext", O_RDWR);
    if (fd < 0) {
        fprintf(stderr,"Failed to open /dev/vgatext");
        return 1;
    }
    
    // Обрабатываем команды
    if (strcmp(argv[1], "clear") == 0) {
        if (ioctl(fd, VGATEXT_CLEAR_SCREEN) < 0) {
            fprintf(stderr,"ioctl clear screen failed");
        } else {
            printf("Screen cleared\n");
        }
    }
    else if (strcmp(argv[1], "set_cursor") == 0) {
        if (argc != 3) {
            printf("Usage: %s set_cursor <position>\n", argv[0]);
            close(fd);
            return 1;
        }
        
        value = atoi(argv[2]);
        if (value < 0 || value >= SCREEN_SIZE) {
            printf("Cursor position must be between 0 and %d\n", SCREEN_SIZE - 1);
            close(fd);
            return 1;
        }
        
        if (ioctl(fd, VGATEXT_SET_CURSOR, &value) < 0) {
            fprintf(stderr,"ioctl set cursor failed");
        } else {
            printf("Cursor set to position %d\n", value);
        }
    }
    else if (strcmp(argv[1], "get_cursor") == 0) {
        if (ioctl(fd, VGATEXT_GET_CURSOR, &value) < 0) {
            fprintf(stderr,"ioctl get cursor failed");
        } else {
            printf("Current cursor position: %d\n", value);
        }
    }
    else if (strcmp(argv[1], "set_text_color") == 0) {
        if (argc != 3) {
            printf("Usage: %s set_text_color <color>\n", argv[0]);
            close(fd);
            return 1;
        }
        
        value = atoi(argv[2]);
        if (value < 0 || value > 15) {
            printf("Text color must be between 0 and 15\n");
            close(fd);
            return 1;
        }
        
        if (ioctl(fd, VGATEXT_SET_TEXT_COLOR, &value) < 0) {
            fprintf(stderr,"ioctl set text color failed");
        } else {
            printf("Text color set to %d\n", value);
        }
    }
    else if (strcmp(argv[1], "get_text_color") == 0) {
        if (ioctl(fd, VGATEXT_GET_TEXT_COLOR, &value) < 0) {
            fprintf(stderr,"ioctl get text color failed");
        } else {
            printf("Current text color: %d\n", value);
        }
    }
    else if (strcmp(argv[1], "set_bg_color") == 0) {
        if (argc != 3) {
            printf("Usage: %s set_bg_color <color>\n", argv[0]);
            close(fd);
            return 1;
        }
        
        value = atoi(argv[2]);
        if (value < 0 || value > 15) {
            printf("Background color must be between 0 and 15\n");
            close(fd);
            return 1;
        }
        
        if (ioctl(fd, VGATEXT_SET_BG_COLOR, &value) < 0) {
            fprintf(stderr,"ioctl set background color failed");
        } else {
            printf("Background color set to %d\n", value);
        }
    }
    else if (strcmp(argv[1], "get_bg_color") == 0) {
        if (ioctl(fd, VGATEXT_GET_BG_COLOR, &value) < 0) {
            fprintf(stderr,"ioctl get background color failed");
        } else {
            printf("Current background color: %d\n", value);
        }
    }
    else if (strcmp(argv[1], "write") == 0) {
        if (argc < 3) {
            printf("Usage: %s write <text>\n", argv[0]);
            close(fd);
            return 1;
        }
        
        // Объединяем все аргументы в одну строку
        char text[1024] = {0};
        for (int i = 2; i < argc; i++) {
            strcat(text, argv[i]);
            if (i < argc - 1) strcat(text, " ");
        }
        
        if (write(fd, text, strlen(text)) < 0) {
            fprintf(stderr,"write failed");
        } else {
            printf("Text written: %s\n", text);
        }
    }
    else if (strcmp(argv[1], "demo") == 0) {
        printf("Running VGA Text Driver Demo...\n");
        
        // Очистка экрана
        ioctl(fd, VGATEXT_CLEAR_SCREEN);
        sleep(1);
        
        // Запись текста с разными цветами
        write(fd, "VGA Text Driver Demo\n", 21);
        
        // Изменение цвета текста
        value = 2; // Зеленый
        ioctl(fd, VGATEXT_SET_TEXT_COLOR, &value);
        write(fd, "Green text\n", 11);
        
        value = 4; // Красный
        ioctl(fd, VGATEXT_SET_TEXT_COLOR, &value);
        write(fd, "Red text\n", 9);
        
        value = 1; // Синий фон
        ioctl(fd, VGATEXT_SET_BG_COLOR, &value);
        value = 7; // Белый текст
        ioctl(fd, VGATEXT_SET_TEXT_COLOR, &value);
        write(fd, "White text on blue background\n", 30);
        
        // Демонстрация скроллинга
        value = 0; // Черный фон
        ioctl(fd, VGATEXT_SET_BG_COLOR, &value);
        value = 3; // Голубой текст
        ioctl(fd, VGATEXT_SET_TEXT_COLOR, &value);
        
        for (int i = 1; i <= 30; i++) {
            char line[50];
            snprintf(line, sizeof(line), "Line %d: This is a test line for scrolling demonstration.\n", i);
            write(fd, line, strlen(line));
            usleep(100000); // 100ms задержка
        }
        
        printf("Demo completed\n");
    }
    else {
        printf("Unknown command: %s\n", argv[1]);
        print_usage(argv[0]);
    }
    
    close(fd);
    return 0;
}