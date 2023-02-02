import os
import argparse

def parse_args():
    parser = argparse.ArgumentParser(description="Merging raw frames to video script")
    parser.add_argument('--fps', required=False, default=30)
    parser.add_argument('--path', required=True)
    parser.add_argument('--extension', required=False, default='png', help="png/jpeg etc." )
    return parser.parse_args()

def combine_frames(path, fps, extension):
    """
    Function for merging the raw frames to video.
    params:
        * path: the path where the raw frames are and the output video will be saved
        * fps: the fps we want the output video will be. default is 30
    """
    os.chdir(path)
    path = path + f'/*.{extension}'
    os.system(f"ffmpeg -framerate {fps} -pattern_type glob -i '{path}' -c:v libx264 -pix_fmt yuv420p out.mp4")


if __name__ == "__main__":
    args = parse_args()
    combine_frames(args.path, args.fps, args.extension)
