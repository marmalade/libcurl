import os
import sys
import shutil
import filecmp

def copy_if_modified(srce,dest):
    if not os.path.exists(dest) or not filecmp.cmp(srce,dest):
        print srce,"to",dest,"COPY"
        shutil.copy(srce,dest)
    else:
        print srce,"to",dest,"NOT MODIFIED"

def run(module_path, global_path, args=None):
    path = os.path.dirname(module_path)
    copy_if_modified(os.path.join(path,"curlbuild.h"),os.path.join(path,"upstream","include","curl","curlbuild.h"))
    copy_if_modified(os.path.join(path,"setup.h"),os.path.join(path,"upstream","lib","setup.h"))
