import os
import sys
import shutil

def run(module_path, global_path, args=None):
    path = os.path.dirname(module_path)
    shutil.copy(os.path.join(path,"curlbuild.h"),os.path.join(path,"upstream","include","curl"))
    shutil.copy(os.path.join(path,"setup.h"),os.path.join(path,"upstream","lib"))
