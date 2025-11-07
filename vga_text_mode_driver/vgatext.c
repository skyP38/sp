#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/sched/signal.h>
#include <linux/interrupt.h>

#define VGATEXT_DRIVER "vgatext_driver"

#define VGATEXT_VENDOR_ID   0x1B36  // QEMU's vendor ID
#define VGATEXT_PRODUCT_ID  0x0005  // pci-testdev device ID

#define VGATEXT_BAR_NUM     2
#define VGATEXT_BAR_MASK    (1 << VGATEXT_BAR_NUM)

// Регистры VGA Text Mode
#define REG_OUTPUT          0x00  // RW Регистр вывода
#define REG_CURSOR_POS      0x04  // RW Текущая координата курсора
#define REG_TEXT_COLOR      0x08  // RW Текущий цвет текста
#define REG_BG_COLOR        0x0C  // RW Текущий цвет фона
#define REG_STATUS          0x10  // RO Регистр статуса

#define CLEAR_REQUEST_FLAG     0x0F01  // Флаг "запрос очистки"

// Размеры экрана
#define SCREEN_WIDTH        80
#define SCREEN_HEIGHT       25
#define SCREEN_SIZE         (SCREEN_WIDTH * SCREEN_HEIGHT)

// IOCTL команды 
#define VGATEXT_IOCTL_BASE     0x56 // 
#define VGATEXT_GET_CURSOR     _IOR(VGATEXT_IOCTL_BASE, 0, int)
#define VGATEXT_SET_CURSOR     _IOW(VGATEXT_IOCTL_BASE, 1, int)
#define VGATEXT_GET_TEXT_COLOR _IOR(VGATEXT_IOCTL_BASE, 2, int)
#define VGATEXT_SET_TEXT_COLOR _IOW(VGATEXT_IOCTL_BASE, 3, int)
#define VGATEXT_GET_BG_COLOR   _IOR(VGATEXT_IOCTL_BASE, 4, int)
#define VGATEXT_SET_BG_COLOR   _IOW(VGATEXT_IOCTL_BASE, 5, int)
#define VGATEXT_CLEAR_SCREEN   _IO(VGATEXT_IOCTL_BASE, 6)

#define REG_INTERRUPT_ENABLE  0x14  // RW Включение прерываний

static struct pci_device_id vgatext_id_table[] = {
    { PCI_DEVICE(VGATEXT_VENDOR_ID, VGATEXT_PRODUCT_ID) },
    { 0, }
};

MODULE_DEVICE_TABLE(pci, vgatext_id_table);

static int vgatext_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void vgatext_remove(struct pci_dev *pdev);

static struct pci_driver vgatext = {
    .name       = VGATEXT_DRIVER,
    .id_table   = vgatext_id_table,
    .probe      = vgatext_probe,
    .remove     = vgatext_remove
};

// структура устройства
struct vgatext_data {
    u8 __iomem      *hwmem;
    char            *screen_buffer;
    int             cursor_pos;
    int             text_color;
    int             bg_color;
    struct device   *vgatext_dev;
    struct cdev     cdev;
};

static inline int vgatext_char_ready(struct vgatext_data* drv) {
    return (ioread8(drv->hwmem + REG_OUTPUT) == 0);
}

// функция ожидания готовности
static int vgatext_wait_ready(struct vgatext_data* drv) {
    int timeout = 10000; // 1000 попыток
    
    while (timeout-- > 0) {
        if (vgatext_char_ready(drv)) {
            return 0;
        }
        udelay(1);
    }
    printk(KERN_DEBUG "vgatext: device timeout, continuing\n");
    return -ETIMEDOUT;
}

static __poll_t vgatext_poll(struct file *file, poll_table *wait) {
    struct vgatext_data* drv = file->private_data;
    __poll_t mask = 0;
    
    if (vgatext_char_ready(drv)) {
        mask |= EPOLLOUT | EPOLLWRNORM;
    }
    
    return mask;
}

static int create_char_devs(struct vgatext_data* drv);
static int destroy_char_devs(void);

