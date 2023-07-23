import os.path
import pandas as pd
import pygame as pg
import pygame_gui as pg_gui
from features import Features, Arguments
from easygui import fileopenbox
from model_performance_data_collection import main

"""
Useful example for building documentation images
"""


def select_file(default_folder_path="./"):
    file_name = fileopenbox(msg='Please locate the desired file',
                            title='Choose your file with the videos context id',
                            filetypes=["*.csv", ["*.csv", "*.txt"]],
                            default=default_folder_path)
    if file_name is not None:
        print(f"file name: {file_name}")

        file_is_selected, msg, data = handle_context_id_file(file_name)

        return file_is_selected, msg, data
    return False, "No File Was Selected", []
    # try:
    #     load_buttons(f, file_name)
    #     loop_until_folder_selected = False
    # except pg.error:
    #     print("\nWrong file selected")
    #     print("the selection is the path you are in while using the window selector")
    #     default_folder_path = file_name
    #     file_name = None


def check_list_for_files(list_of_files):
    for i, file in enumerate(list_of_files):
        if not os.path.exists(file):
            print(f"Index {i} : {file} doesnt exist")
            return True, f"ERROR: in Index {i} : {file} doesnt exist"
    return False, "File Loaded Successfully"


def handle_context_id_file(file_name):
    lines = []
    if ".txt" in file_name:
        with open(file_name, "r") as f:
            file = f.read()
        lines = [x for x in file.split("\n") if x != '']


    elif ".csv" in file_name:
        data = pd.read_csv(file_name)
        if data.empty:
            lines = data.columns.to_list()
            lines = [x for x in lines if 'Unnamed' not in x]
        else:
            lines = [data.columns[0]] + data.to_numpy().flatten().tolist()
    return False,"File Loaded Successfully", lines
    # return False,f"Loaded File {file_name}",lines


def handle_sdk(sdk):
    match sdk:
        case "Detector+Tracker":
            return "both"
        case "Detector":
            return "detector"
        case "Tracker":
            return "tracker"


def handle_video_generation(video_gen):
    match video_gen:
        case "Generate Files":
            return [False, True]
        case "Generate Video":
            return [True, False]

        case "Generate Both ":
            return [True, True]


pg.init()

pg.display.set_caption('Quick Start')
window_surface = pg.display.set_mode((800, 600))
manager = pg_gui.UIManager((800, 600))

background = pg.Surface((800, 600))
background.fill(pg.Color('#111111'))

# Merlin IP:
merlin_ip = pg_gui.elements.UITextEntryLine(initial_text="172.12.10.5",
                                     relative_rect=pg.Rect((160, 50), (150, 40)),
                                     manager=manager)
label_ip_merlin = pg_gui.elements.UILabel(relative_rect=pg.Rect((30, 50), (150, 40)),
                                   text='Merlin NX IP:',
                                   manager=manager)

# Meerkat IP:
meerkat_ip = pg_gui.elements.UITextEntryLine(initial_text="172.12.10.10",
                                     relative_rect=pg.Rect((160, 100), (150, 40)),
                                     manager=manager)
label_ip_meerkat = pg_gui.elements.UILabel(relative_rect=pg.Rect((30, 100), (150, 40)),
                                   text='Meerkat NX IP:',
                                   manager=manager)
# SDK Configuration:
sdk = pg_gui.elements.UIDropDownMenu(options_list=["Detector+Tracker", "Detector", "Tracker"],
                                     starting_option='Detector+Tracker',
                                     relative_rect=pg.Rect((500, 50), (250, 40)),
                                     manager=manager)
# Load CSV / TXT file:
load_file = pg_gui.elements.UIButton(relative_rect=pg.Rect((500, 250), (250, 40)),
                                     text='Load Video_Names Text File',
                                     manager=manager)

# Input 1 - 3 video names:
video_name_1 = pg_gui.elements.UITextEntryLine(initial_text="Video 1",
                                               placeholder_text="Video 1",
                                               relative_rect=pg.Rect((50, 150), (350, 40)),
                                               manager=manager)

video_name_2 = pg_gui.elements.UITextEntryLine(initial_text="Video 2",
                                               placeholder_text="Video 2",
                                               relative_rect=pg.Rect((50, 250), (350, 40)),
                                               manager=manager)

