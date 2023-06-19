import os
from collections import namedtuple
import cv2
import csv
import argparse
from utils.find_boxes_distance import get_closest_distance
from utils.merge_frames import zero_padding, combine_frames, get_extension
from utils.db_client import DB_Client
from utils.s3_client import S3_Client
from config import settings as opt
from tqdm import tqdm
import pandas as pd
import numpy as np
from time import perf_counter, sleep
import motmetrics as mm

# MOT Metrics info:
# https://pub.towardsai.net/multi-object-tracking-metrics-1e602f364c0c
# https://github.com/cheind/py-motmetrics/tree/develop
# TODO chen change paths to new name
Detection = namedtuple("Detection", ["image_path", "gt", "pred"])

tracker_metrics = ['idf1', 'idp', 'idr', 'recall', 'precision', 'num_unique_objects', 'mostly_tracked',
                   'partially_tracked', 'mostly_lost', 'num_false_positives', 'num_misses', 'num_switches',
                   'num_fragmentations', 'mota', 'motp', 'num_transfer', 'num_ascend', 'num_migrate', 'num_predictions',
                   'num_matches', 'num_frames', 'num_objects', 'num_detections', 'idfn', 'idtp', 'idfp']

general_columns_names = ['context_id', 'video_gk', 'source_path',
                         'video_name', 'sky_condition', 'light_condition',
                         'light_intensity', 'landform', 'frame_shape_w', 'frame_shape_h','payload_code','wavelength_code']


# ['num_frames', 'obj_frequencies', 'pred_frequencies', 'num_matches', 'num_switches', 'num_transfer',
#                'num_ascend', 'num_migrate', 'num_false_positives', 'num_misses', 'num_detections', 'num_objects',
#                'num_predictions', 'num_unique_objects', 'track_ratios', 'mostly_tracked', 'partially_tracked',
#                'mostly_lost', 'num_fragmentations', 'motp', 'mota', 'precision', 'recall', 'id_global_assignment',
#                'idfp', 'idfn', 'idtp', 'idp', 'idr', 'idf1']

def parse_args():
    parser = argparse.ArgumentParser(description="IOU script")
    parser.add_argument('--video_context_id', required=False)
    parser.add_argument('--nx_ip', required=False)
    parser.add_argument("--flow_id", required=False)
    parser.add_argument("--create_video", required=False)
    return parser.parse_args()


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
    gt = gt.drop_duplicates(subset=["frame_id", "object_id"])
    gt = gt.reset_index()
    gt.drop(columns=["index", "class_code", "subclass_code"], inplace=True,errors='ignore')
    gt.index.names = ['index']
    return gt


def get_data_no_context_id(video_id, client):
    query = """ 
                SELECT dwh.objects.class_code,dwh.objects.subclass_code,
                dwh.objects.coordinates,dwh.objects.frame_id,
                dwh.objects.video_gk, 
                dwh.videos.source_path,
                dwh.objects.object_id,
                dwh.videos.video_name
                
                FROM (dwh.videos
                INNER JOIN dwh.objects ON dwh.videos.video_gk = dwh.objects.video_gk)
                
                
                where dwh.videos.video_name='video_context_id'
                and dwh.objects.shape_code=1
                order by frame_id

                """
    print("Getting GT data from Database using video name")
    query_gt = query.replace("video_context_id", video_id)

    data_gt = client.sql_query_db(query_gt)
    assert not data_gt.empty, f"No GT file in Database of name: {video_id}"
    data_gt.loc[:, "context_id"] = video_id
    return data_gt


