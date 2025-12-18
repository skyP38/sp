# Компилятор и флаги
CC = gcc
CFLAGS = -Wall -g `pkg-config --cflags gimp-3.0 gimpui-3.0 gtk4 gegl-0.4`
LIBS = `pkg-config --libs gimp-3.0 gimpui-3.0 gtk4 gegl-0.4` -lm

# Имя исполняемого файла
TARGET = stereobm

# Директория установки плагина
PLUGIN_DIR = ~/.config/GIMP/3.0/plug-ins/stereobm

# Исходные файлы
SRCS = stereobm_main.c stereobm_compute.c stereobm_plugin.c stereobm_dialog.c

# Объектные файлы
OBJS = $(SRCS:.c=.o)

# Правило по умолчанию
all: $(TARGET)

# Сборка исполняемого файла
$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

# Компиляция объектных файлов
%.o: %.c stereobm.h
	$(CC) $(CFLAGS) -c $< -o $@

# Установка плагина
install: $(TARGET)
	cp $(TARGET) $(PLUGIN_DIR)/

# Очистка
clean:
	rm -f $(OBJS) $(TARGET)

# Переустановка
reinstall: clean all install

.PHONY: all install clean reinstall