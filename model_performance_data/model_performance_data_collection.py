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
from time import perf_counter
# TODO chen change paths to new name
Detection = namedtuple("Detection", ["image_path", "gt", "pred"])


# TODO - when im done with all, re-write the parse_args function
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
    gt=gt.drop_duplicates(subset=["frame_id","object_id"])
    gt=gt.reset_index()
    gt.drop(columns=["index","class_code","subclass_code"], inplace=True)
    gt.index.names = ['index']
    return gt

def get_data_no_context_id(video_id,client):
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
    data_gt.loc[:,"context_id"]=video_id
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
            dwh.videos.video_name
            
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
        if data_gt.empty :
            print(f"No GT file in Database of context id: {video_id}")
            data_gt=get_data_no_context_id(video_id,db_client)
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
    return data_gt, data_gt.source_path[0:1]


def clean_gt_boxes(gt_boxes):
    drop_indx = gt_boxes[gt_boxes.class_name == "dilemma_zone"].index
    gt_boxes = gt_boxes.drop(drop_indx)
    return gt_boxes


def download_frames(urls):
    s3 = S3_Client({})
    for url in urls:
        if isinstance(url, str):
            bucket = url.split("//")[1].split("/")[0]
            key = url.split("//")[1].split(bucket)[1]
            folder_name = key.split("/")[-1]
            os.makedirs(f"data/{folder_name}/", exist_ok=True)
            files_list = s3.get_files_in_folder(bucket, key[1:])
            for file in tqdm(files_list, desc="Downloading Frames"):
                if not os.path.exists(f"data/{folder_name}/{file.split('/')[-1]}"):  # if file exist don't download it
                    s3.download_file(bucket, file,
                                     f"data/{folder_name}/{file.split('/')[-1]}")  # TODO add functionality to download whole folder and not just file as expeced now
    return f"data/{folder_name}/"


def generate_detections_csv(ip, video_path, flow_id, output_csv_path,pixel_mean,pixel_std):
    print("mean std" ,pixel_mean ,pixel_std)
    if not os.path.exists(output_csv_path):
        start=perf_counter()
        os.system(
            f"single_frame/./sdk_sample_single_frame {ip} {video_path} {flow_id} {output_csv_path} {pixel_mean} {pixel_std}")
        print("detection time:",perf_counter()-start)
    else:
        print("Detections CSV Already Exist")
        if os.stat(output_csv_path).st_size<10:
            print("Trying to Create Detection File again")
            os.system(
                f"single_frame/./sdk_sample_single_frame {ip} {video_path} {flow_id} {output_csv_path}")

def get_box_from_detection_csv(urls,norm_values):
    frames_folder = download_frames(urls)
    video_path = opt.video_context_id
    if not video_path.endswith('/'):
        video_path += '/'
    video_path = f"{os.getcwd()}/data/{video_path}"

    csv_path = f"{os.getcwd()}/data/detections/{opt.video_context_id}_detections.csv"
    if "8bit" in video_path:
        values=norm_values.loc['8']
    else:
        values = norm_values.loc['16']
    generate_detections_csv(opt.nx_ip, video_path, opt.flow_id, csv_path,values[0],values[1])
    assert os.path.exists(csv_path) and  os.stat(csv_path).st_size>10, "Detection File Creation Failed"
    detections_data = pd.read_csv(csv_path)
    try:
        detections_data[["x1", "y1", "x2", "y2"]] = detections_data[["x1", "y1", "x2", "y2"]].astype(pd.Int32Dtype())
    except Exception as e:
        print("Detection File Has Empty lines:" , e)

    return detections_data, frames_folder


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


def match_detection_gt(gt_boxes, det_boxes):
    if "index" not in gt_boxes:
        gt_boxes.index.names = ['index']
        gt_boxes=gt_boxes.reset_index()


    results = define_new_columns(gt_boxes)


    break_flag = False
    det_boxes.reset_index(inplace=True)
    for frame_id in tqdm(gt_boxes.frame_id.unique(),desc="Matching GT to Detections"):
        for i, gt_box in gt_boxes[gt_boxes.frame_id == frame_id].iterrows():
            best_iou = 0

            for j, det_box in det_boxes[det_boxes.frame_id == frame_id].iterrows():
                if not det_boxes[det_boxes.frame_id == frame_id].empty and det_box[["x1", "y1", "x2", "y2"]].sum() > 0:
                    iou, _ = bb_iou_score(gt_box[["x1", "y1", "x2", "y2"]], det_box[["x1", "y1", "x2", "y2"]])
                    if iou > best_iou:
                        results.loc[
                            results["index"] == gt_box["index"], ["det_class", "det_x1", "det_y1", "det_x2",
                                                                            "det_y2", "iou"]] = [
                            *det_box[["class", "x1", "y1", "x2", "y2"]].values, iou]
                        break_flag = True
                        break
            if break_flag:
                det_boxes = det_boxes.drop(det_box["index"])
                break_flag = False

    FN = results.iloc[results[results['iou'].isnull()].index]
    results = results.drop(results[results['iou'].isnull()].index)
    results = results.astype({"det_x1": "int", "det_y1": "int", "det_x2": "int", "det_y2": "int", "iou": "float"})
    return results, det_boxes,FN

