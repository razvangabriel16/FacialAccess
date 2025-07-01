#!/usr/bin/env/python3
import cv2
import sys
import os
import numpy as np
import json
import tflite_runtime.interpreter as tflite

"""
def find_face(path, padding = 20, input_size = 112):
    
    output_dir = "faces"
    os.makedirs(output_dir, exist_ok=True)
    
    img = cv2.imread(path)
    height, width = img.shape[:2]

    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    gray = cv2.equalizeHist(gray)

    face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')
    if face_cascade.empty():
        print("nu a incarcat clasificatorul")
        return None
    faces = face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=3)
    if len(faces) == 0:
        print("nu a vazut bot")
        return None

    processed_faces = []
    for i, (x, y, w, h) in enumerate(faces):
        x1 = max(x - padding, 0)
        y1 = max(y - padding, 0)
        x2 = min(x + w + padding, width)
        y2 = min(y + h + padding, height)
        #practic nr de pixeli va fi 3 *(x2-x1) * (y2 - y1)
        cv2.rectangle(img, (x1, y1), (x2, y2), (255, 0, 0), 2)  # BGR
        face_img = img[y1:y2, x1:x2]
        
        save_path = os.path.join(output_dir, f'{path}_face_{i+1}.jpeg')
        face_display = cv2.resize(face_img, (input_size, input_size))
        cv2.imwrite(save_path, face_display)
        
        face_processed = cv2.resize(face_img, (input_size, input_size))
        face_processed = face_processed.astype(np.float32)
        face_processed = (face_processed - 127.5) / 128.0  # normalizea în interval [-1, 1]
        face_processed = np.expand_dims(face_processed, axis=0)
        processed_faces.append(face_processed)
    
    cv2.imwrite('capture_faces.jpg', img)
    return processed_faces

def get_embedding(tflite_model_path, face_img):
    interpreter = tflite.Interpreter(model_path=tflite_model_path)
    interpreter.allocate_tensors()
    
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    interpreter.set_tensor(input_details[0]['index'], face_img)
    interpreter.invoke()
    embedding = interpreter.get_tensor(output_details[0]['index'])[0]
    return embedding


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python detect_faces.py <image_path>")
        sys.exit(1)
    model_path = "/home/pi/project/mobilefacenet.tflite"
    idx = int(sys.argv[2]) 
    processed_faces = find_face(sys.argv[1])
    if processed_faces:
        embeddings_data = []
        for i, face_img in enumerate(processed_faces):
            embedding = get_embedding(model_path, face_img)
            embedding_list = embedding.tolist()
            embeddings_data.append({
                "face_index": i + idx + 1,
                "embedding": embedding_list,
                "source_image": sys.argv[1]
            })
        output_path = "embeddings.json"
        if os.path.exists(output_path):
            with open(output_path, 'r') as f:
                existing_data = json.load(f)
        else:
            existing_data = []
        existing_data.extend(embeddings_data)
        with open(output_path, 'w') as f:
            json.dump(existing_data, f, indent=4)
"""

def find_face(path, padding = 20, input_size = 112):
    output_dir = "faces"
    os.makedirs(output_dir, exist_ok=True)
    img = cv2.imread(path, cv2.IMREAD_GRAYSCALE)
    height, width = img.shape[:2]
    img = cv2.equalizeHist(img)

    #de facut egalizarea inainte de asta, in asm
    face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')
    if face_cascade.empty():
        print("nu a incarcat clasificatorul")
        return None
    faces = face_cascade.detectMultiScale(img, scaleFactor=1.1, minNeighbors=3)
    if len(faces) == 0:
        print("nu a vazut bot")
        return None

    processed_faces = []
    for i, (x, y, w, h) in enumerate(faces):
        x1 = max(x - padding, 0)
        y1 = max(y - padding, 0)
        x2 = min(x + w + padding, width)
        y2 = min(y + h + padding, height)
        cv2.rectangle(img, (x1, y1), (x2, y2), (255, 0, 0), 2)  # BGR
        face_img = img[y1:y2, x1:x2]
        
        save_path = os.path.join(output_dir, f'{path}_face_{i+1}.jpeg')
        face_display = cv2.resize(face_img, (input_size, input_size))
        cv2.imwrite(save_path, face_display)
        
        face_processed = cv2.resize(face_img, (input_size, input_size))
        face_processed = face_processed.astype(np.float32)
        face_processed = (face_processed - 127.5) / 128.0  # normalizea în interval [-1, 1]
        face_processed = np.expand_dims(face_processed, axis=0)
        processed_faces.append(face_processed)
    
    cv2.imwrite('capture_faces.jpg', img)
    return processed_faces

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python face_detector.py <image_path> [padding] [input_size]")
        print("Defaults: padding=20, input_size=112")
        sys.exit(1)
    args = {
        'path': sys.argv[1],
        'padding': int(sys.argv[2]) if len(sys.argv) > 2 else 20,
        'input_size': int(sys.argv[3]) if len(sys.argv) > 3 else 112
    }
    model_path = "/home/pi/project2/mobilefacenet.tflite"
    processed_faces = find_face(**args)
    idx = int(sys.argv[2]) 
    processed_faces = find_face(sys.argv[1])
    if processed_faces:
        embeddings_data = []
        for i, face_img in enumerate(processed_faces):
            embedding = get_embedding(model_path, face_img)
            embedding_list = embedding.tolist()
            embeddings_data.append({
                "face_index": i + idx + 1,
                "embedding": embedding_list,
                "source_image": sys.argv[1]
            })
        output_path = "embeddings.json"
        if os.path.exists(output_path):
            with open(output_path, 'r') as f:
                existing_data = json.load(f)
        else:
            existing_data = []
        existing_data.extend(embeddings_data)
        with open(output_path, 'w') as f:
            json.dump(existing_data, f, indent=4)
    print(json.dumps(processed_faces))

