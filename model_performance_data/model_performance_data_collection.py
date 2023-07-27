import os
import time
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
import warnings

warnings.filterwarnings('ignore')

os.makedirs("data/", exist_ok=True)
os.makedirs("results/", exist_ok=True)

# MOT Metrics info:
# https://pub.towardsai.net/multi-object-tracking-metrics-1e602f364c0c
# https://github.com/cheind/py-motmetrics/tree/develop
Detection = namedtuple("Detection", ["image_path", "gt", "pred"])

tracker_metrics = ['idf1', 'idp', 'idr', 'recall', 'precision', 'num_unique_objects', 'mostly_tracked',
                   'partially_tracked', 'mostly_lost', 'num_false_positives', 'num_misses', 'num_switches',
                   'num_fragmentations', 'mota', 'motp', 'num_transfer', 'num_ascend', 'num_migrate', 'num_predictions',
                   'num_matches', 'num_frames', 'num_objects', 'num_detections', 'idfn', 'idtp', 'idfp']

general_columns_names = ['context_id', 'video_gk', 'bits', 'source_path',
                         'video_name', 'sky_condition', 'light_condition',
                         'light_intensity', 'landform', 'frame_shape_w', 'frame_shape_h', 'payload_code',
                         'wavelength_code', 'wavelength', 'sensor_terrain', 'target_terrain']


# ['num_frames', 'obj_frequencies', 'pred_frequencies', 'num_matches', 'num_switches', 'num_transfer',
#                'num_ascend', 'num_migrate', 'num_false_positives', 'num_misses', 'num_detections', 'num_objects',
#                'num_predictions', 'num_unique_objects', 'track_ratios', 'mostly_tracked', 'partially_tracked',
#                'mostly_lost', 'num_fragmentations', 'motp', 'mota', 'precision', 'recall', 'id_global_assignment',
#                'idfp', 'idfn', 'idtp', 'idp', 'idr', 'idf1']

def get_time() -> str:
    """
    get local current time in specific format
    :return: current time
    """
    time_info = time.localtime()
    date = str(time_info[2]) + '/' + str(time_info[1]) + '/' + str(time_info[0])
    time_now = str(time_info[3]) + ':' + str(time_info[4]) + ':' + str(time_info[5])
    return f"{date}_{time_now}"


start_time = get_time()


def parse_args():
    parser = argparse.ArgumentParser(description="IOU script")
    parser.add_argument('--video_context_id', required=False)
    parser.add_argument('--nx_ip', required=False)
    parser.add_argument("--create_video", required=False)
    return parser.parse_args()


def add_info_to_fail_file(video_id: str, reason: str):
    """
    Add error to the failed_file in order to not stop the process of multiple videos runs

    :param video_id: video context id
    :param reason: explanation of the error
    :return:
    """

    failed_file = "failed_videos.csv"
    time_now = get_time()
    if os.path.exists(failed_file):
        data = pd.read_csv(failed_file, index_col=0)
        data.loc[len(data)] = [start_time, time_now, video_id, reason]
        data.to_csv(failed_file)
    else:
        data = pd.DataFrame(columns=["initial script start time", "error time", "video_context_id", "reason"])
        data.loc[len(data)] = [start_time, time_now, video_id, reason]
        data.to_csv(failed_file)


def get_class_codes(db_client: object) -> dict:
    """
    get dictionary of the class code value  to class name. I know it can be done directly though the quarry

    :param db_client: client of the DB
    :return: dictionary of the class code to class name
    """
    quarry = """
    
    Select attribute_name,code,description
    from dwh.decode
    where attribute_name='class'
    order by code
    limit 100

    """
    class_code_data = db_client.sql_query_db(quarry)

    return class_code_data[["code", "description"]].set_index("code").to_dict()["description"]


