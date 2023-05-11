import os
from collections import namedtuple
import cv2
import csv
import argparse
from utils.find_boxes_distance import get_closest_distance
from utils.merge_frames import zero_padding, combine_frames, get_extension
from IOU.utils.db_client import DB_Client
from IOU.utils.s3_client import S3_Client
from IOU.config import settings as opt
from tqdm import tqdm

Detection = namedtuple("Detection", ["image_path", "gt", "pred"])


# TODO - when im done with all, re-write the parse_args function
def parse_args():
    parser = argparse.ArgumentParser(description="IOU script")
    parser.add_argument('--video_context_id', required=False)
    parser.add_argument('--nx_ip', required=False)
    parser.add_argument("--flow_id", required=False)
    return parser.parse_args()


# def create_results_folder():
#     if path.endswith('/'):
#         name = (path.split('/'))[-2]
#     else:
#         name = (path.split('/'))[-1]
#     new_folder = name + '_results/'
#     try:
#         os.makedirs(new_folder)
#     except FileExistsError:
#         print(f"Folder {os.path.abspath(new_folder)} already exists, saving result in this path")
#     return os.path.abspath(new_folder)
def get_class_codes(db_client):
    quarry = """
    
    Select attribute_name,code,description
    from dwh.decode
    where attribute_name='class'
    order by code
    limit 100

    """
    class_code_data = db_client.sql_query_db(quarry)

    return class_code_data[["code", "description"]].set_index("code").to_dict()["description"]


def get_subclass_codes(db_client):
    quarry = """
    
    Select attribute_name,code,description
    from dwh.decode
    where attribute_name='subclass'
    order by code
    limit 100

    """
    subclass_code_data = db_client.sql_query_db(quarry)
    return subclass_code_data[["code", "description"]].set_index("code").to_dict()["description"]


def clean_data_gt(gt, db_client):
    class_df = get_class_codes(db_client)
    subclass_df = get_subclass_codes(db_client)
    gt['class_name'] = gt.class_code.apply(lambda x: class_df[x])
    gt['subclass_name'] = gt.subclass_code.apply(lambda x: subclass_df[x])
    return gt


def get_data(video_id):
    db_creds = {'user': 'chen-sheiner', 'password': '4kWY05mQQiRcr4BCFqG5',
                'host': 'database-poc.cgkbiipjug0o.eu-west-1.rds.amazonaws.com', 'db_name': 'SXDBPROD'}
    db_client = DB_Client(db_creds)
    get_class_codes(db_client)
    query = """ 
            SELECT dwh.objects.class_code,dwh.objects.subclass_code,
            dwh.objects.coordinates,dwh.objects.frame_id,
            dwh.allegro_videos.context_id,
            dwh.objects.video_gk, 
            dwh.videos.source_path,
            dwh.objects.shape_code
            
            FROM ((dwh.allegro_videos
            
            INNER JOIN dwh.objects ON dwh.allegro_videos.video_gk = dwh.objects.video_gk)
            INNER JOIN dwh.videos ON dwh.allegro_videos.video_gk = dwh.videos.video_gk )
            
            where dwh.allegro_videos.context_id='video_context_id'
            and dwh.objects.shape_code=1
            order by frame_id
            """

    # limit 100

    query_gt = query.replace("video_context_id", video_id)
    if not os.path.exists(f"{os.getcwd()}/data/{video_id}.csv"):
        print("Getting GT data from Database")

        data_gt = db_client.sql_query_db(query_gt)
        assert not data_gt.empty, f"No GT file in Database of name: {video_id}"
        data_gt = clean_data_gt(data_gt, db_client)
        data_gt.to_csv(f"{os.getcwd()}/data/{video_id}.csv")
    else:
        print("GT csv already exist")


def get_box_from_gt_csv():
    get_data(opt.video_context_id)
    csv_path = f"{os.getcwd()}/data/{opt.video_context_id}.csv"

    result = {}
    urls = []
    with open(csv_path, 'r') as csv_file:
        csv_reader = csv.reader(csv_file)

        next(csv_reader)
        # results = {}
        for line in csv_reader:
            # frame_id = line[0]
            frame_id = line[4]
            # subclass = line[2]
            subclass = line[-1]

            coordinates_lst = list(line[3])
            coordinates_lst = [char for char in coordinates_lst if
                               char.isdigit() or char == '.' or char == ' ' or char == ',']

            x1 = float((''.join(coordinates_lst)).split(',')[0])
            y1 = float((''.join(coordinates_lst)).split(',')[1])
            x2 = float((''.join(coordinates_lst)).split(',')[4])
            y2 = float((''.join(coordinates_lst)).split(',')[5])
            if subclass == 'motorcycle':
                class_ = "two-wheeled"
            else:
                class_ = line[-2]

            urls.append(line[7])
            result.setdefault(frame_id, []).append(
                (
                    class_,
                    x1,
                    y1,
                    x2,
                    y2
                )
            )

    sorted_keys = result.keys()
    sorted_keys = [int(x) for x in sorted_keys]
    sorted_keys.sort()
    return {str(key): result[str(key)] for key in sorted_keys}, list(set(urls))


