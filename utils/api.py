from config.sound_config import SoundConfig
from config.global_config import global_config_obj
import requests
import json
import os

def get_all_switch_state():
    return {
        **global_config_obj.__dict__
    }

def update_all_switch_state(state):
    print("更改开关状态:", state)
    global_config_obj.__dict__.update(state)
    global_config_obj.save()

def list_plugins():
    resp = requests.get("http://43.155.97.158:8089/list/kb")
    return json.loads(resp.text)

def upload_plugin(name, version, downloadUrl):
    resp = requests.post("http://43.155.97.158:8089/upload", json.dumps({
        "namespace": "kb",
        "name": name,
        "author": os.getlogin(),
        "version": version,
        "downloadUrl": downloadUrl
    }))
    return json.loads(resp.text)

def delete_plugin(name):
    resp = requests.delete("http://43.155.97.158:8089/del/kb/" + name)
    return json.loads(resp.text)

def get_plugin(name):
    resp = requests.delete("http://43.155.97.158:8089/get/kb/" + name)
    return json.loads(resp.text)

def upload_file(filepath):
    resp = requests.post("http://43.155.97.158:8088/uploadFile", files={
        "file": open(filepath, "rb")
    })
    return resp.json()

def download_file(filename):
    resp = requests.get("http://43.155.97.158:8088/downloadFile/" + filename)
    if not os.path.exists("download_files"):
        os.mkdir("download_files")
    with open(os.path.join("download_files", filename), "wb") as f:
        f.write(resp.content)
    return True