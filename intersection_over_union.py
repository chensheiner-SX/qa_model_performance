from collections import namedtuple
import numpy as np
import cv2
import csv


def get_box_from_csv(file):
    boxes = []
    with open(file, "r") as data_file:
        # BB = tuple()
        data = csv.reader(data_file)
        for index, row in enumerate(data):
            if index == 0:
                for index, item in enumerate(row):
                    if item == " X1":
                        x1y1 = (index, index + 1)
                    elif item == " X2":
                        x2y2 = (index, index + 1)
                continue

            boxes.append([float(row[x1y1[0]]), float(row[x1y1[1]]), float(row[x2y2[0]]), float(row[x2y2[1]])])
        return boxes
            # break


def bb_intersection_over_union(boxA, boxB):
    xA = max(boxA[0], boxB[0])
    yA = max(boxA[1], boxB[1])
    xB = max(boxA[2], boxB[2])
    yB = max(boxA[3], boxB[3])

    interArea = max(0, xB - xA + 1) * max(0, yB - yA + 1)

    boxA_area = (boxA[2] - boxA[0] + 1) * (boxA[3] - boxA[1] + 1)
    boxB_area = (boxB[2] - boxB[0] + 1) * (boxB[3] - boxB[1] + 1)

    iou = interArea / float(boxA_area + boxB_area - interArea)
    return iou


def main():
    boxes = get_box_from_csv("detections.csv")
    iou = bb_intersection_over_union(boxes[0], [325.59185005856153, 232.2080901381139, 366.1758023033221, 253.68783578523653])
    print(iou)


if __name__ == "__main__":
    main()