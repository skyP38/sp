import cv2
import numpy as np
import sys

def process_stereo_image(image_path):
    # Загрузка side-by-side изображения
    stereo_img = cv2.imread(image_path)
    if stereo_img is None:
        print(f"Ошибка: не удалось загрузить изображение {image_path}")
        return

    # Разделение на левую и правую части
    height, width = stereo_img.shape[:2]
    mid = width // 2

    left_img = stereo_img[:, :mid]
    right_img = stereo_img[:, mid:]

    # Конвертация в grayscale
    left_gray = cv2.cvtColor(left_img, cv2.COLOR_BGR2GRAY)
    right_gray = cv2.cvtColor(right_img, cv2.COLOR_BGR2GRAY)

    # Создание StereoBM объекта
    stereo = cv2.StereoBM_create(numDisparities=64, blockSize=15)
    stereo.setPreFilterType(1)
    stereo.setPreFilterCap(31)
    stereo.setPreFilterSize(5)
    stereo.setTextureThreshold(10)
    stereo.setUniquenessRatio(15)
    stereo.setSpeckleWindowSize(0)
    stereo.setSpeckleRange(0)

    # Вычисление карты диспаратности
    disparity = stereo.compute(left_gray, right_gray)

    # Нормализация для визуализации
    disparity_visual = cv2.normalize(disparity, None, 0, 255, cv2.NORM_MINMAX, cv2.CV_8U)

    # Применение цветовой карты
    disparity_color = cv2.applyColorMap(disparity_visual, cv2.COLORMAP_JET)
    cv2.imshow('Disparity Map', disparity_color)
    cv2.waitKey(0)

    print("Обработка завершена!")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Использование: python stereo_processing.py <path_to_stereo_image>")
        print("Пример: python stereo_processing.py stereo_image.jpg")
        sys.exit(1)

    image_path = sys.argv[1]
    process_stereo_image(image_path)




