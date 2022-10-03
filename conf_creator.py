
fileName = '/Users/tom/Desktop/Clickup_Tasks.csv'
templateFile = '/Users/tom/Downloads/template.conf'
confName = input('Insert conf file name: ')
outputFile = f'/Users/tom/Downloads/{confName}.conf'

with open(fileName, 'r') as csvFile:
    lines = csvFile.readlines()
    header = [col.replace(' ', '_') for col in lines[0].split(',')]
    header[-1] = header[-1].replace('\n', '')
    headerStr = str(header).replace("'", '"')

with open(templateFile, 'r') as tmpFile:
    tmpLines = tmpFile.readlines()
    for i in range(len(tmpLines)):
        if 'columns => ' in tmpLines[i]:
            tmpLines[i] = tmpLines[i][:-1]
            tmpLines[i] += headerStr + '\n'
        elif 'index => ' in tmpLines[i]:
            tmpLines[i] = tmpLines[i][:-1]
            tmpLines[i] += f'"{confName}"\n'

with open(outputFile, 'w') as output:
    output.writelines(tmpLines)

