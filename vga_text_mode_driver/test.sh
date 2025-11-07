echo 0 > /sys/class/vgatext/vgatext/cursor_pos

# 1. Базовый тест
echo "Hello VGA Text Driver!" > /dev/vgatext

# 2. Тест переноса строк
echo -e "Line 1\nLine 2\nLine 3" > /dev/vgatext

# 3. Тест длинной строки (автоматический перенос)
echo "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum" > /dev/vgatext

# 4. Тест цветов через sysfs
echo "Default colors" > /dev/vgatext
echo 4 > /sys/class/vgatext/vgatext/text_color  # Красный
echo "Red text" > /dev/vgatext
echo 2 > /sys/class/vgatext/vgatext/text_color  # Зеленый
echo 1 > /sys/class/vgatext/vgatext/bg_color    # Синий фон
echo "Green on blue" > /dev/vgatext
echo 1 > /sys/class/vgatext/vgatext/clear_screen

# 5. Тест скроллинга - генерируем много строк
for i in {1..30}; do
    echo "line $i" >/dev/vgatext
done

# 6. Проверка текущих значений
echo "Cursor: $(cat /sys/class/vgatext/vgatext/cursor_pos)"
echo "Text color: $(cat /sys/class/vgatext/vgatext/text_color)"
echo "BG color: $(cat /sys/class/vgatext/vgatext/bg_color)"

# # 7. Тест программы управления через ioctl
echo "Testing ioctl control program..."
./vga_control clear
./vga_control set_text_color 2
./vga_control set_bg_color 0
./vga_control write "This is text via ioctl control program"
./vga_control set_cursor 160
./vga_control write "Text at position 160"
./vga_control get_cursor
./vga_control get_text_color
./vga_control get_bg_color
