import os
from collections import namedtuple
import numpy as np
import cv2
import csv
import argparse
import json
from find_boxes_distance import get_closest_distance
from merge_frames import zero_padding, combine_frames, get_extension

Detection = namedtuple("Detection", ["image_path", "gt", "pred"])

# TODO - when im done with all, re-write the parse_args function
def parse_args():
    parser = argparse.ArgumentParser(description="IOU script")
    parser.add_argument('--video_folder_path', required=True)
    parser.add_argument('--nx_ip', required=False, default='172.12.10.33')
    return parser.parse_args()


def generate_detections_csv(ip, video_path, flow_id):
    os.system(
        f"/home/lior.lakay/Desktop/python_scripts/IOU/iou_packed_test/single_frame/build/./sdk_sample_single_frame {ip} {video_path} {flow_id}")


def get_box_from_gt_csv(csv_path):
    result = {}
    with open(csv_path, 'r') as csv_file:
        csv_reader = csv.reader(csv_file)

        next(csv_reader)
        results = {}
        for line in csv_reader:
            frame_id = line[0]
            subclass = line[2]

            coordinates_lst = list(line[3])
            coordinates_lst = [char for char in coordinates_lst if char.isdigit() or char == '.' or char == ' ']

            x1 = float((''.join(coordinates_lst)).split(' ')[0])
            y1 = float((''.join(coordinates_lst)).split(' ')[1])
            x2 = float((''.join(coordinates_lst)).split(' ')[4])
            y2 = float((''.join(coordinates_lst)).split(' ')[5])
            if subclass == 'motorcycle':
                class_ = "two-wheeled"
            else:
                class_ = line[1]

            result.setdefault(frame_id, []).append(
                (
                    class_,
                    x1,
                    y1,
                    x2,
                    y2
                )
            )

    return result


def get_box_from_detection_csv(csv_path):
    boxes = {}
    with open(csv_path, "r") as csv_file:
        csv_reader = csv.reader(csv_file)
        next(csv_reader)
        for row in csv_reader:
            if row[1] == "None":
                boxes[row[0]] = -1
            else:
                boxes.setdefault(row[0], []).append(
                    (
                        row[1],  # detection class
                        row[2],  # x1
                        row[3],  # y1
                        row[4],  # x2
                        row[5]   # y2
                    )
                )

        return boxes


