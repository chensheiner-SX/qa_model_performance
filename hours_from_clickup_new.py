import csv
import pandas as pd
import xlsxwriter
import openpyxl
import os
from tkinter import *
from tkinter import filedialog
from pyclickup import ClickUp
import datetime
import progressbar
import calendar

clickupTitle = 'Select .csv file from ClickUp'
excelTitle = 'Select .xlsx file'
tmpFileName = 'tmp_file.csv'

fieldnames = ['Worker Name', 'SOI-Indoor', 'SOI-Outdoor', 'Ananas', 'Ogen', 'Hedek', 'Storm Breaker', 'Litening', 'Tracker', 'Meerkat', 'Bluebird', 'SPEAR UAV', 'Xtend', 'AI Dojo', 'Tezeus- SPEAR UAV+ Rafael', 'Pre-sale', 'Other', 'Infra', 'Aeronautics', 'Office']

clickup = ClickUp('pk_5781232_AJ8WSGF4ZCZRH66CE6NPIS7SCV1CVMYG') # Ron Ganam's Token

users = ','.join([str(user.id) for user in clickup.teams[0].members])

thisDate = datetime.datetime.now()
currentDay = thisDate.day
currentMonth = thisDate.month
currentYear = thisDate.year

lastDay = calendar.monthrange(currentYear, currentMonth)[1]

firstUnix = datetime.datetime.strptime(f'1.{currentMonth}.{currentYear} 00:00:00,0',
                           '%d.%m.%Y %H:%M:%S,%f')
firstUnixMs = int(firstUnix.timestamp() * 1000)

lastUnix = datetime.datetime.strptime(f'{lastDay}.{currentMonth}.{currentYear} 23:59:59,0',
                           '%d.%m.%Y %H:%M:%S,%f')
lastUnixMs = int(lastUnix.timestamp() * 1000)

timeTracked = clickup.get(f'https://api.clickup.com/api/v2/team/3745276/time_entries?start_date={firstUnixMs}&end_date={lastUnixMs}&assignee={users}')

def selectFile(title):
    root = Tk()
    root.withdraw()
    fileName = filedialog.askopenfilename(parent=root, title=title)
    root.destroy()
    return fileName

def checkSubject(subject):
    if subject == 'Meerkat,Hedek':
        return 'Meerkat'
    elif subject == 'Marketing':
        return 'Pre-sale'
    elif subject == 'StormBreaker':
        return 'Storm Breaker'
    elif subject == '' or subject == 'Security' or subject not in fieldnames:
        return 'Other'
    return subject

def findSubject(taskDict):
    subject = 'Other'
    for field in taskDict['custom_fields']:
        if field['name'] == 'Subject':
            if 'value' in field.keys():
                valueId = field['value'][0]
                for value in field['type_config']['options']:
                    if value['id'] == valueId:
                        subject = checkSubject(value['label'])
                        return subject
    return subject

# clickUpFilePath = selectFile(clickupTitle)
excelFilePath = selectFile(excelTitle)

workers = {}

bar = progressbar.ProgressBar(maxval=len(timeTracked['data']), \
    widgets=[progressbar.Bar('=', '[', ']'), ' ', progressbar.Percentage()])
bar.start()

for index, trackedItem in enumerate(timeTracked['data']):
    currentTask = trackedItem['task']
    taskData = clickup.get(f'https://api.clickup.com/api/v2/task/{currentTask["id"]}')
    if 'err' in taskData:
        continue
    name = trackedItem['user']['username']
    if '@' in name:
        name = ' '.join((name[:name.rindex('@')].split('.')))
    subject = findSubject(taskData)
    tracked = int(trackedItem['duration']) / 3600000
    
    if name in workers.keys():
        if subject in workers[name].keys():
            workers[name][subject] += tracked
        else:
            workers[name][subject] = tracked
    else:
        workers[name] = {
            subject: tracked
        }
    bar.update(index + 1)

# with open(clickUpFilePath, 'r') as clickUpFile:
#     reader = csv.DictReader(clickUpFile)
#     for row in reader:
#         name = row['Username']
#         if '@' in name:
#             name = ' '.join((name[:name.rindex('@')].split('.')))
#         if row['Subject'] == None:
#             subject = 'Other'
#         else:
#             subject = row['Subject'][1:-1]
#         if subject == '' or subject == 'Security':
#             subject = 'Other'
#         elif subject == 'Meerkat,Hedek':
#             subject = 'Meerkat'
#         elif subject == 'Marketing':
#             subject = 'Pre-sale'
#         elif subject == 'StormBreaker':
#             subject = 'Storm Breaker'
#         tracked = round(int(row['Time Tracked']) / 3600000, 2)
#         if name in workers.keys():
#             if subject in workers[name].keys():
#                 workers[name][subject] += tracked
#             else:
#                 workers[name][subject] = tracked
#         else:
#             workers[name] = {
#                 subject: tracked
#             }

workersList = []
for worker in workers.keys():
    workers[worker]['Worker Name'] = worker
    workersList.append(workers[worker])



with open(tmpFileName, 'w') as csvFile:
    writer = csv.DictWriter(csvFile, fieldnames=fieldnames)
    writer.writeheader()
    writer.writerows(workersList)

book = openpyxl.load_workbook(excelFilePath)
excelWriter = pd.ExcelWriter(excelFilePath, engine='openpyxl')
excelWriter.book = book
newData = pd.DataFrame(pd.read_csv(tmpFileName))
excelWriter.sheets = dict((ws.title, ws) for ws in book.worksheets)
newData.to_excel(excelWriter, sheet_name='Working Hours')

excelWriter.save()
os.remove(tmpFileName)