def get_data(video_id):
    db_creds = {'user': 'chen-sheiner', 'password': '4kWY05mQQiRcr4BCFqG5',
                'host': 'database-poc.cgkbiipjug0o.eu-west-1.rds.amazonaws.com', 'db_name': 'SXDBPROD'}
    db_client = DB_Client(db_creds)
    query = """ 
            SELECT dwh.objects.class_code,dwh.objects.subclass_code,
            dwh.objects.coordinates,dwh.objects.frame_id,
            dwh.allegro_videos.context_id,
            dwh.objects.video_gk, 
            dwh.videos.source_path,
            dwh.objects.object_id,
            dwh.videos.video_name,
			dwh.videos.bits,
			dwh.videos.payload_code,dwh.videos.wavelength_code,
			dwh.get_desc('wavelength',wavelength_code) as wavelength,
            dwh.get_desc('sky_condition',sky_condition_code) as sky_condition,
            dwh.get_desc('light_condition',light_condition_code) as light_condition,
            dwh.get_desc('light_intensity',light_intensity_code) as light_intensity,
            dwh.get_desc('landform',landform_code) as landform
            
            FROM ((dwh.allegro_videos
            
            INNER JOIN dwh.objects ON dwh.allegro_videos.video_gk = dwh.objects.video_gk)
            INNER JOIN dwh.videos ON dwh.allegro_videos.video_gk = dwh.videos.video_gk )
            
            where dwh.allegro_videos.context_id='video_context_id'
            and dwh.objects.shape_code=1
            order by frame_id
            """

    # limit 100

    if not os.path.exists(f"{os.getcwd()}/data/GT/{video_id}.csv"):
        print("Getting GT data from Database")
        query_gt = query.replace("video_context_id", video_id)

        data_gt = db_client.sql_query_db(query_gt)
        if data_gt.empty:
            print(f"No GT file in Database of context id: {video_id}")
            data_gt = get_data_no_context_id(video_id, db_client)
        data_gt = clean_data_gt(data_gt, db_client)
        data_gt.to_csv(f"{os.getcwd()}/data/GT/{video_id}.csv")
    else:
        print("GT CSV Already Exist")
        data_gt = pd.read_csv(f"{os.getcwd()}/data/GT/{video_id}.csv")
    return data_gt


def get_box_from_gt_csv():
    data_gt = get_data(opt.video_context_id)
    data_gt.coordinates = data_gt.coordinates.apply(eval)  # TODO add rename to motorcycle to two-wheel subclass
    data_gt.coordinates = data_gt.coordinates.apply(lambda x: np.concatenate(x[0:3:2]))
    data_gt[["x1", "y1", "x2", "y2"]] = pd.DataFrame(data_gt.coordinates.tolist(), index=data_gt.index)
    data_gt[["x1", "y1", "x2", "y2"]] = data_gt[["x1", "y1", "x2", "y2"]].astype(int)
    # assert "data-lake-production-source" not in data_gt.source_path[0], ["Data is in S3 Bucket that i dont have access to, data-lake-production-source ",data_gt.source_path[0]]
    return data_gt, data_gt.source_path[0:1]


def clean_gt_boxes(gt_boxes,frames_folder):
    drop_indx = gt_boxes[gt_boxes.class_name == "dilemma_zone"].index
    gt_boxes = gt_boxes.drop(drop_indx,errors='ignore')
    gt_boxes.drop(columns="index", inplace=True,errors='ignore')

    frame = os.listdir(frames_folder)[0]
    demo_img = cv2.imread(f"{frames_folder}{frame}")
    if not isinstance(demo_img,np.ndarray) :
        demo_img = cv2.imread(f"{frames_folder}/{frame}")

    frame_width = int(demo_img.shape[1])
    frame_height = int(demo_img.shape[0])
    gt_boxes["frame_shape_w"] = frame_width
    gt_boxes["frame_shape_h"] = frame_height


    general_data = gt_boxes[general_columns_names].drop_duplicates()
    assert general_data.shape[0] == 1, "Video configuration change mid video"
    gt_boxes.drop(columns=general_columns_names, inplace=True,errors='ignore')
    return gt_boxes,general_data


def download_frames(urls):
    s3 = S3_Client({})
    for url in urls:
        if isinstance(url, str):
            bucket = url.split("//")[1].split("/")[0]
            key = url.split("//")[1].split(bucket)[1]
            folder_name = key.split("/")[-1]
            os.makedirs(f"data/{folder_name}/", exist_ok=True)
            print(bucket, key[1:])
            files_list = s3.get_files_in_folder(bucket, key[1:])
            for file in tqdm(files_list, desc="Downloading Frames"):
                if not os.path.exists(f"data/{folder_name}/{file.split('/')[-1]}"):  # if file exist don't download it
                    s3.download_file(bucket, file,
                                     f"data/{folder_name}/{file.split('/')[-1]}")  # TODO add functionality to download whole folder and not just file as expeced now
    return f"data/{folder_name}/"


def clean_tracker_data(data):
    try:
        data[["x1", "y1", "x2", "y2"]] = data[["x1", "y1", "x2", "y2"]].astype(pd.Int32Dtype())
    except Exception as e:
        print("Tracker  File Has Empty lines:", e)
    # data = data[data.score > 0]
    data = data[data[["x1", "y1", "x2", "y2"]].sum(axis=1) > 0]
    return data