def clean_gt_boxes(gt_boxes):
    for key in list(gt_boxes.keys()):
        objects_list = gt_boxes[key]
        for indx, object in reversed(list(enumerate(objects_list))):
            if object[0] == "dilemma_zone":
                del objects_list[indx]

    return gt_boxes


def download_frames(urls, id):
    s3 = S3_Client({})
    for url in urls:
        if isinstance(url, str):
            bucket = url.split("//")[1].split("/")[0]
            key = url.split("//")[1].split(bucket)[1]
            folder_name = key.split("/")[-1]
            os.makedirs(f"data/{folder_name}/", exist_ok=True)
            # assert os.path.exists(f"data/{id}/{folder_name}"), "Missing Frames "
            files_list=s3.get_files_in_folder(bucket, key[1:])
            for file in tqdm(files_list, desc="Downloading Frames"):
                if not os.path.exists(f"data/{folder_name}/{file.split('/')[-1]}"):  # if file exist don't download it
                    s3.download_file(bucket, file, f"data/{folder_name}/{file.split('/')[-1]}") # TODO add functionality to download whole folder and not just file as expeced now
    return f"data/{folder_name}/"

def generate_detections_csv(ip, video_path, flow_id, output_csv_path):
    if not os.path.isfile(output_csv_path):

        os.system(
            f"single_frame/./sdk_sample_single_frame {ip} {video_path} {flow_id} {output_csv_path}")
        assert os.path.isfile(output_csv_path), "Detection CSV creation failed"
    else:
        print("detections csv already exist")


def get_box_from_detection_csv(urls):
    frames_folder=download_frames(urls, opt.video_context_id)
    video_path = opt.video_context_id
    if not video_path.endswith('/'):
        video_path += '/'
    video_path=f"{os.getcwd()}/data/{video_path}"

    csv_path = f"{os.getcwd()}/data/{opt.video_context_id}_detections.csv"
    generate_detections_csv(opt.nx_ip, video_path, opt.flow_id, csv_path)

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
                        row[5]  # y2
                    )
                )

        return boxes,frames_folder


def bb_iou_score(boxA, boxB, epsilon=1e-5):
    """ Given two boxes `boxA` and `boxB` defined as a tuple of four numbers:
        (x1,y1,x2,y2)
        where:
            x1,y1 represent the upper left corner
            x2,y2 represent the lower right corner
        It returns the Intersect of Union score for these two boxes.

        Args:
            boxA:          (tuple of 4 numbers) (x1,y1,x2,y2)
            boxB:          (tuple of 4 numbers) (x1,y1,x2,y2)
            epsilon:    (float) Small value to prevent division by zero

        Returns:
            (float) The Intersect of Union score.
    """
    xA = max(boxA[0], boxB[0])
    yA = max(boxA[1], boxB[1])
    xB = min(boxA[2], boxB[2])
    yB = min(boxA[3], boxB[3])

    # interArea = max(0, xB - xA + 1) * max(0, yB - yA + 1)
    #
    # boxA_area = (boxA[2] - boxA[0] + 1) * (boxA[3] - boxA[1] + 1)  #
    # boxB_area = (boxB[2] - boxB[0] + 1) * (boxB[3] - boxB[1] + 1)
    #
    # iou = interArea / float(boxA_area + boxB_area - interArea)
    # return iou

    width = (xB - xA)
    height = (yB - yA)
    # handle case where there is NO overlap
    if (width < 0) or (height < 0):
        return 0.0
    area_overlap = width * height

    # COMBINED AREA
    area_a = (boxA[2] - boxA[0]) * (boxA[3] - boxA[1])
    area_b = (boxB[2] - boxB[0]) * (boxB[3] - boxB[1])
    area_combined = area_a + area_b - area_overlap

    # RATIO OF AREA OF OVERLAP OVER COMBINED AREA
    iou = area_overlap / (area_combined + epsilon)
    return iou


