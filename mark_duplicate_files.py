import enum
import os
import glob
import shutil
import progressbar
import hashlib

outputPath = '/mnt/data/to_upload/output/'
allDataFilesDir = '/mnt/data/to_upload/atr_all_data/'

dirsInOutput = [os.path.join(outputPath, name) for name in os.listdir(outputPath)]
outputFiles = {}

print('\nProccessing output folder...\n')

bar = progressbar.ProgressBar(maxval=len(dirsInOutput), \
    widgets=[progressbar.Bar('=', '[', ']'), ' ', progressbar.Counter(), progressbar.Percentage()])
bar.start()

for index, folder in enumerate(dirsInOutput):
    tmpList = [os.path.join(folder, name) for name in os.listdir(folder)]
    for item in tmpList:
        if not os.path.isdir(item) and not item.endswith('.json'):
            outputFiles[os.path.split(item)[1]] = hashlib.md5(open(item,'rb').read()).hexdigest()
    bar.update(index + 1)
bar.finish()



videoFiles = {
    'avi': glob.glob(allDataFilesDir + '1/**/*.avi', recursive = True),
    'mpg': glob.glob(allDataFilesDir + '1/**/*.mpg', recursive = True),
    'mp4': glob.glob(allDataFilesDir + '1/**/*.mp4', recursive = True),
    'ts': glob.glob(allDataFilesDir + '1/**/*.ts', recursive = True),
    'bmp': glob.glob(allDataFilesDir + '1/**/*.bmp', recursive = True)
}

for fileType in videoFiles:
    print(f'\nProccessing file type: {fileType} ...\n')
    bar = progressbar.ProgressBar(maxval=len(videoFiles[fileType]), \
        widgets=[progressbar.Bar('=', '[', ']'), ' ', progressbar.Counter(), progressbar.Percentage()])
    bar.start()
    for index, videoFile in enumerate(videoFiles[fileType]):
        filePath, fileName = os.path.split(videoFile)
        if fileName in outputFiles:
            print(f'\nOrigin file found: {videoFile}')
            if 'ORIGIN_' not in fileName:
                os.rename(videoFile, os.path.join(filePath, 'ORIGIN_' + fileName))
        else:
            checkSum = hashlib.md5(open(videoFile, 'rb').read()).hexdigest()
            for item in outputFiles:
                if outputFiles[item] == checkSum:
                    print(f'\nDuplicate file found: {videoFile}')
                    if 'DUPLICATE_' not in fileName:
                        os.rename(videoFile, os.path.join(filePath, 'DUPLICATE_' + fileName))
                    break
        bar.update(index + 1)
    bar.finish()