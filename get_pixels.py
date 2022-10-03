import cv2
import os
from pyautogui import *
from tkinter import Tk
from tkinter.filedialog import askopenfilename

def getFileName():
    Tk().withdraw()
    return askopenfilename(title='Choose an image file')

class DrawLineWidget(object):
    
    # imageName = ''
    
    def __init__(self, imageName):

        self.imageName = imageName
        self.original_image = cv2.imread(self.imageName)
        self.clone = self.original_image.copy()

        cv2.namedWindow('image')
        cv2.setMouseCallback('image', self.extract_coordinates)

        # List to store start/end points
        self.image_coordinates = []
    
    def instructions(self):
        message = '''
        Enter image file name, including the file extention (i.e. "image.png").
        To draw a line and get the size, drag with mouse click from point A to B
        a line will be drawn on top of the image marking your selection.
        After drawing, the size will be printed to console.
        Press 'i' to see these instructions.
        Press 'c' to change the image.
        Press 'r' key to clear both image drawings and console.
        Press 'q' to quit.
        
        Enjoy! :)
        '''
        alert(message)

    def extract_coordinates(self, event, x, y, flags, parameters):
        # Record starting (x,y) coordinates on left mouse button click
        if event == cv2.EVENT_LBUTTONDOWN:
            self.image_coordinates = [(x,y)]

        # Record ending (x,y) coordintes on left mouse bottom release
        elif event == cv2.EVENT_LBUTTONUP:
            os.system('cls' if os.name in ('nt', 'dos') else 'clear')
            self.image_coordinates.append((x,y))
            print(f'Starting: {self.image_coordinates[0]}, Ending: {self.image_coordinates[1]}\n')
            
            xSize = abs(self.image_coordinates[1][0] - self.image_coordinates[0][0])
            ySize = abs(self.image_coordinates[1][1] - self.image_coordinates[0][1])
            
            print(f'Size detected:\n\
                x: {xSize} px,\n\
                y: {ySize} px,\n\
                Total square area: {xSize * ySize} px\n')

            # Draw line
            cv2.line(self.clone, self.image_coordinates[0], self.image_coordinates[1], (36,255,12), 2)
            cv2.imshow("image", self.clone) 

        # Clear drawing boxes on right mouse button click
        elif event == cv2.EVENT_RBUTTONDOWN:
            self.clone = self.original_image.copy()

    def show_image(self):
        return self.clone

if __name__ == '__main__':
    imageName = getFileName()
    draw_line_widget = DrawLineWidget(imageName)
    draw_line_widget.instructions()
    while True:
        cv2.imshow('image', draw_line_widget.show_image())
        key = cv2.waitKey(1)
        
        if key == ord('i'):
            draw_line_widget.instructions()
            
        elif key == ord('c'):
            imageName = getFileName()
            if imageName != '':
                draw_line_widget = DrawLineWidget(imageName)
        
        elif key == ord('r'):
            imageName = draw_line_widget.imageName
            draw_line_widget = DrawLineWidget(imageName)
            cv2.imshow('image', draw_line_widget.show_image())
            os.system('cls' if os.name in ('nt', 'dos') else 'clear')

        # Close program with keyboard 'q'
        elif key == ord('q'):
            cv2.destroyAllWindows()
            # exit(1)
            break