// определение функций файловых операций
static int vgatext_open(struct inode *inode, struct file *file);
static int vgatext_release(struct inode *inode, struct file *file);
static long vgatext_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t vgatext_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t vgatext_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);

// настраиваем структуру file_operations
static const struct file_operations vgatext_fops = {
    .owner          = THIS_MODULE,
    .open           = vgatext_open,
    .release        = vgatext_release,
    .unlocked_ioctl = vgatext_ioctl,
    .read           = vgatext_read,
    .write          = vgatext_write,
    .poll           = vgatext_poll,
};

static int dev_major = 0;
static struct class *vgatextclass = NULL;
static struct vgatext_data vgatext_data_private;

// Функции sysfs для курсора
static ssize_t cursor_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", vgatext_data_private.cursor_pos);
}

static ssize_t cursor_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    int new_value;

    if (kstrtoint(buf, 10, &new_value) != 0)
        return -EINVAL; // некорректный аргумент

    if (new_value >= 0 && new_value < SCREEN_SIZE) {
        vgatext_data_private.cursor_pos = new_value;
        if (vgatext_data_private.hwmem) {
            iowrite32(new_value, vgatext_data_private.hwmem + REG_CURSOR_POS);
        }
    }
    return count;
}

static DEVICE_ATTR(cursor_pos, 0644, cursor_show, cursor_store);

// Функции sysfs для цвета текста
static ssize_t text_color_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", vgatext_data_private.text_color);
}

static ssize_t text_color_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    int new_value;

    if (kstrtoint(buf, 10, &new_value) != 0)
        return -EINVAL;

    vgatext_data_private.text_color = new_value;
    if (vgatext_data_private.hwmem) {
        iowrite32(new_value, vgatext_data_private.hwmem + REG_TEXT_COLOR);
    }
    return count;
}

static DEVICE_ATTR(text_color, 0644, text_color_show, text_color_store);

// Функции sysfs для цвета фона
static ssize_t bg_color_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", vgatext_data_private.bg_color);
}

static ssize_t bg_color_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    int new_value;

    if (kstrtoint(buf, 10, &new_value) != 0)
        return -EINVAL;

    vgatext_data_private.bg_color = new_value;
    if (vgatext_data_private.hwmem) {
        iowrite32(new_value, vgatext_data_private.hwmem + REG_BG_COLOR);
    }
    return count;
}

static DEVICE_ATTR(bg_color, 0644, bg_color_show, bg_color_store);

// Добавить функцию очистки для sysfs
static ssize_t clear_screen_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    int value;
    
    if (kstrtoint(buf, 10, &value) != 0 || value != 1)
        return -EINVAL;

    printk(KERN_INFO "vgatext: starting screen clear procedure via sysfs\n");

    struct vgatext_data* drv = &vgatext_data_private;
    printk(KERN_INFO "vgatext: hwmem = %p\n", drv->hwmem);
    // Очистка экрана - зануляем весь BAR2
    if (drv->hwmem) {
        iowrite8(1, drv->hwmem + CLEAR_REQUEST_FLAG);
            
        // Сбрасываем регистры
        iowrite32(0, drv->hwmem + REG_CURSOR_POS);
        iowrite32(7, drv->hwmem + REG_TEXT_COLOR);
        iowrite32(0, drv->hwmem + REG_BG_COLOR);
        iowrite8(0, drv->hwmem + REG_OUTPUT);

        printk(KERN_INFO "vgatext: BAR2 cleared and registers reset\n");
    } 

    if (drv->screen_buffer) {
        memset(drv->screen_buffer, ' ', SCREEN_SIZE);
        printk(KERN_INFO "vgatext: internal buffer cleared\n");
    }
    drv->cursor_pos = 0;
    drv->text_color = 7;
    drv->bg_color = 0;

    printk(KERN_INFO "vgatext: screen cleared via sysfs\n");
    
    return count;
}

static DEVICE_ATTR(clear_screen, 0220, NULL, clear_screen_store);