def match_detection_gt_by_distance(gt_boxes, det_boxes):
    results = define_new_columns(gt_boxes)
    break_flag = False
    det_boxes.reset_index(inplace=True)
    for frame_id in gt_boxes.frame_id.unique():
        for i, gt_box in gt_boxes[gt_boxes.frame_id == frame_id].iterrows():
            best_iou = 0
            for j, det_box in det_boxes[det_boxes.frame_id == frame_id].iterrows():
                if not det_boxes[det_boxes.frame_id == frame_id].empty:
                    iou, _ = bb_iou_score(gt_box[["x1", "y1", "x2", "y2"]], det_box[["x1", "y1", "x2", "y2"]])
                    if iou > best_iou:
                        results.loc[
                            results["Unnamed: 0"] == gt_box["Unnamed: 0"], ["det_class", "det_x1", "det_y1", "det_x2",
                                                                            "det_y2", "iou"]] = [
                            *det_box[["class", "x1", "y1", "x2", "y2"]].values, iou]
                        break_flag = True
                        break
            if break_flag:
                det_boxes = det_boxes.drop(det_box["index"])
                break_flag = False

    FN = results.iloc[results[results['iou'].isnull()].index]
    results = results.drop(results[results['iou'].isnull()].index)
    results = results.astype({"det_x1": "int", "det_y1": "int", "det_x2": "int", "det_y2": "int", "iou": "float"})
    return results, det_boxes,FN





def define_new_columns(data):


    data.loc[:, "det_class"] = None
    data.loc[:, "det_subclass"] = None
    data.loc[:, "det_x1"] = None
    data.loc[:, "det_y1"] = None
    data.loc[:, "det_x2"] = None
    data.loc[:, "det_y2"] = None
    data.loc[:, "det_score"] = None  # TODO to be added
    data.loc[:, "iou"] = None
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

def get_normalization_data(payload_code=11,wavelength=4):
    db_creds = {'user': 'chen-sheiner', 'password': '4kWY05mQQiRcr4BCFqG5',
                'host': 'database-poc.cgkbiipjug0o.eu-west-1.rds.amazonaws.com', 'db_name': 'SXDBPROD'}
    db_client = DB_Client(db_creds)
    quarry = """
    select  distinct bits,mean_norm,std_norm
    from dwh.sensor_normalization_values
    where dwh.sensor_normalization_values.payload_code = payload_code_value and wavelength_code=wavelength_code_value
    """
    quarry = quarry.replace("payload_code_value",payload_code)
    quarry = quarry.replace("wavelength_code_value", wavelength)

    normalization_data = db_client.sql_query_db(quarry)
    normalization_data.set_index("bits",inplace=True)
    return normalization_data