def get_subclass_codes(db_client: object) -> dict:
    """
    get dictionary of the subclass code value  to class name.I know it can be done directly though the quarry

    :param db_client: client of the DB
    :return: dictionary of the subclass code to subclass name
    """

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
    """
    clean the dataframe of GT
    """
    class_df = get_class_codes(db_client)
    subclass_df = get_subclass_codes(db_client)
    gt['class_name'] = gt.class_code.apply(lambda x: class_df[x])
    gt['subclass_name'] = gt.subclass_code.apply(lambda x: subclass_df[x])
    gt = gt.drop_duplicates(subset=["frame_id", "object_id"])
    gt = gt.reset_index()
    gt.drop(columns=["index", "class_code", "subclass_code"], inplace=True, errors='ignore')
    gt.index.names = ['index']
    return gt


def get_data_no_context_id(video_id: str, gk: int, client: object) -> pd.DataFrame:
    """
    if video cant be found using the context id of allegro, try in VIDEOS DB

    :param video_id: video context id
    :param gk: gk value of the video
    :param client: DB client
    :return: GT data
    """

    query = """ 
                SELECT dwh.objects.class_code,dwh.objects.subclass_code,
                dwh.objects.coordinates,dwh.objects.frame_id,
                dwh.objects.video_gk, 
                dwh.videos.source_path,
                dwh.objects.object_id,
                dwh.videos.video_name,
                dwh.videos.bits,
                dwh.videos.payload_code,
                dwh.videos.wavelength_code,
                dwh.get_desc('wavelength',wavelength_code) as wavelength,
                dwh.get_desc('sky_condition',sky_condition_code) as sky_condition,
                dwh.get_desc('light_condition',light_condition_code) as light_condition,
                dwh.get_desc('light_intensity',light_intensity_code) as light_intensity,
                dwh.get_desc('landform',landform_code) as landform,
                dwh.get_desc('sensor_terrain',sensor_terrain_code) as sensor_terrain,
                dwh.get_desc('target_terrain',target_terrain_code) as target_terrain
                
                FROM (dwh.videos
                INNER JOIN dwh.objects ON dwh.videos.video_gk = dwh.objects.video_gk)
                
                
                where dwh.videos.video_gk='video_gk_id'
                and dwh.objects.shape_code=1
                order by frame_id

                """
    print("Getting GT data from Database using video name")
    query_gt = query.replace("video_gk_id", str(gk))

    data_gt = client.sql_query_db(query_gt)
    if data_gt.empty:
        print(f"No GT file in Database of name: {video_id}")
        add_info_to_fail_file(opt.video_context_id, f"No GT file in Database of name: {video_id}")
    data_gt.loc[:, "context_id"] = video_id
    return data_gt


