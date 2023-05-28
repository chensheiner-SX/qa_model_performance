
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

db_creds = {'user': 'chen-sheiner', 'password': '4kWY05mQQiRcr4BCFqG5',
            'host': 'database-poc.cgkbiipjug0o.eu-west-1.rds.amazonaws.com', 'db_name': 'SXDBPROD'}
db_client = DB_Client(db_creds)

query = """ 
select video_name,video_gk, frame_count
from dwh.videos
where payload_code=11 and wavelength_code=4 and bits='8'
order by frame_count
            """
d = db_client.sql_query_db(query)


data=pd.DataFrame(columns=["video_name_8","video_gk_8"," frame_count_8","video_name_16","video_gk_16"," frame_count_16"],index=range(len(d)))
for i, name in enumerate(d.video_name):

    query = """ 
    select video_name,video_gk, frame_count
    from dwh.videos
    where payload_code=11 and wavelength_code=4 and bits='16' and video_name like '%%test_name%%'
    order by frame_count
                """
    query = query.replace("test_name", name[:-20])
    a = db_client.sql_query_db(query)
    # print(a)
    data_eight=d.iloc[i]
    data.iloc[i]=[data_eight.video_name,data_eight.video_gk,data_eight.frame_count,a.video_name[0],a.video_gk[0],a.frame_count[0]]

for gk in tqdm(data.video_gk_16):
    quarry_test = """
    select  video_gk
    from dwh.sensor_normalization_values
    where dwh.sensor_normalization_values.video_gk = test
    """
    quarry_test = quarry_test.replace("test", str(gk))

    q = db_client.sql_query_db(quarry_test)
    if not q.empty:
        print(q)


quarry="""
select  distinct bits,mean_norm,std_norm
from dwh.sensor_normalization_values
where dwh.sensor_normalization_values.payload_code = 11 and wavelength_code=4 
"""
normalization_data = db_client.sql_query_db(quarry)