import os
import csv
import argparse


def parse_args():
    parser = argparse.ArgumentParser(description="This script will parsing the csv annotations for video")
    parser.add_argument("--name", required=True, help="Name of annotations csv file", metavar="file_name.csv")

    return parser.parse_args()

def combine_and_get_num(lst):
    result = ''
    for index in range(len(lst)):
        while(not lst[index].isdigit()):
            index += 1
        while(lst[index].isdigit()):
            result += lst[index]
            index += 1
        if lst[index] == '.':
            result += lst[index]
            index += 1
            while(lst[index].isdigit()):
                result += lst[index]
                index +=1
        break

    lst = lst[index:]
    return lst, float(result)


def parse_csv(csv_path):
    result = {}
    with open(csv_path, 'r') as csv_file:
        csv_reader = csv.reader(csv_file)

        next(csv_reader)
        results = {}
        for line in csv_reader:
            frame_id = line[0]
            subclass = line[2]
            # TODO: line[3] is considered as string, find a way to make it a list
            coordinates_lst = list(line[3])
            coordinates_lst = [char for char in coordinates_lst if char.isdigit() or char == '.' or char == ' ']
            # print(coordinates_lst)
            x1 = float((''.join(coordinates_lst)).split(' ')[0])
            y1 = float((''.join(coordinates_lst)).split(' ')[1])
            x2 = float((''.join(coordinates_lst)).split(' ')[4])
            y2 = float((''.join(coordinates_lst)).split(' ')[5])
            if subclass == 'motorcycle':
                class_ = "two-wheeled"
            else:
                class_ = line[1]

            result.setdefault(frame_id, []).append(
                (
                    class_,
                    x1,
                    y1,
                    x2,
                    y2
                )
            )

        print(result)

def main():
    pass

if __name__ == "__main__":
    args = parse_args()
    parse_csv(os.path.abspath(args.name))
    main()