static int create_char_devs(struct vgatext_data *drv) {
    int err;
    dev_t dev;

    err = alloc_chrdev_region(&dev, 0, 1, "vgatext");
    if (err < 0) {
        printk(KERN_ERR "Failed to allocate char device region\n");
        return err;
    }

    dev_major = MAJOR(dev);

    // регистрируем sysfs класс под названием vgatext
    vgatextclass = class_create(THIS_MODULE, "vgatext");
    if (IS_ERR(vgatextclass)) {
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(vgatextclass);
    }

    cdev_init(&vgatext_data_private.cdev, &vgatext_fops);
    vgatext_data_private.cdev.owner = THIS_MODULE;
    
    err = cdev_add(&vgatext_data_private.cdev, MKDEV(dev_major, 0), 1);
    if (err) {
        class_destroy(vgatextclass);
        unregister_chrdev_region(dev, 1);
        return err;
    }

    vgatext_data_private.vgatext_dev = device_create(vgatextclass, NULL, MKDEV(dev_major, 0), NULL, "vgatext");
    if (IS_ERR(vgatext_data_private.vgatext_dev)) {
        cdev_del(&vgatext_data_private.cdev);
        class_destroy(vgatextclass);
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(vgatext_data_private.vgatext_dev);
    }

    dev_set_drvdata(vgatext_data_private.vgatext_dev, &vgatext_data_private);

    // Создаем sysfs атрибуты
    err = device_create_file(vgatext_data_private.vgatext_dev, &dev_attr_cursor_pos);
    if (err < 0)
        printk(KERN_ERR "Failed to create cursor_pos sysfs attribute\n");

    err = device_create_file(vgatext_data_private.vgatext_dev, &dev_attr_text_color);
    if (err < 0)
        printk(KERN_ERR "Failed to create text_color sysfs attribute\n");

    err = device_create_file(vgatext_data_private.vgatext_dev, &dev_attr_bg_color);
    if (err < 0)
        printk(KERN_ERR "Failed to create bg_color sysfs attribute\n");

    err = device_create_file(vgatext_data_private.vgatext_dev, &dev_attr_clear_screen);
    if (err < 0)
        printk(KERN_ERR "Failed to create clear_screen sysfs attribute\n");

    printk(KERN_INFO "vgatext: char device created successfully\n");

    return 0;
}

static int vgatext_open(struct inode *inode, struct file *file) {
    // копируем (с выделением памяти) структуру  testdev_data, которая была
	// инициализирована ранее в коде, работающим с PCI

    // struct vgatext_data *dev_priv = kmemdup(&vgatext_data_private, sizeof(struct vgatext_data), GFP_KERNEL);
    // if (!dev_priv)
    //     return -ENOMEM; // недостаточно памяти

    // // Выделяем собственный буфер для этого файлового дескриптора
    // dev_priv->screen_buffer = vmalloc(SCREEN_SIZE);
    // if (!dev_priv->screen_buffer) {
    //     kfree(dev_priv);
    //     return -ENOMEM;
    // }
    
    // // Инициализируем буфер пробелами
    // memset(dev_priv->screen_buffer, ' ', SCREEN_SIZE);

    // dev_priv->cursor_pos = vgatext_data_private.cursor_pos;
    // dev_priv->text_color = vgatext_data_private.text_color;
    // dev_priv->bg_color = vgatext_data_private.bg_color;

    // сохраняем указатель на структуру как приватные данные открытого файла
	// теперь эта структура будет доступна внутри всех операций, выполняемых над этим файлом
    // file->private_data = dev_priv;

    file->private_data = &vgatext_data_private;
    return 0;
}

static ssize_t vgatext_read(struct file *file, char __user *buf, size_t count, loff_t *offset) {
    // При чтении драйвер ничего не выводит, но завершает работу без ошибки
    return 0;
}