def generate_tracker_csv(ip, video_path, flow_id, output_csv_path, pixel_mean, pixel_std):
    if not os.path.exists(output_csv_path):
        start = perf_counter()
        os.system(
            f"single_frame/./sdk_sample_raw_frames {ip} {video_path} {flow_id} {output_csv_path} {pixel_mean} {pixel_std}")
        print("Tracker time:", perf_counter() - start)
    else:
        print("Tracker CSV Already Exist")
        if os.stat(output_csv_path).st_size < 10:
            print("Trying to Create Tracker File again")
            os.system(
                f"single_frame/./sdk_sample_raw_frames {ip} {video_path} {flow_id} {output_csv_path} {pixel_mean} {pixel_std}")


def generate_detections_csv(ip, video_path, flow_id, output_csv_path, pixel_mean, pixel_std):
    if not os.path.exists(output_csv_path):
        start = perf_counter()
        os.system(
            f"single_frame/./sdk_sample_single_frame {ip} {video_path} {flow_id} {output_csv_path} {pixel_mean} {pixel_std}")
        print("Detections time:", perf_counter() - start)
    else:
        print("Detections CSV Already Exist")
        if os.stat(output_csv_path).st_size < 10:
            print("Trying to Create Detection File again")
            os.system(
                f"single_frame/./sdk_sample_single_frame {ip} {video_path} {flow_id} {output_csv_path} {pixel_mean} {pixel_std}")


def get_box_from_detection_csv(urls, norm_values,flow):
    # frames_folder = download_frames(urls) #TODO change back
    frames_folder = '/home/chen/PycharmProjects/qa-scripts/model_performance_data/data/RitzCarltonHerzliya_D20210706_Pn8_SSummer_N00027_CtNone_H030_LcFullDaylight_Ds01500_LtNone_LfDunes_VrNone_StNatural_LdBackLight_B00_V00_Js00_P15_T30_MWIR_unknown-8bit'
    video_path = opt.video_context_id
    if not video_path.endswith('/'):
        video_path += '/'
    video_path = f"{os.getcwd()}/data/{video_path}"
    extension = os.listdir(video_path)[0].split(".")[1]
    # print(f"extention of images:{extension}")

    csv_path_detection = f"{os.getcwd()}/data/detections/{opt.video_context_id}_detections.csv"
    csv_path_tracker = f"{os.getcwd()}/data/detections/{opt.video_context_id}_tracker.csv"
    try:
        if "8bit" in video_path:
            values = norm_values.loc['8']
        else:
            values = norm_values.loc['16']
    except KeyError:
        print("no correct type of compression in normalization-values database.")
        print("using the first configuration as values:",norm_values.iloc[:1])
        values = norm_values.iloc[0]
    generate_detections_csv(opt.nx_ip, video_path, flow, csv_path_detection, values[0], values[1])
    sleep(5)
    video_path += f"%05d.{extension}"
    generate_tracker_csv(opt.nx_ip, video_path, flow, csv_path_tracker, values[0], values[1])
    assert os.path.exists(csv_path_detection) and os.stat(
        csv_path_detection).st_size > 10, "Detection File Creation Failed"
    assert os.path.exists(csv_path_tracker) and os.stat(csv_path_tracker).st_size > 10, "Tracker File Creation Failed"
    detections_data = pd.read_csv(csv_path_detection)
    tracker_data = pd.read_csv(csv_path_tracker)
    tracker_data = clean_tracker_data(tracker_data)
    try:
        detections_data[["x1", "y1", "x2", "y2"]] = detections_data[["x1", "y1", "x2", "y2"]].astype(pd.Int32Dtype())
    except Exception as e:
        print("Detection File Has Empty lines:", e)

    return detections_data, tracker_data, frames_folder


def calc_iou(roi1, roi2):
    # Sum all "white" pixels clipped to 1
    U = np.sum(np.clip(roi1 + roi2, 0, 1))
    # +1 for each overlapping white pixel (these will = 2)
    I = len(np.where(roi1 + roi2 == 2)[0])
    return (I / U), U, I


