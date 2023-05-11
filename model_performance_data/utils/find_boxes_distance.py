import math
import sys

"""
Script for finding the distance between
2 BB centers.
"""
def find_line_center(*args):
    x1 = float(args[0])
    y1 = float(args[1])
    x2 = float(args[2])
    y2 = float(args[3])

    return ((x1+x2) / 2, (y1+y2) / 2)


def find_squares_distance(boxA, boxB):
    # box = (x1, y1, x2, y2)
    boxA_center = find_line_center(boxA[1], boxA[2], boxA[3], boxA[4])  # center point (x, y)
    boxB_center = find_line_center(boxB[1], boxB[2], boxB[3], boxB[4])  # center point (x, y)

    return math.sqrt((boxA_center[0] - boxB_center[0])**2 + (boxA_center[1] - boxB_center[1])**2)


def get_closest_distance(boxA, boxes):
    """
    Function to find the closest distance between boxes
    params:
        * boxA:     box as tuple (x1,y1,x2,y2)
        * boxes:    list of boxes from where we want to find the closest one
    returns:
        * result:   the index in the list of the relevant box
    """

    result = 0
    current_min_distance = sys.float_info.max

    for index, box in enumerate(boxes):
        distance = find_squares_distance(boxA, box)
        if distance < current_min_distance:
            current_min_distance = distance
            result = index

    return result