static ssize_t vgatext_write(struct file *file, const char __user *buf, size_t count, loff_t *offset) {
    struct vgatext_data* drv = file->private_data;
    char *text_buf;
    int i;
    ssize_t ret = count;

    if (!drv->hwmem) {
        printk(KERN_ERR "No BAR2 memory mapped\n");
        return -ENODEV;
    }

    drv->cursor_pos = vgatext_data_private.cursor_pos;

    text_buf = kmalloc(count + 1, GFP_KERNEL);
    if (!text_buf)
        return -ENOMEM;

    if (copy_from_user(text_buf, buf, count)) {
        kfree(text_buf);
        return -EFAULT; // некорректный адресс
    }
    text_buf[count] = '\0';

    printk(KERN_INFO "vgatext: writing %zu bytes, cursor starts at %d\n", count, drv->cursor_pos);

    iowrite32(drv->cursor_pos, drv->hwmem + REG_CURSOR_POS);
    iowrite32(drv->text_color, drv->hwmem + REG_TEXT_COLOR);
    iowrite32(drv->bg_color, drv->hwmem + REG_BG_COLOR);
    vgatext_wait_ready(drv);

    // обработка текста посимвольно
    for (i = 0; i < count; i++) {
        char c = text_buf[i];

        if (vgatext_wait_ready(drv) < 0) {
            printk(KERN_WARNING "vgatext: device timeout, continuing\n");
        }
        
        if (c == '\n') {
            iowrite8(c, drv->hwmem + REG_OUTPUT);

            // Перенос строки - перемещаем курсор на начало следующей строки
            int current_line = drv->cursor_pos / SCREEN_WIDTH;
            drv->cursor_pos = (current_line + 1) * SCREEN_WIDTH;

            // Отправляем команду скроллинга только если достигли конца экрана
            if (current_line + 1 >= SCREEN_HEIGHT) {
                vgatext_wait_ready(drv);
                iowrite8(0x0C, drv->hwmem + REG_OUTPUT); // Form Feed для скроллинга
                vgatext_wait_ready(drv);
                drv->cursor_pos = (SCREEN_HEIGHT - 1) * SCREEN_WIDTH;
            } 
            printk(KERN_INFO "vgatext: newline, cursor now at %d\n", drv->cursor_pos);
        } else if (c == '\r') {
            // Возврат каретки - в начало текущей строки
            int current_line = drv->cursor_pos / SCREEN_WIDTH;
            drv->cursor_pos = current_line * SCREEN_WIDTH;
            printk(KERN_INFO "vgatext: carriage return, cursor now at %d\n", drv->cursor_pos);
        } else {   
            iowrite8(c, drv->hwmem + REG_OUTPUT);

            // Сохраняем символ в буфере экрана
            if (drv->cursor_pos < SCREEN_SIZE) {
                drv->screen_buffer[drv->cursor_pos] = c;
            }
            
            printk(KERN_INFO "vgatext: wrote '%c' at position %d\n", c, drv->cursor_pos);
            
            // Обновляем позицию курсора
            drv->cursor_pos++;

            // Перенос на следующую строку если достигли конца строки
            if ((drv->cursor_pos % SCREEN_WIDTH) == 0) {
                int current_line = drv->cursor_pos / SCREEN_WIDTH;
                if (current_line >= SCREEN_HEIGHT) {
                    iowrite8(0x0C, drv->hwmem + REG_OUTPUT);
                    drv->cursor_pos = (SCREEN_HEIGHT - 1) * SCREEN_WIDTH;
                }
            }

            if (drv->cursor_pos >= SCREEN_SIZE) {
                drv->cursor_pos = SCREEN_SIZE - 1;
            }
        }

        iowrite32(drv->cursor_pos, drv->hwmem + REG_CURSOR_POS);
        // udelay(1);
    }
    vgatext_data_private.cursor_pos = drv->cursor_pos;

    printk(KERN_INFO "vgatext: write completed, cursor ends at %d\n", drv->cursor_pos);

    kfree(text_buf);

    return ret;
}