def bb_iou_score(box_a, box_b):
    """ Given two boxes `boxA` and `boxB` defined as a tuple of four numbers:
        (x1,y1,x2,y2)
        where:
            x1,y1 represent the upper left corner
            x2,y2 represent the lower right corner
        It returns the Intersect of Union score for these two boxes.

        Args:
            box_a:          (tuple of 4 numbers) (x1,y1,x2,y2)
            boxB:          (tuple of 4 numbers) (x1,y1,x2,y2)
            epsilon:    (float) Small value to prevent division by zero

        Returns:
            (float) The Intersect of Union score.
    """
    box_b = box_b.astype(int)
    box_a = box_a.astype(int)

    demo_frame_w = max(box_a[0], box_a[2], box_b[0], box_b[2]) + 50
    demo_frame_h = max(box_a[1], box_a[3], box_b[1], box_b[3]) + 50
    roi1 = np.zeros((demo_frame_h, demo_frame_w))
    roi2 = np.zeros((demo_frame_h, demo_frame_w))

    roi1[box_a[1]:box_a[3], box_a[0]:box_a[2]] = 1
    roi2[box_b[1]:box_b[3], box_b[0]:box_b[2]] = 1

    iou, union_area, intersection = calc_iou(roi1, roi2)
    return iou, intersection


def match_tracker_gt(gt_boxes, track_boxes):
    if "index" not in gt_boxes:
        gt_boxes.index.names = ['index']
        gt_boxes = gt_boxes.reset_index()

    # results = define_new_columns(gt_boxes)

    gt_boxes["h"] = gt_boxes.apply(lambda x: int(x["y2"] - x["y1"]), axis=1)
    gt_boxes["w"] = gt_boxes.apply(lambda x: int(x["x2"] - x["x1"]), axis=1)
    track_boxes["h"] = track_boxes.apply(lambda x: int(x["y2"] - x["y1"]), axis=1)
    track_boxes["w"] = track_boxes.apply(lambda x: int(x["x2"] - x["x1"]), axis=1)

    acc_list = []
    for iou_thrsh in np.arange(0, 1, 0.1):
        acc = mot_metric_calc(gt_boxes, track_boxes, iou_threshold=iou_thrsh)
        acc_list.append(acc)

    mh = mm.metrics.create()

    summary = mh.compute_many(
        acc_list,
        metrics=tracker_metrics,
        names=np.around(np.arange(0, 1, 0.1), decimals=1),
        generate_overall=False
    )
    summary.index.name = 'iou_threshold'
    return summary


def mot_metric_calc(gt_data, tracker_data, iou_threshold=0.5):
    acc = mm.MOTAccumulator(auto_id=True)
    for frame_id in tqdm(gt_data.frame_id.unique(), desc=f"Matching GT to Tracker @iou={iou_threshold:.1f}"):
        gt_boxes_in_frame = gt_data[gt_data.frame_id == frame_id]
        track_boxes_in_frame = tracker_data[tracker_data.frame_id == frame_id]

        hypothesis = track_boxes_in_frame[["x1", "y1", "w", "h"]]  # coordinates of three hypothesis in the frame
        # Format--> X, Y, Width, Height

        gt = gt_boxes_in_frame[["x1", "y1", "w", "h"]]  # coordinates of groundtruth objects

        distance_matrix = mm.distances.iou_matrix(hypothesis, gt, max_iou=iou_threshold)
        acc.update(
            gt_boxes_in_frame.object_id.to_list(),  # Ground truth object ID in this frame
            track_boxes_in_frame.track_id.to_list(),  # Detector hypothesis ID in this frame
            [distance_matrix])

    return acc


def create_detection_gt_result(gt_data, det_data,general_data):
    final_result = pd.DataFrame()
    acc_list=[]
    for iou_threshold in np.arange(0, 1, 0.1):
    # for iou_threshold in [0.7]:
        result, acc= match_detection_gt_v2(gt_data, det_data,general_data, iou_threshold=np.round(iou_threshold, decimals=1))
        acc_list.append(acc)
        final_result = pd.concat([final_result, result])

    return final_result,acc_list


