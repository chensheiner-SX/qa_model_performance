# Import Libraries
import cv2
import numpy as np

# Parameters for Shi-tomasi corner detection
st_params = dict(maxCorners = 30,
                qualityLevel = 0.1,
                minDistance = 2,
                blockSize = 7)



# Parameters for Lucas-Kande optical flow
lk_params = dict(winSize = (15,15),
                maxLevel = 2,
                criteria = (cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 10,1))

dotted_trail=True


resi=(1024,640)

# Video Capture - Input Video
cap = cv2.VideoCapture('output_video2.avi')

# Color for optical flow
color = (0, 255, 0)


# Read the capture and get the first frame
ret, first_frame = cap.read()
first_frame = cv2.resize(first_frame, resi)

# Convert frame to Grayscale
prev_gray = cv2.cvtColor(first_frame, cv2.COLOR_BGR2GRAY)

# Find the strongest corners in the first frame
prev = cv2.goodFeaturesToTrack(prev_gray, mask=None, **st_params)
# Create an image with the same dimensions as the frame for later drawing purposes
mask = np.zeros_like(first_frame)


# Output Video
out = cv2.VideoWriter('with_trail.mp4',cv2.VideoWriter_fourcc(*'MP4V'), 5, resi)


count=0
# While loop
while (cap.isOpened()):
    count+=1
    # Read the capture and get the first frame
    ret, frame = cap.read()
    if not ret:
        exit()
    frame = cv2.resize(frame, resi)
    # Convert all frame to Grayscale (previously we did only the first frame)
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    # Calculate optical flow by Lucas-Kanade
    next, status, error = cv2.calcOpticalFlowPyrLK(prev_gray, gray, prev, None, **lk_params)

    # Select good feature for the previous position
    good_old = prev[status == 1]

    # Select good feature for the next position
    good_new = next[status == 1]

    # spesific to this Video- Aggrogate Big Sail Boat Dots
    test=good_new[:-1].mean(axis=0)
    test_old=good_old[:-1].mean(axis=0)

    good_new=np.vstack([good_new[:-1].mean(axis=0), good_new[-1]])
    good_old=np.vstack([good_old[:-1].mean(axis=0), good_old[-1]])

    # Draw optical flow track
    for i, (new, old) in enumerate(zip(good_new, good_old)):
        # Return coordinates for the new point
        a, b = new.ravel()

        # Return coordinates for the old point
        c, d = new.ravel()

        # # Draw doted line between new and old position - dotted trail
        if dotted_trail:
            mask = cv2.line(mask,
                        (int(a), int(b)),
                        (int(c), int(d)),
                        color, 2)

        # Draw filled circle
        frame = cv2.circle(frame,
                           (int(a), int(b)),
                           3,
                           (255,0,0),
                           -1)

        if i==0:
            # Draw BB on Big Sail Boat
            frame = cv2.rectangle(frame, (int(a) - 20, int(b) - 30), (int(a) + 20, int(b) + 30), (0, 255, 255), 1)
            frame = cv2.putText(frame,
                                "Sail_Boat",
                                (int(a)-20, int(b)-35),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.3,
                                (0, 255, 255),
                                1,
                                cv2.LINE_4)
        else:
            # Draw BB on Small Sail Boat
            frame = cv2.rectangle(frame, (int(a) - 10, int(b) - 15), (int(a) + 10, int(b) + 15), (255, 255, 0), 1)
            frame = cv2.putText(frame,
                                "Small_Boat",
                                (int(a)-10, int(b)-17),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.3,
                                (255, 255, 0),
                                1,
                                cv2.LINE_4)
    # Draw Frame Number on Video
    # frame=cv2.putText(frame,
    #             str(count),
    #             (50, 50),
    #             cv2.FONT_HERSHEY_SIMPLEX, 1,
    #             (0, 255, 255),
    #             2,
    #             cv2.LINE_4)

    # Overlay optical flow on original frame
    output = cv2.add(frame, mask)

    # Update previous frame
    prev_gray = gray.copy()

    # Update previous good features
    prev = good_new.reshape(-1, 1, 2)
    out.write(output)
    # Open new window and display the output
    cv2.imshow("Optical Flow", output)

    # Close the frame
    if cv2.waitKey(300) & 0xFF == ord('q'):
        break

# Release and Destroy
cap.release()
cv2.destroyAllWindows()
out.release()