def get_data(video_id: str) -> pd.DataFrame:
    """
    Get GT of the video.
    :param video_id: video context id
    :return: GT data
    """
    db_creds = {'user': 'chen-sheiner', 'password': '4kWY05mQQiRcr4BCFqG5',
                'host': 'database-poc.cgkbiipjug0o.eu-west-1.rds.amazonaws.com', 'db_name': 'SXDBPROD'}
    db_client = DB_Client(db_creds)
    gk_querry = """
    SELECT distinct video_gk, context_id
    from dwh.allegro_videos
    where context_id='video_context_id'
    """
    gk_querry = gk_querry.replace("video_context_id", video_id)
    gk_data = db_client.sql_query_db(gk_querry)
    if gk_data.empty:
        print(f"No Video in allegro_videos : {video_id}")
        add_info_to_fail_file(video_id, "Video Doesnt Exist in Allegro_videos")
        return None

    gk = gk_data.video_gk[0]
    print(f"Gathering GT of Video GK: {gk}")
    query = """ 
            SELECT dwh.objects.class_code,dwh.objects.subclass_code,
            dwh.objects.coordinates,dwh.objects.frame_id,
            dwh.allegro_videos.context_id,
            dwh.objects.video_gk, 
            dwh.videos.source_path,
            dwh.objects.object_id,
            dwh.videos.video_name,
			dwh.videos.bits,
			dwh.videos.payload_code,
			dwh.videos.wavelength_code,
			dwh.get_desc('wavelength',wavelength_code) as wavelength,
            dwh.get_desc('sky_condition',sky_condition_code) as sky_condition,
            dwh.get_desc('light_condition',light_condition_code) as light_condition,
            dwh.get_desc('light_intensity',light_intensity_code) as light_intensity,
            dwh.get_desc('landform',landform_code) as landform,
            dwh.get_desc('sensor_terrain',sensor_terrain_code) as sensor_terrain,
			dwh.get_desc('target_terrain',target_terrain_code) as target_terrain
            
            FROM ((dwh.allegro_videos
            
            INNER JOIN dwh.objects ON dwh.allegro_videos.video_gk = dwh.objects.video_gk)
            INNER JOIN dwh.videos ON dwh.allegro_videos.video_gk = dwh.videos.video_gk )
            
            where dwh.videos.video_gk='video_gk_id'
            and dwh.objects.shape_code=1
            order by frame_id
            """

    # limit 100

    if not os.path.exists(f"{os.getcwd()}/data/GT/{video_id}.csv"):
        print("Getting GT data from Database")
        query_gt = query.replace("video_gk_id", str(gk))

        data_gt = db_client.sql_query_db(query_gt)
        if data_gt.empty:
            print(f"No GT file in Database of context id: {video_id}")
            data_gt = get_data_no_context_id(video_id, gk, db_client)
        data_gt = clean_data_gt(data_gt, db_client)
        data_gt.to_csv(f"{os.getcwd()}/data/GT/{video_id}.csv")
        data_gt = pd.read_csv(f"{os.getcwd()}/data/GT/{video_id}.csv")
    else:
        print("GT CSV Already Exist")
        data_gt = pd.read_csv(f"{os.getcwd()}/data/GT/{video_id}.csv")
    return data_gt


def get_box_from_gt_csv():
    data_gt = get_data(opt.video_context_id)
    if data_gt is None:
        return None, None
    data_gt.coordinates = data_gt.coordinates.apply(eval)  # TODO add rename to motorcycle to two-wheel subclass
    data_gt.coordinates = data_gt.coordinates.apply(lambda x: np.concatenate(x[0:3:2]))
    data_gt[["x1", "y1", "x2", "y2"]] = pd.DataFrame(data_gt.coordinates.tolist(), index=data_gt.index)
    data_gt[["x1", "y1", "x2", "y2"]] = data_gt[["x1", "y1", "x2", "y2"]].astype(int)
    return data_gt, data_gt.source_path[0:1]


def clean_gt_boxes(gt_boxes: pd.DataFrame, frames_folder: str) -> tuple[pd.DataFrame]:
    """

    :param gt_boxes: GT data
    :param frames_folder: path of the frames directory
    :return: GT data & General data of the video
    """
    drop_indx = gt_boxes[gt_boxes.class_name == "dilemma_zone"].index
    gt_boxes = gt_boxes.drop(drop_indx, errors='ignore')
    gt_boxes.drop(columns="index", inplace=True, errors='ignore')

    frame = os.listdir(frames_folder)[0]
    demo_img = cv2.imread(f"{frames_folder}{frame}")
    if not isinstance(demo_img, np.ndarray):
        demo_img = cv2.imread(f"{frames_folder}/{frame}")

    frame_width = int(demo_img.shape[1])
    frame_height = int(demo_img.shape[0])
    gt_boxes["frame_shape_w"] = frame_width
    gt_boxes["frame_shape_h"] = frame_height

    general_data = gt_boxes[general_columns_names].drop_duplicates()
    if general_data.shape[0] != 1:
        print("Video configuration change mid video")
        add_info_to_fail_file(opt.video_context_id, "Video configuration change mid video")
    gt_boxes.drop(columns=general_columns_names, inplace=True, errors='ignore')
    return gt_boxes, general_data