def match_detection_gt_v2(gt_data, det_data,general_data, iou_threshold=0.5):
    det_data = det_data.dropna(subset=['x1', 'x2', 'y1', 'y2'])

    gt_data["h"] = gt_data.apply(lambda x: int(x["y2"] - x["y1"]), axis=1)
    gt_data["w"] = gt_data.apply(lambda x: int(x["x2"] - x["x1"]), axis=1)
    gt_data["gt_area"] = gt_data["w"] * gt_data["h"]
    det_data["det_h"] = det_data.apply(lambda x: int(x["y2"] - x["y1"]), axis=1)
    det_data["det_w"] = det_data.apply(lambda x: int(x["x2"] - x["x1"]), axis=1)
    det_data["det_area"] = det_data["det_w"] * det_data["det_h"]
    det_data.rename(
        columns={'class': 'det_class', 'x1': 'det_x1', 'y1': 'det_y1', 'x2': 'det_x2', 'y2': 'det_y2', }, inplace=True)
    results = define_new_columns(gt_data,general_data.columns).iloc[:0, :].copy()



    acc = mm.MOTAccumulator(auto_id=False)
    for frame_id in tqdm(gt_data.frame_id.unique(), desc=f"Matching GT to Tracker @iou={iou_threshold:.1f}"):
        det_boxes_in_frame = det_data[det_data.frame_id == frame_id]
        gt_boxes_in_frame = gt_data[gt_data.frame_id == frame_id]

        if det_boxes_in_frame.empty:
            for i, row in gt_boxes_in_frame.iterrows():
                results.loc[len(results)] = row.to_dict() | \
                                            {"match_type": "FN"} |\
                                            general_data.to_dict('index')[0]
            continue
        hypothesis = det_boxes_in_frame[
            ["det_x1", "det_y1", "det_w", "det_h"]]  # coordinates of three hypothesis in the frame
        # Format--> X, Y, Width, Height

        gt = gt_boxes_in_frame[["x1", "y1", "w", "h"]]  # coordinates of groundtruth objects

        distance_matrix = mm.distances.iou_matrix(hypothesis, gt, max_iou=iou_threshold)
        acc.update(
            gt_boxes_in_frame.object_id.to_list(),  # Ground truth object ID in this frame
            det_boxes_in_frame.index.to_list(),  # Detector hypothesis ID in this frame
            [distance_matrix], frameid=frame_id)
        matched = acc.mot_events.loc[frame_id]
        matched.drop_duplicates(subset=["OId", "HId"],inplace=True)

        for i, row in matched.iterrows():
            if row.Type == "MATCH" or row.Type == "ASCEND" or row.Type == "SWITCH" or row.Type == "TRANSFER" or row.Type == "MIGRATE" :
                if gt_boxes_in_frame[gt_boxes_in_frame.object_id == row.OId].class_name.to_list()[0] in det_boxes_in_frame.loc[
                    row.HId].det_class:
                    results.loc[len(results)] = \
                    gt_boxes_in_frame[gt_boxes_in_frame.object_id == row.OId].to_dict('index')[
                        gt_boxes_in_frame[gt_boxes_in_frame.object_id == row.OId].index[0]] | \
                    det_boxes_in_frame.loc[row.HId].to_dict() | \
                    {"iou": 1 - row.D, "distance": row.D, "match_type": "TP"} | \
                    general_data.to_dict('index')[0]
                else:
                    # print("Miss match of classes")
                    results.loc[len(results)] = \
                        gt_boxes_in_frame[gt_boxes_in_frame.object_id == row.OId].to_dict('index')[
                            gt_boxes_in_frame[gt_boxes_in_frame.object_id == row.OId].index[0]] | \
                        {"iou": 1 - row.D, "distance": row.D, "match_type": "FN"} | \
                        general_data.to_dict('index')[0]

                    results.loc[len(results)] = det_boxes_in_frame.loc[row.HId].to_dict() | \
                                                {"iou": 1 - row.D, "distance": row.D, "match_type": "FP"} | \
                                                general_data.to_dict('index')[0]
            if row.Type == "MISS":
                results.loc[len(results)] = gt_boxes_in_frame[gt_boxes_in_frame.object_id == row.OId].to_dict('index')[
                                                gt_boxes_in_frame[gt_boxes_in_frame.object_id == row.OId].index[0]] | \
                                            {"iou": 1 - row.D, "distance": row.D, "match_type": "FN"} | \
                                            general_data.to_dict('index')[0]
            elif row.Type == 'FP':
                results.loc[len(results)] = det_boxes_in_frame.loc[row.HId].to_dict() | \
                                            {"iou": 1 - row.D, "distance": row.D, "match_type": "FP"} | \
                                            general_data.to_dict('index')[0]
    results["iou_threshold"] = 1 - iou_threshold
    return results, acc


