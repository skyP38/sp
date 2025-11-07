#!/bin/bash

# Выгрузить драйвер
sudo rmmod vgatext

# Удалить устройство из системы
echo 1 | sudo tee /sys/bus/pci/devices/0000:00:03.0/remove
sleep 1

# Rescan PCI шину
echo 1 | sudo tee /sys/bus/pci/rescan
sleep 1

# Загрузить драйвер снова
sudo insmod vgatext.ko
