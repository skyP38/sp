# Компилятор и флаги
CC = gcc
CFLAGS = -Wall -g `pkg-config --cflags gimp-3.0 gimpui-3.0 gtk4 gegl-0.4`
LIBS = `pkg-config --libs gimp-3.0 gimpui-3.0 gtk4 gegl-0.4` -lm

# Имя исполняемого файла
TARGET = watermark

# Директория установки плагина
PLUGIN_DIR = ~/.config/GIMP/3.0/plug-ins/watermark

# Исходные файлы
SRCS = watermark.c ui.c watermark_logic.c ui_pages.c image.c text.c mosaic.c pixel.c

# Объектные файлы
OBJS = $(SRCS:.c=.o)

# Заголовочные файлы
HEADERS = ui.h watermark_logic.h ui_pages.h image.h text.h mosaic.h pixel.h

# Правило по умолчанию
all: $(TARGET)

# Сборка исполняемого файла
$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)

# Компиляция объектных файлов
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Установка плагина
install: $(TARGET)
	mkdir -p $(PLUGIN_DIR)
	cp $(TARGET) $(PLUGIN_DIR)/
	chmod +x $(PLUGIN_DIR)/$(TARGET)
	@echo "Plugin installed to $(PLUGIN_DIR)"

# Очистка
clean:
	rm -f $(OBJS) $(TARGET)

# Пересборка
rebuild: clean all

# Переустановка
reinstall: clean all install

# Справка
help:
	@echo "Available targets:"
	@echo "  all       - Build the plugin (default)"
	@echo "  install   - Build and install to GIMP plugins directory"
	@echo "  clean     - Remove object files and executable"
	@echo "  rebuild   - Clean and rebuild"
	@echo "  reinstall - Clean, rebuild and reinstall"
	@echo "  help      - Show this help"

.PHONY: all install clean rebuild reinstall help