# def match_detection_gt(gt_boxes, det_boxes, track_boxes):
#     tracker_object_match = pd.DataFrame(index=gt_boxes.object_id.unique(), columns=["track_id"])
#     if "index" not in gt_boxes:
#         gt_boxes.index.names = ['index']
#         gt_boxes = gt_boxes.reset_index()
#
#     results = define_new_columns(gt_boxes)
#
#     matched_gt_det_flag = False
#     det_boxes.reset_index(inplace=True)
#     track_boxes.reset_index(inplace=True)
#     for frame_id in tqdm(gt_boxes.frame_id.unique(), desc="Matching GT to Detections"):
#         for i, gt_box in gt_boxes[gt_boxes.frame_id == frame_id].iterrows():
#             best_iou = 0
#
#             for j, det_box in det_boxes[det_boxes.frame_id == frame_id].iterrows():
#                 if not det_boxes[det_boxes.frame_id == frame_id].empty and det_box[["x1", "y1", "x2", "y2"]].sum() > 0:
#                     iou, _ = bb_iou_score(gt_box[["x1", "y1", "x2", "y2"]], det_box[["x1", "y1", "x2", "y2"]])
#                     if iou > best_iou:
#                         results.loc[
#                             results["index"] == gt_box["index"], ["det_class", "det_x1", "det_y1", "det_x2",
#                                                                   "det_y2", "iou"]] = [
#                             *det_box[["class", "x1", "y1", "x2", "y2"]].values, iou]
#                         matched_gt_det_flag = True
#
#             if matched_gt_det_flag:
#                 det_boxes = det_boxes.drop(det_box["index"], errors='ignore')
#                 tracks_in_frame = track_boxes[track_boxes.frame_id == frame_id]
#                 if not tracks_in_frame.empty:
#                     index = get_closest_distance(gt_box[["x1", "y1", "x2", "y2"]],
#                                                  tracks_in_frame[["x1", "y1", "x2", "y2"]])
#
#                     if tracker_object_match.loc[gt_box.object_id].isna()[0]:
#                         iou, _ = bb_iou_score(gt_box[["x1", "y1", "x2", "y2"]],
#                                               tracks_in_frame.loc[index, ["x1", "y1", "x2", "y2"]])
#                         if iou > 0:
#                             tracker_object_match.loc[gt_box.object_id] = tracks_in_frame.loc[index].track_id
#                             results.loc[
#                                 results["index"] == gt_box["index"], ["track_id", "track_x1", "track_y1", "track_x2",
#                                                                       "track_y2"]] = tracks_in_frame.loc[
#                                 index, ["track_id", "x1", "y1", "x2", "y2"]].values
#
#                             tracks_in_frame.drop(index, inplace=True)
#                             track_boxes.drop(index, inplace=True)
#
#                     elif tracker_object_match.loc[gt_box.object_id][0] == tracks_in_frame.loc[index].track_id:
#                         results.loc[
#                             results["index"] == gt_box["index"], ["track_id", "track_x1", "track_y1", "track_x2",
#                                                                   "track_y2"]] = tracks_in_frame.loc[
#                             index, ["track_id", "x1", "y1", "x2", "y2"]].values
#                         tracks_in_frame.drop(index, inplace=True)
#                         track_boxes.drop(index, inplace=True)
#                 matched_gt_det_flag = False
#
#     FN = results.iloc[results[results['iou'].isnull()].index]
#     results = results.drop(results[results['iou'].isnull()].index)
#     results = results.astype({"det_x1": "int", "det_y1": "int", "det_x2": "int", "det_y2": "int", "iou": "float"})
#     return results, det_boxes, FN, tracker_object_match


#
# def match_detection_gt_by_distance(gt_boxes, det_boxes):
#     results = define_new_columns(gt_boxes)
#     break_flag = False
#     det_boxes.reset_index(inplace=True)
#     for frame_id in gt_boxes.frame_id.unique():
#         for i, gt_box in gt_boxes[gt_boxes.frame_id == frame_id].iterrows():
#             best_iou = 0
#             for j, det_box in det_boxes[det_boxes.frame_id == frame_id].iterrows():
#                 if not det_boxes[det_boxes.frame_id == frame_id].empty:
#                     iou, _ = bb_iou_score(gt_box[["x1", "y1", "x2", "y2"]], det_box[["x1", "y1", "x2", "y2"]])
#                     if iou > best_iou:
#                         results.loc[
#                             results["Unnamed: 0"] == gt_box["Unnamed: 0"], ["det_class", "det_x1", "det_y1", "det_x2",
#                                                                             "det_y2", "iou"]] = [
#                             *det_box[["class", "x1", "y1", "x2", "y2"]].values, iou]
#                         break_flag = True
#                         break
#             if break_flag:
#                 det_boxes = det_boxes.drop(det_box["index"])
#                 break_flag = False
#
#     FN = results.iloc[results[results['iou'].isnull()].index]
#     results = results.drop(results[results['iou'].isnull()].index)
#     results = results.astype({"det_x1": "int", "det_y1": "int", "det_x2": "int", "det_y2": "int", "iou": "float"})
#     return results, det_boxes, FN