static long vgatext_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct vgatext_data* drv = file->private_data;
    int value;

    switch (cmd) {
        case VGATEXT_GET_CURSOR:
            value = drv->cursor_pos;
            if (copy_to_user((int __user *)arg, &value, sizeof(value)))
                return -EFAULT;
            break;
            
        case VGATEXT_SET_CURSOR:
            if (copy_from_user(&value, (int __user *)arg, sizeof(value)))
                return -EFAULT;
            if (value >= 0 && value < SCREEN_SIZE) {
                drv->cursor_pos = value;
                if (drv->hwmem) {
                    iowrite32(value, drv->hwmem + REG_CURSOR_POS);
                }
            }
            break;
            
        case VGATEXT_GET_TEXT_COLOR:
            value = drv->text_color;
            if (copy_to_user((int __user *)arg, &value, sizeof(value)))
                return -EFAULT;
            break;
            
        case VGATEXT_SET_TEXT_COLOR:
            if (copy_from_user(&value, (int __user *)arg, sizeof(value)))
                return -EFAULT;
            drv->text_color = value;
            if (drv->hwmem) {
                iowrite32(value, drv->hwmem + REG_TEXT_COLOR);
            }
            break;
            
        case VGATEXT_GET_BG_COLOR:
            value = drv->bg_color;
            if (copy_to_user((int __user *)arg, &value, sizeof(value)))
                return -EFAULT;
            break;
            
        case VGATEXT_SET_BG_COLOR:
            if (copy_from_user(&value, (int __user *)arg, sizeof(value)))
                return -EFAULT;
            drv->bg_color = value;
            if (drv->hwmem) {
                iowrite32(value, drv->hwmem + REG_BG_COLOR);
            }
            break;
            
        case VGATEXT_CLEAR_SCREEN:
            // Очистка экрана - зануляем весь BAR2
            if (drv->hwmem) {
                iowrite8(1, drv->hwmem + CLEAR_REQUEST_FLAG);

                // Сбрасываем регистры
                iowrite32(0, drv->hwmem + REG_CURSOR_POS);
                iowrite32(7, drv->hwmem + REG_TEXT_COLOR);
                iowrite32(0, drv->hwmem + REG_BG_COLOR);
                iowrite8(0, drv->hwmem + REG_OUTPUT);

                printk(KERN_INFO "vgatext: BAR2 cleared and registers reset\n");
            } 

            if (drv->screen_buffer) {
                memset(drv->screen_buffer, ' ', SCREEN_SIZE);
                printk(KERN_INFO "vgatext: internal buffer cleared\n");
            }
            drv->cursor_pos = 0;
            drv->text_color = 7;
            drv->bg_color = 0;

             // Синхронизируем с глобальной структурой
            vgatext_data_private.cursor_pos = drv->cursor_pos;
            vgatext_data_private.text_color = drv->text_color;
            vgatext_data_private.bg_color = drv->bg_color;
            break;
            
        default:
            return -ENOTTY; // неподходящий ioctl
    }
    
    return 0;
}

static int vgatext_release(struct inode *inode, struct file *file) {
    // struct vgatext_data* priv = file->private_data;
    // kfree(priv);
    // priv = NULL;
    file->private_data = NULL;
    return 0;
}

static int destroy_char_devs(void) {
    // Удаляем sysfs атрибуты
    if (vgatext_data_private.vgatext_dev) {
        device_remove_file(vgatext_data_private.vgatext_dev, &dev_attr_cursor_pos);
        device_remove_file(vgatext_data_private.vgatext_dev, &dev_attr_text_color);
        device_remove_file(vgatext_data_private.vgatext_dev, &dev_attr_bg_color);
    }

    if (vgatextclass) {
        device_destroy(vgatextclass, MKDEV(dev_major, 0));
        class_unregister(vgatextclass);
        class_destroy(vgatextclass);
    }
    
    if (dev_major)
        unregister_chrdev_region(MKDEV(dev_major, 0), 1);

    return 0;
}

static int __init vgatext_driver_init(void) {
    return pci_register_driver(&vgatext);
}

static void __exit vgatext_driver_exit(void) {
    pci_unregister_driver(&vgatext);
}