def clean_gt_boxes(gt_boxes):
    for key in list(gt_boxes.keys()):
        objects_list = gt_boxes[key]
        for indx, object in reversed(list(enumerate(objects_list))):
            if object[0] == "dilemma_zone":
                del objects_list[indx]

    return gt_boxes


def get_num_of_objects(gt_boxes):
    total_objects = 0
    total_frames = len(gt_boxes.keys())
    for key in list(gt_boxes.keys()):
        objects_list = gt_boxes[key]
        total_objects += len(objects_list)

    return total_objects / total_frames


def get_object_size(gt_boxes):
    results = {'vehicle': [], 'person': [], 'two-wheeled': []}
    v_total_width = v_total_height = 0
    p_total_width = p_total_height = 0
    t_total_width = t_total_height = 0
    vehicle_count = person_count = two_wheeled_count = 0

    for key in list(gt_boxes.keys()):
        objects_list = gt_boxes.get(key)
        for obj in objects_list:
            if obj[0] == "vehicle":
                vehicle_count += 1
                object_width = obj[3] - obj[1]
                object_height = obj[4] - obj[2]
                results["vehicle"].append((object_width, object_height))
            elif obj[0] == "person":
                person_count += 1
                object_width = obj[3] - obj[1]
                object_height = obj[4] - obj[2]
                results["person"].append((object_width, object_height))
            elif obj[0] == "two-wheeled":
                two_wheeled_count += 1
                object_width = obj[3] - obj[1]
                object_height = obj[4] - obj[2]
                results["two-wheeled"].append((object_width, object_height))

    for v_width, v_height in results["vehicle"]:
        v_total_width += v_width
        v_total_height += v_height

    for p_width, p_height in results["person"]:
        p_total_width += p_width
        p_total_height += p_height

    for t_width, t_height in results["two-wheeled"]:
        t_total_width += t_width
        t_total_height += t_height

    result_list = []
    try:
        p_avg_width = p_total_width / person_count
        p_avg_height = p_total_height / person_count
        result_list.append((p_avg_width, p_avg_height))
    except ZeroDivisionError:
        result_list.append((0, 0))
    try:
        v_avg_widht = v_total_width / vehicle_count
        v_avg_height = v_total_height / vehicle_count
        result_list.append((v_avg_widht, v_avg_height))
    except ZeroDivisionError:
        result_list.append((0, 0))
    try:
        t_avg_widht = t_total_width / two_wheeled_count
        t_avg_height = t_total_height / two_wheeled_count
        result_list.append((t_avg_widht, t_avg_height))
    except ZeroDivisionError:
        result_list.append((0, 0))

    return result_list


def get_distance_between_objects(gt_boxes):
    return None
    # TODO: finish implement this func
    distances = set()
    for frame in gt_boxes.keys():
        boxes = gt_boxes[frame]
        for index, box in enumerate(boxes):
            curr_box_x1 = float(box[1])
            curr_box_y1 = float(box[2])
            for other_box in range(index + 1, len(boxes)):
                other_box_x1 = float(box[1])
                other_box_y1 = float(box[2])
                print(other_box_x1, curr_box_x1)
                x1_distance = (curr_box_x1 - other_box_x1)
                y1_distance = abs(curr_box_y1 - other_box_y1)
                print(x1_distance, y1_distance)
                distances.add((x1_distance, y1_distance))
    print(distances)


