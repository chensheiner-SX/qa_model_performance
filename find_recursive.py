import fnmatch
from nis import match
import os
import shutil

src = '/Users/tom/Desktop/test'

foldersToDelete = []
filesToDelete = []
for root, dirnames, filenames in os.walk(src):
    for dirName in dirnames:
        if '_N' in dirName:
            start, end = dirName.split('_N')
            start += '_N'
            end = end[:end.index('_')]
            newName = start + end
            os.rename(os.path.join(root, dirName), os.path.join(root, newName))
for root, dirnames, filenames in os.walk(src):
    for dirName in fnmatch.filter(dirnames, 'Json'):
        # foldersToDelete.append(os.path.join(root, dirName))
        shutil.rmtree(os.path.join(root, dirName))
        
for root, dirnames, filenames in os.walk(src):
    for fileName in fnmatch.filter(filenames, '*.json'):
        os.remove(os.path.join(root, fileName))
        # filesToDelete.append(os.path.join(root, fileName))

# print(foldersToDelete)        
# print(filesToDelete)