video_name_3 = pg_gui.elements.UITextEntryLine(initial_text="Video 3",
                                               placeholder_text="Video 3",
                                               relative_rect=pg.Rect((50, 350), (350, 40)),
                                               manager=manager)

# Load 3 videos:
load_3_videos = pg_gui.elements.UIButton(relative_rect=pg.Rect((50, 450), (350, 40)),
                                     text='Load Videos',
                                     manager=manager)

# Generate Files and Videos:
video_generate = pg_gui.elements.UIDropDownMenu(relative_rect=pg.Rect((500, 100), (250, 40)),
                                                options_list=['Generate Files', "Generate Video", "Generate Both "],
                                                starting_option="Generate Both ",
                                                manager=manager)

# Start Run:
start_run = pg_gui.elements.UIButton(relative_rect=pg.Rect((650, 450), (120, 120)),
                                     text='Run',
                                     manager=manager)

# Error massage:
error = pg_gui.elements.UILabel(relative_rect=pg.Rect((50, 500), (500, 40)),
                                text='Error: No Error',
                                manager=manager)

clock = pg.time.Clock()
is_running = True
features = Features(merlin_ip, label_ip_merlin,meerkat_ip, label_ip_meerkat, sdk, load_file, video_name_1, video_name_2, video_name_3,load_3_videos, video_generate,
                    start_run, error)
opt = Arguments("127.12.10.5","127.12.10.10", True, True, "both", [])
while is_running:
    time_delta = clock.tick(60) / 1000.0
    for event in pg.event.get():
        if event.type == pg.QUIT:
            is_running = False
        elif event.type == pg_gui.UI_BUTTON_PRESSED:
            match event.ui_element:
                case features.start_run:
                    print("start_run")
                    opt.print_options()
                    start_run.rebuild()
                    features.start_run.set_text("Processing...")
                    features.start_run.rebuild()
                    window_surface.blit(background, (0, 0))
                    manager.draw_ui(window_surface)
                    pg.display.update()
                    main(opt)
                    features.start_run.set_text("Done")

                case features.load_3_videos:
                    print("load_3_videos")
                    print("video 1", features.video_name_1.text)
                    print("video 2", features.video_name_2.text)
                    print("video 3", features.video_name_3.text)
                    data=[features.video_name_1.text,features.video_name_2.text,features.video_name_3.text]
                    drop_index=[]
                    for i in range(len(data)):
                        if f"Video {i+1}" == data[i]:
                            drop_index.append(i)
                    for i in sorted(drop_index, reverse=True):
                        del data[i]
                    opt.video_context_id=data
                case features.load_file:
                    print("load_file")
                    result = True
                    while result:
                        result, msg, opt.video_context_id = select_file()
                        features.error.set_text(msg)
                        features.error.rebuild()
                        window_surface.blit(background, (0, 0))
                        manager.draw_ui(window_surface)
                        pg.display.update()

        elif event.type == pg_gui.UI_TEXT_ENTRY_FINISHED:
            match event.ui_element:
                case features.merlin_ip:
                    print("merlin_ip", features.merlin_ip.text)
                    opt.merlin_ip = features.merlin_ip.text
                case features.meerkat_ip:
                    print("ip", features.meerkat_ip.text)
                    opt.meerkat_ip = features.meerkat_ip.text
                case features.video_name_1:
                    print("video 1", features.video_name_1.text)
                case features.video_name_2:
                    print("video 2", features.video_name_2.text)
                case features.video_name_3:
                    print("video 3", features.video_name_3.text)
        elif event.type == pg_gui.UI_DROP_DOWN_MENU_CHANGED:
            match event.ui_element:
                case features.sdk:
                    print("sdk", event.text)
                    opt.sdk = handle_sdk(sdk)
                case features.video_generate:
                    print("video_generate", event.text)
                    video_generate = handle_video_generation(event.text)
                    opt.create_video = video_generate[0]
                    opt.generate = video_generate[1]

        manager.process_events(event)

    manager.update(time_delta)

    window_surface.blit(background, (0, 0))
    manager.draw_ui(window_surface)

    pg.display.update()