def define_new_columns(data,general_columns):
    data.loc[:,general_columns]=None
    data.loc[:, "det_class"] = None
    data.loc[:, "det_subclass"] = None
    data.loc[:, "det_x1"] = None
    data.loc[:, "det_y1"] = None
    data.loc[:, "det_x2"] = None
    data.loc[:, "det_y2"] = None
    data.loc[:, "det_area"] = None
    # data.loc[:, "track_x1"] = None
    # data.loc[:, "track_y1"] = None
    # data.loc[:, "track_x2"] = None
    # data.loc[:, "track_y2"] = None
    # data.loc[:, "det_score"] = None
    data.loc[:, "iou"] = None
    data.loc[:, "distance"] = None
    data.loc[:, "match_type"] = None
    # data.loc[:, "track_id"] = None
    return data


def define_options(args):
    if args.nx_ip != None:
        opt.set("nx_ip", args.nx_ip)
    if args.flow_id != None:
        opt.set("flow_id", args.flow_id)
    if args.video_context_id != None:
        opt.set("video_context_id", args.video_context_id)
    if args.create_video != None:
        opt.set("create_video", args.create_video)


def get_normalization_data(payload_code=11, wavelength=4):
    db_creds = {'user': 'chen-sheiner', 'password': '4kWY05mQQiRcr4BCFqG5',
                'host': 'database-poc.cgkbiipjug0o.eu-west-1.rds.amazonaws.com', 'db_name': 'SXDBPROD'}
    db_client = DB_Client(db_creds)
    quarry = """
    select  distinct bits,mean_norm,std_norm
    from dwh.sensor_normalization_values
    where dwh.sensor_normalization_values.payload_code = payload_code_value and wavelength_code=wavelength_code_value
    """

    quarry = quarry.replace("payload_code_value", str(payload_code))
    quarry = quarry.replace("wavelength_code_value", str(wavelength))

    normalization_data = db_client.sql_query_db(quarry)
    normalization_data.set_index("bits", inplace=True)
    return normalization_data