def main():
    indexes_to_delete = set()
    FP_counter = FN_counter = TP_counter = 0

    # getting the ground-truth boxes from csv, the format is {frame_id: [(class,x1,y1,x2,y2), ... ] }
    gt_boxes, frames_urls = get_box_from_gt_csv()

    # getting rid of "dilemma_zone" annotations
    gt_boxes = clean_gt_boxes(gt_boxes)

    # getting the detection boxes from csv, the format is {frame_id: [(class,x1,y1,x2,y2), ... ] }
    detect_boxes ,frames_folder= get_box_from_detection_csv(frames_urls)

    # Getting details for output report:
    # Avg object's width and height in the video (pixels)
    objects_sizes = get_object_size(gt_boxes)

    # Avg objects per frame in the video
    object_count = get_num_of_objects(gt_boxes)

    # Avg denisty in the video (pixels)
    object_density = get_distance_between_objects(gt_boxes)

    # pre-process the frames in the folder
    # zero_padding(path, extension)
    # combine_frames(path, fps, extension, output_vid_name)
    #
    # cap = cv2.VideoCapture(f"{os.getcwd()}/results/{output_vid_name}.mp4")
    # if not cap.isOpened():
    #     print(f"Error occurred... Video {os.getcwd() + '/' + output_vid_name}.mp4 is not opened")
    #     exit(1)

    # in order to save result video
    vid_fps = 20.0
    frame=os.listdir(frames_folder)[0]
    frame_width = int(cv2.imread(f"{frames_folder}{frame}").shape[1])
    frame_height = int(cv2.imread(f"{frames_folder}{frame}").shape[0])
    out = cv2.VideoWriter(f'results/{opt.video_context_id}_video.avi', cv2.VideoWriter_fourcc('M','J','P','G'), vid_fps,
                          (frame_width, frame_height))

    frame_num = int(next(iter(detect_boxes.keys())))  # getting the first frame number (it can be 0 or 1)

    with open(f"results/{opt.video_context_id}_report.csv", "a") as results_file:
        # Checking whetere the result file exist
        if not os.path.exists(f"{os.getcwd()}/results/{opt.video_context_id}_report.csv"):
            results_file.write(
                "Recall,Precision,PD,FAR,obj_type,obj_width,obj_height,obj_type,obj_width,obj_height,obj_type,obj_width,obj_height\n")
        # Read until video is completed
        for frame in os.listdir(frames_folder):
            frame=cv2.imread(f"{frames_folder}{frame}")
            # Capture frame-by-frame
            # ret, frame = cap.read()
            # if ret == True:
            # Saving the gt and detections results for frame_num
            detection_result = detect_boxes.get(str(frame_num))
            gt_result = gt_boxes.get(str(frame_num))

            # Case 1:
            # No gt frame_num but there's detection frame_num
            # i.e. |gt_csv_frames| < |detection_csv_frames|
            if gt_result is None and detection_result is not None:
                if detection_result != -1:
                    FP_counter += len(detection_result)
                frame_num += 1
                continue

            # Case 2:
            # No gt frame_num and no detection frame_num
            elif gt_result is None and detection_result is None:
                frame_num += 1
                continue

            # Case 3:
            # There's gt frame_num but no detection frame_num
            elif gt_result is not None and detection_result is None:
                if len(gt_result):
                    FN_counter += len(gt_result)
                frame_num += 1
                continue
            # Case 4:
            # There's gt frame_num and there's detection frame_num
            elif gt_result is not None and detection_result is not None:
                if detection_result == -1:
                    FN_counter += len(gt_result)
                    frame_num += 1
                    continue

            # Iterating through all gt boxes
            for gt_box in gt_result:
                # Getting the closest prediction box from all prediction boxes list
                index = get_closest_distance(gt_box, detection_result)
                detect_relevant_box = detection_result[index]

                # Adding the prediction box we used to the set
                indexes_to_delete.add(index)

                # Change numeric coordinates values of each box from str to float
                detect_relevant_box = (
                    detect_relevant_box[0],  # Class
                    float(detect_relevant_box[1]),  # X1
                    float(detect_relevant_box[2]),  # Y1
                    float(detect_relevant_box[3]),  # X2
                    float(detect_relevant_box[4])  # Y2
                )

                gt_box = (
                    gt_box[0],  # Class
                    float(gt_box[1]),  # X1
                    float(gt_box[2]),  # Y1
                    float(gt_box[3]),  # X2
                    float(gt_box[4])  # Y2
                )

                # Calculating the iou score
                iou_score = bb_iou_score(
                    (
                        detect_relevant_box[1], detect_relevant_box[2], detect_relevant_box[3],
                        detect_relevant_box[4]),
                    (gt_box[1], gt_box[2], gt_box[3], gt_box[4])
                )

                detection_class = detect_relevant_box[0]
                gt_class = gt_box[0]

                # Checking that prediction class and gt class is different
                # TODO: consider "eye examination" for more accurate results
                if (detection_class).find(gt_class) == -1:
                    if iou_score < 0.25:
                        FN_counter += 1
                    else:
                        FP_counter += 1

                # Otherwise is the same class for both gt and prediction
                else:
                    if iou_score == 0:
                        FN_counter += 1
                    elif 0 < iou_score <= 0.4:
                        FP_counter += 1
                    else:
                        TP_counter += 1

                # Drawing on the frame the followings:
                # * gt_box with green color
                # * detection_box with red color
                # * iou score
                #
                # Note: We are using int values because cv2.rectangle()
                #       doesn't know to work with float values
                detect_start_point = (int(detect_relevant_box[1]), int(detect_relevant_box[2]))
                detect_end_point = (int(detect_relevant_box[3]), int(detect_relevant_box[4]))
                gt_start_point = (int(gt_box[1]), int(gt_box[2]))
                gt_end_point = (int(gt_box[3]), int(gt_box[4]))

                cv2.rectangle(frame, gt_start_point, gt_end_point, (0, 255, 0), 2)

                cv2.rectangle(frame, detect_start_point, detect_end_point, (0, 0, 255), 2)

                cv2.putText(frame, "IoU: {:.2f}".format(iou_score),
                            (int(detect_relevant_box[1]) - 20, int(detect_relevant_box[2]) - 20),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0),
                            2)  # drawing IOU score per image

            # Deleting all prediction boxes we used from the list
            for indx in reversed(list(indexes_to_delete)):
                try:
                    del detection_result[indx]
                except Exception:
                    print(f"Exception when trying to delete detection index")

            indexes_to_delete.clear()

            # TODO: uncomment/comment this line in order to see/not see the engine FP during the video
            for detect_box in detection_result:
                detect_start_point = (int(float(detect_box[1])), int(float(detect_box[2])))
                detect_end_point = (int(float(detect_box[3])), int(float(detect_box[4])))
                cv2.rectangle(frame, detect_start_point, detect_end_point, (0, 0, 255), 2)

            # Displaying the frame with the results
            # cv2.imshow('Frame', frame)
            out.write(frame)

            # Press Q on keyboard to exit
            # if cv2.waitKey(25) & 0xFF == ord('q'):
            #     break

            frame_num += 1



        # All unused prediction boxes considered as FP
        for key in list(detect_boxes.keys()):
            if detect_boxes[key] != -1 and len(detect_boxes[key]):
                FP_counter += len(detect_boxes[key])

        Recall = TP_counter / (TP_counter + FP_counter)
        Precision = TP_counter / (TP_counter + FP_counter)
        PD = TP_counter / (TP_counter + FN_counter)
        FAR = FP_counter / (TP_counter + FP_counter)

        # The objects avg width and height
        person_avg = objects_sizes[0]
        vehicle_avg = objects_sizes[1]
        two_wheeled_avg = objects_sizes[2]

        # Writing all insights to the output report
        results_file.write(
            f"{str(Recall)},{str(Precision)},{str(PD)},{str(FAR)},vehicle,{str(vehicle_avg[0])},{str(vehicle_avg[1])},"
            f"person,{str(person_avg[0])},{str(person_avg[1])},two-wheeled,{two_wheeled_avg[0]},{two_wheeled_avg[1]}\n")
        results_file.write(
            f"\nAverage of {object_count} objects per frame, with average distance of {object_density} between the objects\n")
        results_file.write("\n\n")


    # When everything done, release
    # the video capture object
    # cap.release()
    out.release()
    # Closes all the frames
    cv2.destroyAllWindows()
    print("End")
    return


