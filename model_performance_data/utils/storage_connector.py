
from termcolor import cprint


def get_files_location(files):
    files_location = {}
    for file in files:
        folder = get_location(file)
        if folder not in files_location:
            files_location[folder] = []
        files_location[folder].append(file)

    return files_location
# def get_files_location(files):
#     files_location = []
#     for file in files:
#         folder = get_location(file)
#         if folder not in files_location:
#             files_location.append(folder)
#     return files_location

#
# def get_files_suffix(files):
#     files_suffix = []
#     for file in files:
#         folder = get_suffix(file)
#         if folder not in files_suffix:
#             files_suffix.append(folder)
#     return files_suffix

def get_files_suffix(files_location):
    files_data={}
    files_suffix = []
    for folder in files_location:
        files_data[folder] = {}
        for file in files_location[folder]:
            suffix = get_suffix(file)
            if len(suffix)>1:
                suffix = suffix[1]
                if suffix not in files_data[folder]:
                    files_data[folder][suffix]=0
                files_data[folder][suffix] += 1
    return files_data

def get_suffix(file):
    return file.rsplit('.', 1)


def get_location(file):
    return file.rsplit('/', 1)[0] + '/'


def get_folders_count(files_location):
    sub_folders = -1
    for paths in files_location:
        if paths.count('/') != sub_folders and sub_folders != -1:
            cprint("Too many folder levels in the destination", "red", attrs=['bold'], end='\n')
        elif paths.count('/') != sub_folders and sub_folders == -1:
            sub_folders = paths.count('/')
    if sub_folders == -1:
        cprint("No sub folders", "red", attrs=['bold'], end='\n')

    return sub_folders
