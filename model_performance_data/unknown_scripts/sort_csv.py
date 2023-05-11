import argparse
import pandas as pd


def parse_args():
    parser = argparse.ArgumentParser(description="Merging raw frames to video script."
                                                 "The output video will be saved in the script folder")
    parser.add_argument('--path', required=True, help='The csv file path')
    return parser.parse_args()


args = parse_args()
csv_path = args.path

csv_file = pd.read_csv(f"{csv_path}")
# print("B4 sort:")
# print(csv_file)

# TODO: add capability of send n arguments and pass them to the sort list as columns to sort by
csv_file.sort_values(["frame_id"], axis=0, inplace=True)
# print("After sort:")
# print(csv_file)

# Writing on existing csv
csv_file.to_csv(f"{csv_path}", index=False)