def main():
    print(f"Working on Video: {opt.video_context_id}")
    if opt.generate:
        gerneral_report_file = "results/General_report.csv"

        # getting the ground-truth boxes from csv, the format is {frame_id: [(class,x1,y1,x2,y2), ... ] }
        gt_boxes, frames_urls = get_box_from_gt_csv()


        # getting the detection boxes from csv, the format is {frame_id: [(class,x1,y1,x2,y2), ... ] }
        norm_data = get_normalization_data(gt_boxes.payload_code[0],gt_boxes.wavelength_code[0])
        flow=gt_boxes.wavelength[0]
        print(f"Video's Flow is: {flow}\n")
        detect_boxes, tracker_boxes, frames_folder = get_box_from_detection_csv(frames_urls, norm_data,flow)

        # getting rid of "dilemma_zone" annotations
        gt_boxes, general_data = clean_gt_boxes(gt_boxes,frames_folder)



        # gt_boxes["gt_bb_w"] = gt_boxes["x2"] - gt_boxes["x1"]
        # gt_boxes["gt_bb_h"] = gt_boxes["y2"] - gt_boxes["y1"]
        # gt_boxes["gt_bb_area"] = gt_boxes["gt_bb_w"] * gt_boxes["gt_bb_h"]

        final_result,acc_list = create_detection_gt_result(gt_boxes, detect_boxes,general_data)
        final_result.to_csv(f"results/{opt.video_context_id}_detector_report.csv")

        tracker_report=match_tracker_gt(gt_boxes,tracker_boxes)
        general_temp=general_data.loc[general_data.index.repeat(len(tracker_report))]
        general_temp.index = tracker_report.index
        tracker_report.loc[:,general_columns_names]=general_temp
        tracker_report.to_csv(f"results/{opt.video_context_id}_tracker_report.csv")
        #
        # if os.path.exists(gerneral_report_file):
        #     general = pd.read_csv(gerneral_report_file, index_col=False)
        #     # if not general.context_id.unique().__contains__(opt.video_context_id):
        #     #     print("Adding to General Report...")
        #     #     pd.concat([general, results], axis=0, ignore_index=True).to_csv(gerneral_report_file, index=False)
        #     # else:
        #     #     print("Results already in General Report...")
        # else:
        #     print("Creating General Report...")
        #     results.to_csv(gerneral_report_file)


    else:
        # results = pd.read_csv(f"results/{opt.video_context_id}_report.csv")

        frame_width = 640
        frame_height = 512
        frames_folder = f"data/{opt.video_context_id}/"

    if opt.create_video:
        print("Creating Video..")
        vid_fps = 15.0
        gt_color = (0, 255, 0)
        det_color = (255, 0, 0)
        fn_color = (255, 255, 0)
        fp_color = (255, 0, 100)
        out = cv2.VideoWriter(f'results/{opt.video_context_id}.avi',
                              cv2.VideoWriter_fourcc('M', 'J', 'P', 'G'),
                              vid_fps,
                              (frame_width, frame_height))

        for frame_name in tqdm(sorted(os.listdir(frames_folder)), desc="Creating Video.. "):
            frame = cv2.imread(f"{frames_folder}{frame_name}")
            # print(f"{frames_folder}{frame_name}")
            frame_ind = int(frame_name.split(".")[0])

            # Saving the gt and detections results for frame_num
            results_in_frame = results[results.frame_id == frame_ind]

            cv2.putText(frame, f"{frame_name}",
                        (0 + 20, 0 + 20),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 2)  # drawing IOU score per image

            for i, row in results_in_frame[results_in_frame.detection_category == "TP"].iterrows():
                cv2.rectangle(frame, (int(row.x1), int(row.y1)), (int(row.x2), int(row.y2)), gt_color, 2)
                if not (pd.isna(row.track_x1) and pd.isna(row.track_y1) and pd.isna(row.track_x2) and pd.isna(
                        row.track_y2)):
                    cv2.rectangle(frame, (int(row.track_x1), int(row.track_y1)), (int(row.track_x2), int(row.track_y2)),
                                  (100, 200, 255), 2)
                cv2.rectangle(frame, (int(row.det_x1), int(row.det_y1)), (int(row.det_x2), int(row.det_y2)), det_color,
                              2)
                cv2.putText(frame, "IoU: {:.2f}".format(row.iou),
                            (int(row.det_x1) - 20, int(row.det_y1) - 20),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, det_color, 2)  # drawing IOU score per image
                cv2.putText(frame, "{}".format(row.track_id),
                            (int(row.det_x1) - 20, int(row.det_y1) - 40),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (100, 200, 255), 2)  # drawing IOU score per image
            for i, row in results_in_frame[results_in_frame.detection_category == "FP"].iterrows():
                cv2.rectangle(frame, (int(row.det_x1), int(row.det_y1)), (int(row.det_x2), int(row.det_y2)), fp_color,
                              2)
            #
            for i, row in results_in_frame[results_in_frame.detection_category == "FN"].iterrows():
                cv2.rectangle(frame, (int(row.x1), int(row.y1)), (int(row.x2), int(row.y2)), fn_color, 2)

            out.write(frame)

        out.release()
        # Closes all the frames
        cv2.destroyAllWindows()


if __name__ == "__main__":
    args = parse_args()
    define_options(args)
    # Extracting relevant arguments

    # For now - comment this line to execute.
    # This will create detections csv file from the meerkat engine
    #  generate_detections_csv(nx_ip, path, flow, os.getcwd()+"/test.csv")
    videos_names = opt.video_context_id

    if isinstance(videos_names, list):
        for video_name in videos_names:
            opt.set("video_context_id", video_name)
            if "mwir" in opt.video_context_id.lower():
                opt.set("flow_id", "mwir")
            elif "swir" in opt.video_context_id.lower():
                opt.set("flow_id", "swir")
            elif "lwir" in opt.video_context_id.lower():
                opt.set("flow_id", "lwir")
            elif "flir" in opt.video_context_id.lower():
                opt.set("flow_id", "flir")
            else:
                opt.set("flow_id", "rgb")
            main()
    else:
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
