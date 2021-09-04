import os
import shutil
import errno
import boto3
from botocore.exceptions import ClientError

CONFIGS = [
        { "BUILD_DIRECTORY": "build32", "CV2": "x86", "CVEXT": ""},
        { "BUILD_DIRECTORY": "build64", "CV2": "x64", "CVEXT": "_64"},
]

s3_client = boto3.client('s3')

for cfg in CONFIGS:
    bdir = cfg["BUILD_DIRECTORY"]
    cvdir = None
    cv2 = cfg["CV2"]

    with open(f"{bdir}/CMakeCache.txt") as f:
        for line in f.readlines():
            if line.startswith("OpenCV_DIR"):
                cvdir = line.split("=")[1].strip()
                break

    if cvdir is None:
        raise RuntimeError("no cv dir?")

    nam = f"graphite_{cv2}"
    out_dir = f"deploy/{nam}/"

    try:
        os.makedirs(out_dir)
    except FileExistsError:
        pass

    if cvdir.startswith("C:"):
        cvdir = "/mnt/c" + cvdir[2:]

    gpath = f"{bdir}/graphite/Release/graphite.exe"

    PATHS = [
        gpath,
        f"{bdir}/3rd/SDL/Release/SDL2.dll",
        f"{cvdir}/{cv2}/vc16/bin/opencv_videoio_ffmpeg453{cfg['CVEXT']}.dll",
        f"{cvdir}/{cv2}/vc16/bin/opencv_world453.dll",
    ]

    for p in PATHS:
        bname = os.path.basename(p)
        shutil.copyfile(p, f"{out_dir}/{bname}")


    shutil.make_archive(f"deploy/{nam}", 'zip', out_dir)
    out_zip = f"deploy/{nam}.zip"

    try:
        s3_client.upload_file(out_zip, "flibidydibidy.com", f"dist/{nam}.zip",
                ExtraArgs={'ACL': 'public-read'});
        s3_client.upload_file(gpath, "flibidydibidy.com", f"dist/{nam}.exe",
                ExtraArgs={'ACL': 'public-read'});
    except ClientError as e:
        print(e)
        break

print("update the version in flibidydibidycom")