def main():
    print(f"Working on Video: {opt.video_context_id}")
    if opt.generate:
        gerneral_report_file="results/General_report.csv"

        # getting the ground-truth boxes from csv, the format is {frame_id: [(class,x1,y1,x2,y2), ... ] }
        gt_boxes, frames_urls = get_box_from_gt_csv()

        # getting rid of "dilemma_zone" annotations
        gt_boxes = clean_gt_boxes(gt_boxes)

        # getting the detection boxes from csv, the format is {frame_id: [(class,x1,y1,x2,y2), ... ] }
        norm_data=get_normalization_data()
        detect_boxes, frames_folder = get_box_from_detection_csv(frames_urls,norm_data)
        # assert detect_boxes.x1.sum()!=0, "No Objects in Video"

        frame = os.listdir(frames_folder)[0]
        demo_img=cv2.imread(f"{frames_folder}{frame}")
        frame_width = int(demo_img.shape[1])
        frame_height = int(demo_img.shape[0])
        gt_boxes[ "frame_shape_w"] = frame_width
        gt_boxes[ "frame_shape_h"] = frame_height
        gt_boxes[ "gt_bb_w"] = gt_boxes["x2"]-gt_boxes["x1"]
        gt_boxes[ "gt_bb_h"] = gt_boxes["y2"]-gt_boxes["y1"]
        gt_boxes["gt_bb_area"]=gt_boxes[ "gt_bb_w"] * gt_boxes[ "gt_bb_h"]


        results, FP ,FN= match_detection_gt(gt_boxes, detect_boxes)
        # in order to save result video
        FP[ "det_bb_w"] = FP["x2"]-FP["x1"]
        FP[ "det_bb_h"] = FP["y2"]-FP["y1"]
        FP["det_bb_area"] = FP["det_bb_w"] * FP["det_bb_h"]

        results[ "det_bb_w"] = results["det_x2"]-results["det_x1"]
        results[ "det_bb_h"] = results["det_y2"]-results["det_y1"]
        results["det_bb_area"] = results["det_bb_w"] * results["det_bb_h"]
        FN.loc[:, "detection_category"] = "FN"
        FP.loc[:, "detection_category"] = "FP"
        if not results.empty:
            results.loc[:, "detection_category"] = "TP"

        FP = FP.rename(columns={"frame_id": "frame_id"})
        FP = FP.rename(columns={"x1": "det_x1", "x2": "det_x2", "y1": "det_y1", "y2": "det_y2", "class": "det_class"})

        FP = FP[FP["det_y1"].notna()]
        FN = FN[FN["y1"].notna()]

        results = pd.concat([results, FN], axis=0, ignore_index=True)
        results = pd.concat([results, FP], axis=0, ignore_index=True)

        results = results.sort_values(by="frame_id")
        results.drop(columns=[ "index"], inplace=True)
        results.loc[:,"context_id"]=opt.video_context_id
        results.to_csv(f"results/{opt.video_context_id}_report.csv")
        if os.path.exists(gerneral_report_file):
            general=pd.read_csv(gerneral_report_file,index_col=False)
            if not general.context_id.unique().__contains__(opt.video_context_id):
                print("Adding to General Report...")
                pd.concat([general,results], axis=0,ignore_index=True).to_csv(gerneral_report_file,index=False)
            else:
                print("Results already in General Report...")
        else:
            print("Creating General Report...")
            results.to_csv(gerneral_report_file)


    else:
        results = pd.read_csv(f"results/{opt.video_context_id}_report.csv")

        frame_width=640
        frame_height=480
        frames_folder=f"data/{opt.video_context_id}/"


    if opt.create_video:
        print("Creating Video..")
        vid_fps = 15.0
        gt_color= (0, 255, 0)
        det_color=(255, 0, 0)
        fn_color=(255, 255, 0)
        fp_color=(255, 0, 100)
        out = cv2.VideoWriter(f'results/{opt.video_context_id}_video.avi', cv2.VideoWriter_fourcc('M', 'J', 'P', 'G'),
                              vid_fps,
                              (frame_width, frame_height))

        for frame_name in sorted(os.listdir(frames_folder)):
            frame = cv2.imread(f"{frames_folder}{frame_name}")
            frame_ind = int(frame_name.split(".")[0])

            # Saving the gt and detections results for frame_num
            results_in_frame = results[results.frame_id==frame_ind]

            cv2.putText(frame, f"{frame_name}",
                        (0+20, 0+20),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255,255,255), 2)  # drawing IOU score per image

            for i,row in results_in_frame[results_in_frame.detection_category=="TP"].iterrows():
                cv2.rectangle(frame, (int(row.x1),int(row.y1)), (int(row.x2),int(row.y2)), gt_color, 2)
                cv2.rectangle(frame, (int(row.det_x1),int(row.det_y1)), (int(row.det_x2),int(row.det_y2)), det_color, 2)
                cv2.putText(frame, "IoU: {:.2f}".format(row.iou),
                            (int(row.det_x1)-20, int(row.det_y1)-20),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, det_color,2)  # drawing IOU score per image
            for i,row in results_in_frame[results_in_frame.detection_category=="FP"].iterrows():
                cv2.rectangle(frame, (int(row.det_x1),int(row.det_y1)), (int(row.det_x2),int(row.det_y2)), fp_color, 2)
            #
            for i,row in results_in_frame[results_in_frame.detection_category=="FN"].iterrows():
                cv2.rectangle(frame, (int(row.x1),int(row.y1)), (int(row.x2),int(row.y2)), fn_color, 2)

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
    videos_names=opt.video_context_id

    if isinstance(videos_names,list):
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