def bb_iou_score(boxA, boxB):
    """
    Function to calculate the actual IOU for two boxes
    params:
        * boxA: the engine's prediction BB with (x1,y1,x2,y2)
        * boxB: the ground-truth BB with (x1,y1,x2,y2)
    return:
        * the IOU score for two boxes
    """
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
    # Dictionary for storing the IOU score per frame, the format is { frame_id: [{class_name: score}, ... ] }
    iou_scores = {}


    # getting the detection boxes from csv, the format is {frame_id: [(class,x1,y1,x2,y2), ... ] }
    # detect_boxes = get_box_from_detection_csv("/home/lior.lakay/Desktop/python_scripts/IOU/single_frame/build/detections1.csv")
    detect_boxes = get_box_from_detection_csv("detections.csv")

    # getting the ground-truth boxes from csv, the format is {frame_id: [(class,x1,y1,x2,y2), ... ] }
    # gt_boxes = get_box_from_gt_csv("/home/lior.lakay/Desktop/python_scripts/IOU/TalmeyElyahu_2017_0003_W_CMOS_FOV83_rgb.csv")
    gt_boxes = get_box_from_detection_csv("detections.csv")

    # pre-process the frames in the folder
    zero_padding(path,extension)
    combine_frames(path, fps, extension, output_vid_name)

    cap = cv2.VideoCapture(output_vid_name)
    if not cap.isOpened():
        print(f"Error occurred... Video {os.getcwd() + '/' + output_vid_name} is not opened")
        exit(1)

    frame_num = int(next(iter(detect_boxes.keys())))  # getting the first frame number (it can be 0 or 1)
    # Read until video is completed
    while (cap.isOpened()):

        # Capture frame-by-frame
        ret, frame = cap.read()
        if ret == True:
            # Display the frame results after iou calculation

            # Checking that the detections.csv has the frame_num in the list, i.e. we can find in the
            # detect_boxes dictionary the frame_num as a key
            # if not exists ==> the process of the single_frame.cpp was interrupted while processing
            #                   and the build of detection.csv was not completed for all frames
            # if exists ==> * option 1: receive -1 as indication of no detections at all for the frame_num
            #               * option 2: receive dictionary of detected objects in the format
            #                           of { frame_num: [(class, x1, y1, x2, y2), ... ] }
            if detect_boxes.get(str(frame_num)) is not None and detect_boxes[str(frame_num)] != -1:

                # Iterate through all detections in frame_num
                for detection_box in detect_boxes[str(frame_num)]:

                    # finding the index of the relevant gt box compare to the detection box
                    # in case of multiple objects per frame.
                    # We are assuming that the relevant box from the gt boxes list is the one with
                    # the closest distance between his center point to detection box's center point
                    index = get_closest_distance(detection_box, gt_boxes[str(frame_num)])
                    gt_relevant_box = gt_boxes[str(frame_num)][index]

                    # Changing the numeric coordinates type from str to float
                    gt_relevant_box = (gt_relevant_box[0], float(gt_relevant_box[1]), float(gt_relevant_box[2]),
                                       float(gt_relevant_box[3]), float(gt_relevant_box[4]))
                    detection_box = (detection_box[0], float(detection_box[1]), float(detection_box[2]),
                                     float(detection_box[3]), float(detection_box[4]))

                    # Calculating the iou score
                    iou = bb_iou_score(
                        (detection_box[1], detection_box[2], detection_box[3], detection_box[4]),
                        (gt_relevant_box[1], gt_relevant_box[2], gt_relevant_box[3], gt_relevant_box[4])
                    )

                    # If iou is 0 it means that the detection is FP, need to define what to do with them.
                    # Note: can be situations where the iou is greater than 0 but still very low...
                    #       it can testify that the location of the gt box and the FP is intersecting
                    if iou < 0.1:
                        continue

                    # Verify that the distance assumption between gt box to detection box was
                    # correct(?) by checking each box class name
                    # i.e. if detection-box class is vehicle and gt-box class is two-wheeled its
                    # tell us that the distance process was not correct.
                    # TODO: if this method fails, think about other method for matching multi-objects
                    if (detection_box[0]).find(gt_relevant_box[0]) == -1:
                        print("Not the same class... bad calculation of the distance")
                        exit(1)

                    # Saving all scores for the same frame within the same list in the format of
                    # { frame_num: [ {class: iou_score }, ... ] }
                    iou_scores.setdefault(str(frame_num), []).append({detection_box[0]: iou})

                    # Drawing on the frame the followings:
                    # * gt_box with green color
                    # * detection_box with red color
                    # * iou score
                    gt_start_point = (int(gt_relevant_box[1]), int(gt_relevant_box[2]))
                    gt_end_point = (int(gt_relevant_box[3]), int(gt_relevant_box[4]))
                    detect_start_point = (int(detection_box[1]), int(detection_box[2]))
                    detect_end_point = (int(detection_box[3]), int(detection_box[4]))

                    cv2.rectangle(frame, gt_start_point, gt_end_point, (0, 255, 0), 2)

                    cv2.rectangle(frame, detect_start_point, detect_end_point, (0, 0, 255), 2)

                    cv2.putText(frame, "IoU: {:.4f}".format(iou), (int(detection_box[1]) - 20, int(detection_box[2]) - 20),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0),
                                2)  # drawing IOU score per image
                    print("{}: {:.4f}".format(Detection.image_path, iou))

            else:
                # Implement here the case of no detections at all for the frame_num

                # Checking here that indeed the frame_num was in the detections.csv,
                # and we have failed on detect anything in the frame_num
                if detect_boxes.get(str(frame_num)) is not None:
                    pass
                else:
                    pass

            # Displaying the frame with the results
            cv2.imshow('Frame', frame)

            # Press Q on keyboard to exit
            if cv2.waitKey(25) & 0xFF == ord('q'):
                break

            frame_num += 1

        # Break the loop
        else:
            break

    # When everything done, release
    # the video capture object
    cap.release()

    # Closes all the frames
    cv2.destroyAllWindows()

    # Saving the iou test summary within json file
    os.chdir(main_path)
    with open("IoU_summary.json", 'w') as file:
        json.dump(iou_scores, file, indent=2)

    return



if __name__ == "__main__":
    # TODO: add here same logic with the end of path with slash as we
    #       did in merge_frames.py and decide what params the programe
    #       will get
    args = parse_args()

    nx_ip = args.nx_ip
    path = args.video_folder_path
    extension = get_extension(path)
    output_vid_name = "output"
    flow = "rgb"
    fps = 30

    if not path.endswith('/'):
        path += '/'

    generate_detections_csv(nx_ip, path, flow)

    # combine_frames(args.path, args.fps)
    # detect_boxes = get_box_from_detection_csv("detections1.csv")
    # print(detect_boxes)
    main_path = os.getcwd()
    main()

# The folder we are going to zip is include the followings:
#   * intersection_over_union.py
#   * merge frames
#   * single frame folder
# The flow process I want:
# 1. generate detections.csv
# 2.