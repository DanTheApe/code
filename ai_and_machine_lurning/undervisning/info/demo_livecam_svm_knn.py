"""
Live cam demo: 
collect a few labelled examples by hand, then let an SVM or KNN classify what the camera sees in real time.

"""

import cv2
from skimage.feature import hog
from sklearn.svm import SVC
from sklearn.neighbors import KNeighborsClassifier


IMG_SIZE   = (64, 64)  # fix the size of images from the camera
MODEL_NAME = "knn" # "svm" , "knn" 
CAMERA_ID  = 0 # device ID , default is 0 if using you laptop build-in camera1

# ---------------------------------------------------------------------------
def extract_features(frame):
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)   # colour  to grayscale
    gray = cv2.resize(gray, IMG_SIZE) # converted a fixed size of 64x64
    features = hog(                   # Histogram of Oriented Gradients:
        gray,                    # describes the shape by counting
        orientations=9,         # edge directions in small cells
        pixels_per_cell=(8, 8),
        cells_per_block=(2, 2),
    )
    return features


# Initialize the model 
if MODEL_NAME == "svm":
    model = SVC(kernel="rbf", probability=True)  # kernel: 'poly', 'sigmoid', 'rbf', 'precomputed', 'linear'
else:
    model = KNeighborsClassifier(n_neighbors=5)

X = []   # feature  (one row per example)
y = []    # labels, as  0... 9
trained = False  # flips to True once "t" is pressed


# Running  camera:
cap = cv2.VideoCapture(CAMERA_ID)
print("Keys: [0-9] add example with that label | t = train on what you have | q = quit")

while True:
    ok, frame = cap.read()
    if not ok:
        break

    # 1. if the model is trained, predict on the  grabbed object and show the result on the screen
    if trained:
        feats = extract_features(frame)
        pred  = model.predict([feats])[0]
        conf  = max(model.predict_proba([feats])[0])
        cv2.putText(frame, f"{pred} ({conf * 100:.0f}%)", (20, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)

    # 2 shoing how many training examples were gathered always on the screen
    cv2.putText(frame, f"Examples: {len(y)}", (20, 470),
                cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)

    cv2.imshow(MODEL_NAME + " live demo", frame)
    key = cv2.waitKey(1) & 0xFF

    # 3. keyboard controls: 
    # 'q': quit
    # '0'... '9': to add an example
    # 't':  train the model and start predicting on the image from the camera
    if key == ord("q"):
        break

    if ord("0") <= key <= ord("9"):
        label = chr(key)
        X.append(extract_features(frame))
        y.append(label)
        print(f"Added example for label '{label}'  -- total: {len(y)}")

    # Press 't' to (re)train on demand. 
    if key == ord("t"):
        if len(set(y)) >= 2:
            model.fit(X, y)
            trained = True
            print(f"Trained on {len(y)} examples, {len(set(y))} classes: {sorted(set(y))}")
        else:
            print("Need examples of at least 2 different digits before training.")

cap.release()
cv2.destroyAllWindows()
