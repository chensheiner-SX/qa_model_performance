from collections import namedtuple
import numpy as np
import cv2
import csv
import sys

Detection = namedtuple("Detection", ["image_path", "gt", "pred"])


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
    path = r'/home/lior.lakay/PycharmProjects/IOU/81.png'
    # boxes = get_box_from_csv("detections.csv")
    # iou = bb_intersection_over_union(boxes[0], [325.59185005856153, 232.2080901381139, 366.1758023033221, 253.68783578523653])
    # print(iou)
    Detection("81.png", [39, 63, 203, 112], [54, 66, 198, 114])
    image = cv2.imread(path)
    cv2.rectangle(image, (651, 392), (712, 426), (0, 255, 0), 2)
    cv2.rectangle(image, (664, 401), (714, 418), (0, 0, 255), 2)
    iou = bb_intersection_over_union([651, 392, 712, 426], [664, 401, 714, 418])
    cv2.putText(image, "IoU: {:.4f}".format(iou), (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
    print("{}: {:.4f}".format(Detection.image_path, iou))

    cv2.imshow("Image", image)
    cv2.waitKey(0)


if __name__ == "__main__":
    main()
