import os
import argparse


def parse_args():
    parser = argparse.ArgumentParser(description="Zero padding and sorting raw frames")
    parser.add_argument('--path', required=True, help='The frames folder path')
    return parser.parse_args()


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
    extension = get_extension(path)

    if not path.endswith('/'):
        path += '/'

    zero_padding(path, extension)