def download_frames(urls: list[str]) -> str:
    """
    Download Frames from S3
    :param urls:  list of strings of the urls of the folder of frames in S3
    :return: path to the downloaded frames
    """
    s3 = S3_Client({})
    for url in urls:
        if isinstance(url, str):
            bucket = url.split("//")[1].split("/")[0]
            key = url.split("//")[1].split(bucket)[1]
            # folder_name = key.split("/")[-1]
            os.makedirs(f"data/{opt.video_context_id}/", exist_ok=True)
            files_list = s3.get_files_in_folder(bucket, key[1:])
            for file in tqdm(files_list, desc="Downloading Frames"):
                if not os.path.exists(
                        f"data/{opt.video_context_id}/{file.split('/')[-1]}"):  # if file exist don't download it
                    s3.download_file(bucket, file,
                                     f"data/{opt.video_context_id}/{file.split('/')[-1]}")
    return f"data/{opt.video_context_id}/"


def clean_tracker_data(data: pd.DataFrame):
    """
    clean the tracking data
    :param data: data of the tracker output
    :return:
    """
    try:
        data[["x1", "y1", "x2", "y2"]] = data[["x1", "y1", "x2", "y2"]].astype(pd.Int32Dtype())
    except Exception as e:
        print("Tracker  File Has Empty lines:", e)
    data = data[data[["x1", "y1", "x2", "y2"]].sum(axis=1) > 0]
    return data


def generate_tracker_csv(sdk: str, ip: str, video_path: str,
                         flow_id: str, terrain: str, output_csv_path: str,
                         pixel_mean: float, pixel_std: float, bit: int):
    """
    Run SDK for Tracker
    :param sdk: 'merlin' or 'meerkat'
    :param ip: IP of the NX as a string: 'XXX:XXX:XXX:XXX'
    :param video_path: path to the frames directory
    :param flow_id: flow matching to the video that was chosen
    :param terrain: ground or sea
    :param output_csv_path: output path of the detection/tracking
    :param pixel_mean: value of the normalization mean
    :param pixel_std: value of the normalization std
    :param bit: is the video 8 bit or 16 bit
    :return:
    """
    if not os.path.exists(output_csv_path) or os.stat(output_csv_path).st_size < 700:
        print(f"command: \nsingle_frame_{sdk}/./sdk_sample_raw_frames {ip} {video_path} {flow_id} {terrain} {output_csv_path} {pixel_mean} {pixel_std} {bit}")
        start = perf_counter()
        os.system(
            f"single_frame_{sdk}/./sdk_sample_raw_frames {ip} {video_path} {flow_id} {terrain} {output_csv_path} {pixel_mean} {pixel_std} {bit}")
        print("Tracker time:", perf_counter() - start)
    else:
        print("Tracker CSV Already Exist")
    # If file is lower than 700 B than its empty and the script runs the SDK again
    if os.stat(output_csv_path).st_size < 700:
        print("Trying to Create Tracker File again")
        os.system(
            f"single_frame_{sdk}/./sdk_sample_raw_frames {ip} {video_path} {flow_id} {terrain} {output_csv_path} {pixel_mean} {pixel_std} {bit}")


def generate_detections_csv(sdk: str, ip: str, video_path: str,
                            flow_id: str, terrain: str, output_csv_path: str,
                            pixel_mean: float, pixel_std: float, bit: int):
    """
    Run SDK for Detections based on the arguments passed to the function:
    :param sdk: 'merlin' or 'meerkat'
    :param ip: IP of the NX as a string: 'XXX:XXX:XXX:XXX'
    :param video_path: path to the frames directory
    :param flow_id: flow matching to the video that was chosen
    :param terrain: ground or sea
    :param output_csv_path: output path of the detection/tracking
    :param pixel_mean: value of the normalization mean
    :param pixel_std: value of the normalization std
    :param bit: is the video 8 bit or 16 bit
    :return:

    """
    if not os.path.exists(output_csv_path) or os.stat(output_csv_path).st_size < 700:
        start = perf_counter()
        print(f"command: \nsingle_frame_{sdk}/./sdk_sample_single_frame {ip} {video_path} {flow_id} {terrain} {output_csv_path} {pixel_mean} {pixel_std} {bit}")

        os.system(
            f"single_frame_{sdk}/./sdk_sample_single_frame {ip} {video_path} {flow_id} {terrain} {output_csv_path} {pixel_mean} {pixel_std} {bit}")
        print("Detections time:", perf_counter() - start)
    else:
        print("Detections CSV Already Exist")
    # If file is lower than 700 B than its empty and the script runs the SDK again
    if os.stat(output_csv_path).st_size < 700:
        print("Trying to Create Detection File again")
        os.system(
            f"single_frame_{sdk}/./sdk_sample_single_frame {ip} {video_path} {flow_id} {terrain} {output_csv_path} {pixel_mean} {pixel_std} {bit}")


