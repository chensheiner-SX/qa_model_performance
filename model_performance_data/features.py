import pygame as pg
import pygame_gui as pg_gui


class Arguments:
    def __init__(self, merlin_ip,meerkat_ip, create_video, generate, run_sdk, video_context_id):
        self.merlin_ip = merlin_ip
        self.meerkat_ip = meerkat_ip
        self.create_video = create_video
        self.generate = generate
        self.run_sdk = run_sdk
        self.video_context_id = video_context_id

    def print_options(self):
        print(f"\nMerlin NX IP: {self.merlin_ip}")
        print(f"Meerkat NX IP: {self.meerkat_ip}")
        print(f"Create Video: {self.create_video}")
        print(f"Generate Files: {self.generate}")
        print(f"Run SDK: {self.run_sdk}")
        print(f"Video's Context ID: {self.video_context_id}")


class Features:
    def __init__(self, merlin_ip, label_ip_merlin, meerkat_ip, label_ip_meerkat, sdk, load_file, video_name_1,
                 video_name_2, video_name_3, load_3_videos, video_generate,
                 start_run, error):
        self.merlin_ip = merlin_ip
        self.label_ip_merlin = label_ip_merlin
        self.meerkat_ip = meerkat_ip
        self.label_ip_meerkat = label_ip_meerkat
        self.sdk = sdk
        self.load_file = load_file
        self.video_name_1 = video_name_1
        self.video_name_2 = video_name_2
        self.video_name_3 = video_name_3
        self.load_3_videos = load_3_videos
        self.video_generate = video_generate
        self.start_run = start_run
        self.error = error
