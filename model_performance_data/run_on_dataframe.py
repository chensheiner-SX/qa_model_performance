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
import os

pairs_videos = pd.read_csv("8_vs_16_bit_videos.csv")

for i,row in pairs_videos.sample(10).iterrows():

    os.system(f"python3 model_performance_data_collection.py --video_context_id {row['8bit']} --flow_id flir --create_video False ")
    print("\n\n DONE 8 bit\n\n")
    os.system(f"python3 model_performance_data_collection.py --video_context_id {row['16bit']} --flow_id mwir --create_video False ")
    print("\n\n DONE 16 bit\n\n")