def get_output_from_sdk(norm_values: pd.DataFrame, sdk: str, flow: str, terrain: str, bit: int,
                        run_detector: bool = True) -> pd.DataFrame:
    """
    Run SDK (tracker or Detections) and get the output data

    :param norm_values: normalization values
    :param sdk:  'merlin' or 'meerkat'
    :param flow: flow matching to the video that was chosen
    :param terrain: ground or sea
    :param bit: is the video 8 bit or 16 bit
    :param run_detector: True = run only detection, False = run only tracker
    :return: output of the sdk
    """

    video_path = opt.video_context_id
    if not video_path.endswith('/'):
        video_path += '/'
    video_path = f"{os.getcwd()}/data/{video_path}"
    extension = os.listdir(video_path)[0].split(".")[1]
    print(f"extention of images:{extension}")
    try:
        values = norm_values.loc[str(bit)]

    except KeyError:
        print("no correct type of compression in normalization-values database.")
        print("using the first configuration as values:\n", norm_values.iloc[:1])
        values = norm_values.iloc[0]
    ip = opt.meerkat_ip if sdk == "meerkat" else opt.merlin_ip
    if run_detector:
        csv_path_detection = f"{os.getcwd()}/data/detections/{opt.video_context_id}_detections.csv"
        generate_detections_csv(sdk, ip, video_path, flow, terrain, csv_path_detection, values[0], values[1],
                                bit)
        if not os.path.exists(csv_path_detection) or os.stat(csv_path_detection).st_size <= 700:
            print("Detection File Creation Failed")
            add_info_to_fail_file(opt.video_context_id, "Detection File Creation Failed")
            return None
        detections_data = pd.read_csv(csv_path_detection)
        return detections_data

    else:
        csv_path_tracker = f"{os.getcwd()}/data/trackers/{opt.video_context_id}_tracker.csv"
        video_path += f"%05d.{extension}"
        generate_tracker_csv(sdk, ip, video_path, flow, terrain, csv_path_tracker, values[0], values[1], bit)
        if not os.path.exists(csv_path_tracker) or os.stat(csv_path_tracker).st_size <= 700:
            print("Detection File Creation Failed")
            add_info_to_fail_file(opt.video_context_id, "Detection File Creation Failed")
            return None
        tracker_data = pd.read_csv(csv_path_tracker)
        tracker_data = clean_tracker_data(tracker_data)
        return tracker_data


def match_tracker_gt(gt_boxes: pd.DataFrame, track_boxes: pd.DataFrame) -> pd.DataFrame:
    """
    Match Tracker and GT on 10 different IOU threshold
    """
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
    tdi = []  # Target Detection Indicator
    total_tracks_opened = []  # total_trackes_opened
    total_tracks_opened_overall = []  # total_trackes_opened per frame
    total_false_possitives = []  # total_false_possitives per frame

    for acc in acc_list:
        data = acc.mot_events.groupby('OId').sum()
        tdi.append(len(len(data) - data[data == 0].dropna()))
        total_tracks_opened.append(len(acc.mot_events.HId.dropna().unique()))
        tracks_opened_per_frame = []  # total_trackes_opened per frame
        for frame in acc.mot_events.index.get_level_values("FrameId").unique():
            data = acc.mot_events.loc[frame].groupby("HId").count()
            tracks_opened_per_frame.append(len(data[data != 0].dropna()))
        total_tracks_opened_overall.append(sum(tracks_opened_per_frame))
        total_false_possitives.append(acc.mot_events[acc.mot_events.Type == "FP"].shape[0])
    summary["MOP"] = np.divide(np.multiply(total_tracks_opened_overall, 100),
                               np.add(gt_boxes.shape[0], total_false_possitives))
    summary["TDI"] = tdi
    summary["total_tracks_opened"] = total_tracks_opened
    summary["PD"] = summary["TDI"] / summary["num_unique_objects"]
    summary["FAR"] = summary["num_false_positives"] / summary["total_tracks_opened"]

    summary.index.name = 'iou_threshold'
    return summary


