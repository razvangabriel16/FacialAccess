#!/usr/bin/python

import numpy as np 
import cv2
import sys
import ctypes

def main():
    arg = sys.argv[1]
    image_path = '/pi/home/project2/imagine_contour.pgm'
    img = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
    if img is None:
        print(f"Error: Could not read image from {image_path}")
        return None
    lib = ctypes.CDLL('./libinvert.so')
    height, width = img.shape
    img = np.ascontiguousarray(img, dtype=np.uint8)
    lib.invert_pixels.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_int, ctypes.c_int]
    lib.invert_pixels.restype = None
    lib.invert_pixels(img.ctypes.data_as(ctypes.POINTER(ctypes.c_uint8)), height, width)
    kernel = np.ones((3, 3), dtype=np.uint8)
    img = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
    eroded = cv2.erode(img, kernel)
    dilated = cv2.dilate(img, kernel)
    opened = cv2.morphologyEx(img, cv2.MORPH_OPEN, kernel)
    closed = cv2.morphologyEx(img, cv2.MORPH_CLOSE, kernel)

    num_labels, labels_img, stats, _ = cv2.connectedComponentsWithStats(img)
    areas = stats[1:, cv2.CC_STAT_AREA]
    largest_component_label = 1 + np.argmax(areas)
    largest_component_mask = (labels_img == largest_component_label).astype(np.uint8) * 255
    #not ready yet...

if __name__ == "__main__":
    main()
