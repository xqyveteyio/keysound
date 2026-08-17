import json
import os

class SoundConfig:
    def __init__(self) -> None:
        self.name = ""
        self.mode = ""
        self.repeat_sound = ""
        self.version = "1.0.0"
        self.assigned_sounds = []
        self.soundsList = []
        self.single_key = []

    def save(self):
        with open(f'./sounds/{self.name}/index.json', 'w', encoding='utf-8') as f:
            json.dump(self.__dict__, f, ensure_ascii=False, indent=4)

    @staticmethod
    def read_from_file(filename: str):
        config = SoundConfig()
        json_data = json.load(open(f'./sounds/{filename}/index.json', 'r', encoding='utf-8'))
        config.name = json_data.get("name", "")
        config.mode = json_data.get("mode", "")
        config.repeat_sound = json_data.get("repeat_sound", "")
        config.assigned_sounds = json_data.get("assigned_sounds", [])
        config.soundsList = json_data.get("soundsList", [])
        config.single_key = json_data.get("single_key", [])
        # config.__dict__ = json_data
        return config

    @staticmethod
    def create_sound(name: str):
        print(name)
        config = SoundConfig()
        config.name = name
        # 创建文件夹
        os.mkdir(f'./sounds/{name}')
        os.mkdir(f'./sounds/{name}/sounds')
        # 写入index.json
        with open(f'./sounds/{name}/index.json', 'w', encoding='utf-8') as f:
            json.dump(config.__dict__, f, ensure_ascii=False, indent=4)