def mot_metric_calc(gt_data: pd.DataFrame, tracker_data: pd.DataFrame, iou_threshold: float = 0.5) -> mm.MOTAccumulator:
    """
    Create GT and Tracker matching per IOU threshold
    """
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


def create_detection_gt_result(gt_data: pd.DataFrame, det_data: pd.DataFrame,
                               general_data: pd.DataFrame) -> pd.DataFrame:
    """
    Create Detections Report which include 10 different matching of GT and detections base on IOU Threshold
    """
    final_result = pd.DataFrame()
    acc_list = []
    for iou_threshold in np.arange(0, 1, 0.1):
        # for iou_threshold in [0.7]:
        result, acc = match_detection_gt_v2(gt_data, det_data, general_data,
                                            iou_threshold=np.round(iou_threshold, decimals=1))
        if result is None:
            return None, None
        acc_list.append(acc)
        final_result = pd.concat([final_result, result])

    return final_result, acc_list


def match_detection_gt_v2(gt_data: pd.DataFrame, det_data: pd.DataFrame, general_data: pd.DataFrame,
                          iou_threshold: float = 0.5) -> pd.DataFrame:
    """
    Match gt objects to detections. using motmetrics library.

    """
    det_data = det_data.dropna(subset=['x1', 'x2', 'y1', 'y2'])
    if det_data.empty:
        print("Model found no detections")
        add_info_to_fail_file(opt.video_context_id, "Model found no detections")
        return None, None
    gt_data["h"] = gt_data.apply(lambda x: int(x["y2"] - x["y1"]), axis=1)
    gt_data["w"] = gt_data.apply(lambda x: int(x["x2"] - x["x1"]), axis=1)
    gt_data["gt_area"] = gt_data["w"] * gt_data["h"]

    det_data["det_h"] = det_data.apply(lambda x: int(x["y2"] - x["y1"]), axis=1)
    det_data["det_w"] = det_data.apply(lambda x: int(x["x2"] - x["x1"]), axis=1)
    det_data["det_area"] = det_data["det_w"] * det_data["det_h"]
    det_data.rename(
        columns={'class': 'det_class', 'x1': 'det_x1', 'y1': 'det_y1', 'x2': 'det_x2', 'y2': 'det_y2', }, inplace=True)
    results = define_new_columns(gt_data, general_data.columns).iloc[:0, :].copy()

    acc = mm.MOTAccumulator(auto_id=False)
    for frame_id in tqdm(gt_data.frame_id.unique(), desc=f"Matching GT to Detections @iou={iou_threshold:.1f}"):
        det_boxes_in_frame = det_data[det_data.frame_id == frame_id]
        gt_boxes_in_frame = gt_data[gt_data.frame_id == frame_id]

        if det_boxes_in_frame.empty:
            for i, row in gt_boxes_in_frame.iterrows():
                results.loc[len(results)] = row.to_dict() | \
                                            {"match_type": "FN"} | \
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
        matched.drop_duplicates(subset=["OId", "HId"], inplace=True)

        for i, row in matched.iterrows():
            if row.Type == "MATCH" or row.Type == "ASCEND" or row.Type == "SWITCH" or row.Type == "TRANSFER" or row.Type == "MIGRATE":
                if gt_boxes_in_frame[gt_boxes_in_frame.object_id == row.OId].class_name.to_list()[0] in \
                        det_boxes_in_frame.loc[
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


def define_new_columns(data, general_columns):
    """
    Add empty columns in DF
    """
    data.loc[:, general_columns] = None
    data.loc[:, "det_class"] = None
    data.loc[:, "det_subclass"] = None
    data.loc[:, "det_x1"] = None
    data.loc[:, "det_y1"] = None
    data.loc[:, "det_x2"] = None
    data.loc[:, "det_y2"] = None
    data.loc[:, "det_area"] = None
    data.loc[:, "iou"] = None
    data.loc[:, "distance"] = None
    data.loc[:, "match_type"] = None
    return data


def define_options(args):
    """
    Sync argparse arguments and setting file arguments.

    """
    # if args.nx_ip != None:
    #     opt.set("nx_ip", args.nx_ip)
    if args.video_context_id != None:
        opt.set("video_context_id", args.video_context_id)
    if args.create_video != None:
        opt.set("create_video", args.create_video)


def get_normalization_data(payload_code: int = 11, wavelength: int = 4) -> pd.DataFrame:
    """
    get normalization values for the video
    :param payload_code: code number of the payload
    :param wavelength: code number of the wavelength
    :return:
    """
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
    print(normalization_data)
    # normalization_dat=pd.DataFrame({"mean_norm":0.425823 ,"std_norm":0.00713729},index=['16'])
    # normalization_dat.index.name = 'bits'
    return normalization_data


def set_options(options):
    opt.set("video_context_id", options.video_context_id)
    opt.set("run_sdk", options.run_sdk)
    opt.set("generate", options.generate)
    opt.set("create_video", options.create_video)
    opt.set("merlin_ip", options.merlin_ip)
    opt.set("meerkat_ip", options.meerkat_ip)


def run_script():
    print(f"Working on Video: {opt.video_context_id}")
    if opt.generate:
        gerneral_report_file = "results/General_report.csv"

        # getting the ground-truth boxes from csv, the format is {frame_id: [(class,x1,y1,x2,y2), ... ] }
        gt_boxes, frames_urls = get_box_from_gt_csv()
        if gt_boxes is None:
            return

            # getting the detection boxes from csv, the format is {frame_id: [(class,x1,y1,x2,y2), ... ] }
        norm_data = get_normalization_data(gt_boxes.payload_code[0], gt_boxes.wavelength_code[0])
        flow = gt_boxes.wavelength[0]
        print(f"\nVideo's Flow is: {flow}")
        bit = gt_boxes.bits[0]
        print(f"Video's Bit is: {bit}")
        terrain = gt_boxes.target_terrain[0]
        print(f"Video's Terrain is: {terrain}")
        sdk = "merlin" if gt_boxes.sensor_terrain[0] == "air" else "meerkat"
        print(f"Video's Sdk is: {sdk}\n")
        # Make sure / Download frames of the chosen video
        frames_folder = download_frames(frames_urls)

        # getting rid of "dilemma_zone" annotations
        gt_boxes, general_data = clean_gt_boxes(gt_boxes, frames_folder)

        if opt.run_sdk in ["both", "detector"]:
            # Run detector sdk
            detect_boxes = get_output_from_sdk(norm_data, sdk, flow, terrain, bit, run_detector=True)
            if detect_boxes is None:
                return
            final_result, acc_list = create_detection_gt_result(gt_boxes, detect_boxes, general_data)
            if final_result is None:
                return
            final_result.to_csv(f"results/{opt.video_context_id}_detector_report.csv")

        if opt.run_sdk in ["both", "tracker"]:
            # Run tracker sdk
            tracker_boxes = get_output_from_sdk(norm_data, sdk, flow, terrain, bit, run_detector=False)
            if tracker_boxes is None:
                return
            tracker_report = match_tracker_gt(gt_boxes, tracker_boxes)
            general_temp = general_data.loc[general_data.index.repeat(len(tracker_report))]
            general_temp.index = tracker_report.index
            tracker_report.loc[:, general_columns_names] = general_temp
            tracker_report.to_csv(f"results/{opt.video_context_id}_tracker_report.csv")



    else:
        frames_folder = f"data/{opt.video_context_id}/"

    if opt.create_video:
        print("Creating Video..")
        vid_fps = 15.0
        gt_color = (0, 255, 0)
        det_color = (255, 0, 0)
        fn_color = (255, 255, 0)
        fp_color = (255, 0, 100)
        sleep(1)
        gt = pd.read_csv(f"data/GT/{opt.video_context_id}.csv")
        det = pd.read_csv(f"data/detections/{opt.video_context_id}_detections.csv")
        tracker = pd.read_csv(f"data/trackers/{opt.video_context_id}_tracker.csv")

        det = det.dropna()
        tracker = tracker.dropna()

        gt.coordinates = gt.coordinates.apply(eval)
        gt.coordinates = gt.coordinates.apply(lambda x: np.concatenate(x[0:3:2]))
        gt[["x1", "y1", "x2", "y2"]] = pd.DataFrame(gt.coordinates.tolist(), index=gt.index)
        gt[["x1", "y1", "x2", "y2"]] = gt[["x1", "y1", "x2", "y2"]].astype(int)

        frame_demo = cv2.imread(f"{frames_folder}/{os.listdir(frames_folder)[0]}")
        frame_height, frame_width, _ = frame_demo.shape

        out = cv2.VideoWriter(f'results/{opt.video_context_id}.avi',
                              cv2.VideoWriter_fourcc('M', 'J', 'P', 'G'),
                              vid_fps,
                              (frame_width, frame_height))

        for frame_name in tqdm(sorted(os.listdir(frames_folder)), desc="Creating Video.. "):
            frame = cv2.imread(f"{frames_folder}{frame_name}")
            # print(f"{frames_folder}{frame_name}")
            frame_ind = int(frame_name.split(".")[0])

            # Saving the gt and detections results for frame_num
            gt_in_frame = gt[gt.frame_id == frame_ind]
            det_in_frame = det[det.frame_id == frame_ind]
            tracker_in_frame = tracker[tracker.frame_id == frame_ind]

            cv2.putText(frame, f"{frame_name}",
                        (0 + 20, 0 + 20),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 2)  # drawing IOU score per image
            if not gt_in_frame.empty:
                for i, row in gt_in_frame.iterrows():
                    cv2.rectangle(frame, (int(row.x1), int(row.y1)), (int(row.x2), int(row.y2)), gt_color, 2)

            if not det_in_frame.empty:
                for i, row in det_in_frame.iterrows():
                    cv2.rectangle(frame, (int(row.x1), int(row.y1)), (int(row.x2), int(row.y2)), det_color, 2)
            if not tracker_in_frame.empty:
                for i, row in tracker_in_frame.iterrows():
                    cv2.rectangle(frame, (int(row.x1), int(row.y1)), (int(row.x2), int(row.y2)), fn_color, 2)
                    cv2.putText(frame, "ID: {:.0f}".format(row.track_id),
                                (int(row.x1) - 20, int(row.y1) - 20),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.5, fn_color, 2)
            out.write(frame)

        out.release()
        # Closes all the frames
        cv2.destroyAllWindows()


def main(option=None):
    if option is not None:
        set_options(option)
        videos_names = opt.video_context_id

        for video_name in videos_names:
            opt.set("video_context_id", video_name)
            run_script()


if __name__ == "__main__":
    args = parse_args()
    define_options(args)
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
            run_script()
    else:
        run_script()

# The folder we are going to zip is include the followings:
#   * model_performance_data_collection.py
#   * single frame folder with:
#       - c++ file
#       - CMakeList file
#       - empty build folder: initial state it will be empty, and we build all.
#                             in case of already built, (i.e. the folder isn't
#                             empty) - we won't build and run the exe file
# The flow process:
# 1.generate gt.csv from DB
# 2.download frames form S3
# 3.generate detections.csv
# 4.generate tracker.csv
# 5.match detection and gt
# 6.create detection_report.csv
# 7.match tracker and gt
# 8.create tracker_report.csv
# 9.if configure, create video of tracks and detections
