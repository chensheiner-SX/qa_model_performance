import pygame as pg
import pygame_gui as pg_gui


class Arguments:
    def __init__(self, nx_ip, create_video, generate, run_sdk, video_context_id):
        self.nx_ip = nx_ip
        self.create_video=create_video
        self.generate=generate
        self.run_sdk=run_sdk
        self.video_context_id=video_context_id
    def print_options(self):
        print(f"\nNX IP: {self.nx_ip}")
        print(f"Create Video: {self.create_video}")
        print(f"Generate Files: {self.generate}")
        print(f"Run SDK: {self.run_sdk}")
        print(f"Video's Context ID: {self.video_context_id}")



class Features:
    def __init__(self, ip, label_ip, sdk, load_file, video_name_1, video_name_2, video_name_3, video_generate,
                 start_run, error):
        self.ip = ip
        self.label_ip = label_ip
        self.sdk = sdk
        self.load_file = load_file
        self.video_name_1 = video_name_1
        self.video_name_2 = video_name_2
        self.video_name_3 = video_name_3
        self.video_generate = video_generate
        self.start_run = start_run
        self.error = error