static void release_device(struct pci_dev *pdev) {
    pci_release_region(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
    pci_disable_device(pdev);
}

static int vgatext_probe(struct pci_dev *pdev, const struct pci_device_id *ent) {
    int bar, err;
    u16 vendor, device;
    unsigned long mmio_start, mmio_len;
    struct vgatext_data *drv_data;

    pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor);
    pci_read_config_word(pdev, PCI_DEVICE_ID, &device);

	printk(KERN_INFO "Device vid: 0x%X  pid: 0x%X\n", vendor, device);
    
    bar = pci_select_bars(pdev, IORESOURCE_MEM);
    if (!(bar & VGATEXT_BAR_MASK)) {
        printk(KERN_ERR "No suitable BAR found\n");
        return -ENODEV; // устройство не существует
    }

    err = pci_enable_device_mem(pdev);
    if (err) {
        printk(KERN_ERR "Failed to enable device\n");
        return err;
    }

    err = pci_request_region(pdev, VGATEXT_BAR_NUM, VGATEXT_DRIVER);
    if (err) {
        printk(KERN_ERR "Failed to request region\n");
        pci_disable_device(pdev);
        return err;
    }

    mmio_start = pci_resource_start(pdev, VGATEXT_BAR_NUM);
    mmio_len = pci_resource_len(pdev, VGATEXT_BAR_NUM);

    drv_data = kzalloc(sizeof(struct vgatext_data), GFP_KERNEL);
    if (!drv_data) {
        printk(KERN_ERR "Failed to allocate driver data\n");
        release_device(pdev);
        return -ENOMEM;
    }

    drv_data->hwmem = ioremap(mmio_start, mmio_len);
    if (!drv_data->hwmem) {
        printk(KERN_ERR "Failed to ioremap BAR\n");
        kfree(drv_data);
        release_device(pdev);
        return -EIO;
    }

    printk(KERN_INFO "VGA Text Device mapped resource 0x%lx to 0x%p\n", mmio_start, drv_data->hwmem);

    // инициализация
    drv_data->cursor_pos = 0;
    drv_data->text_color = 7; 
    drv_data->bg_color = 0;   
    drv_data->screen_buffer = vmalloc(SCREEN_SIZE);
    if (!drv_data->screen_buffer) {
        printk(KERN_ERR "Failed to allocate screen buffer\n");
        iounmap(drv_data->hwmem);
        kfree(drv_data);
        release_device(pdev);
        return -ENOMEM;
    }
    memset(drv_data->screen_buffer, ' ', SCREEN_SIZE);

    iowrite32(drv_data->cursor_pos, drv_data->hwmem + REG_CURSOR_POS);
    iowrite32(drv_data->text_color, drv_data->hwmem + REG_TEXT_COLOR);
    iowrite32(drv_data->bg_color, drv_data->hwmem + REG_BG_COLOR);

    memcpy(&vgatext_data_private, drv_data, sizeof(struct vgatext_data));
    create_char_devs(drv_data);
    
    pci_set_drvdata(pdev, drv_data);

    printk(KERN_INFO "VGA Text Device mapped resource 0x%lx to 0x%p, size: 0x%lx\n", 
           mmio_start, drv_data->hwmem, mmio_len);

    // Проверьте что размер BAR2 достаточен
    if (mmio_len < SCREEN_SIZE + 16) {
        printk(KERN_ERR "vgatext: BAR2 too small! Have 0x%lx, need at least 0x%x\n", 
               mmio_len, SCREEN_SIZE + 16);
        iounmap(drv_data->hwmem);
        kfree(drv_data);
        release_device(pdev);
        return -ENOMEM;
    }
    
    // Включаем прерывания в устройстве
    iowrite32(1, drv_data->hwmem + REG_INTERRUPT_ENABLE);

    printk(KERN_INFO "vgatext: probe completed successfully\n");
    return 0;
}

static void vgatext_remove(struct pci_dev *pdev) {
    struct vgatext_data *drv_data = pci_get_drvdata(pdev);

    destroy_char_devs();

    if (drv_data) {
        if (drv_data->screen_buffer) {
            vfree(drv_data->screen_buffer);
            drv_data->screen_buffer = NULL;
        }
        if (drv_data->hwmem)
            iounmap(drv_data->hwmem);
        kfree(drv_data);
    }

    release_device(pdev);
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("OGO");
MODULE_DESCRIPTION("VGA Text Mode Driver for QEMU PCI-testdev device");
MODULE_VERSION("1.0");

module_init(vgatext_driver_init);
module_exit(vgatext_driver_exit);