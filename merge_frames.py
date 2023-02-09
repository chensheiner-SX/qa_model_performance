import os
import argparse


def parse_args():
    parser = argparse.ArgumentParser(description="Merging raw frames to video script."
                                                 "The output video will be saved in the script folder")
    parser.add_argument('--path', required=True, help='The frames folder path')
    parser.add_argument('--fps', required=False, default=30, help='The fps for output video')
    parser.add_argument('--name', required=False, default='output', help="output video name")
    return parser.parse_args()


def combine_frames(path, fps, extension, vid_name):
    """
    Function for merging the raw frames to video using ffmpeg
    params:
        * path:         the path where the raw frames are and the output video will be saved
        * fps:          the fps we want the output video will be. default is 30
        * extension:    the frames extension format
        * vid_name:     the name of the output video that will be created
    """

    # os.chdir(path)
    path = path + f'*{extension}'
    os.system(f"ffmpeg -framerate {fps} -pattern_type glob -i '{path}' -c:v libx264 -pix_fmt yuv420p {vid_name}.mp4")


def zero_padding(path, extension, json_ext='.json'):

    frames_num_lst = []

    # Extracting the frames names(numbers) without the extensions or any other character (if any) and save them to list
    for name in os.listdir(path):
        if name.endswith(extension):
            name = ''.join(filter(str.isdigit, name))
            frames_num_lst.append(int(name))

    # Sorting the frames numbers list and checking how many zeros to fill before each frame new name
    frames_num_lst.sort()
    zeros_to_fill = len(str(frames_num_lst[-1])) + 1

    # Changing the names of frames with zeros leading (and jsons if any)
    for i in range(len(frames_num_lst)):
        frames_num_lst[i] = (str(frames_num_lst[i])).zfill(zeros_to_fill)

    for i, name in enumerate(sorted(os.listdir(path))):
        for j in range(0, len(frames_num_lst)):
            if name.endswith(extension) and int(frames_num_lst[j]) == int(''.join(filter(str.isdigit, name))):
                print(f"{name} --> {frames_num_lst[j]}{extension}")
                os.rename(f"{path}{name}", f"{path}{frames_num_lst[j]}{extension}")
                break
            elif name.endswith(json_ext) and int(frames_num_lst[j]) == int(''.join(filter(str.isdigit, name))):
                print(f"{name} --> {frames_num_lst[j]}{json_ext}")
                os.rename(f"{path}{name}", f"{path}{frames_num_lst[j]}{json_ext}")
                break


def get_extension(path):
    for file in os.listdir(path):
        ext = os.path.splitext(file)[1]
        if ext == '.json':
            continue
        else:
            break
    return ext


if __name__ == "__main__":
    args = parse_args()

    path = args.path
    fps = args.fps
    vid_name = args.name
    extension = get_extension(path)

    if not path.endswith('/'):
        path += '/'

    zero_padding(path, extension)
    combine_frames(path, fps, extension, vid_name)