def define_options(args):
    if args.nx_ip != None:
        opt.set("nx_ip", args.nx_ip)
    if args.flow_id != None:
        opt.set("flow_id", args.flow_id)
    if args.video_context_id != None:
        opt.set("video_context_id", args.video_context_id)


if __name__ == "__main__":
    args = parse_args()
    define_options(args)
    # Extracting relevant arguments

    # results_folder_path = create_results_folder() #TODO: repair and uncomment when all tests are done

    # For now - comment this line to execute.
    # This will create detections csv file from the meerkat engine
    #  generate_detections_csv(nx_ip, path, flow, os.getcwd()+"/test.csv")

    main()

    # Delete the not necessary files
    # os.system(f"rm {output_vid_name}.mp4") # TODO: Fix this when the rest is done

# The folder we are going to zip is include the followings:
#   * intersection_over_union.py
#   * merge_frames.py
#   * find_boxes_distance.py
#   * single frame folder with:
#       - c++ file
#       - CMakeList file
#       - empty build folder: initial state it will be empty, and we build all.
#                             in case of already built, (i.e. the folder isn't
#                             empty) - we won't build and run the exe file
# The flow process:
# 1. generate detections.csv
# 2. generate gt.csv (Asaf's script)
# 2. preprocess frames: zero_padding and merge to mp4
# 3. start to process the frames with iou and all
# 4. rm the output.mp4 from the folder
# 5. making results dir with the ___ name and save there the json and stuff...
#    (maybe also the output.mp4) and add report of how many frames processed,
#    How many FP, etc.
