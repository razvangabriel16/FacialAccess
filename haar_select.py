#!/usr/bin/env/python3
import cv2
import sys
import os
import numpy as np
import json
import tflite_runtime.interpreter as tflite

def initialize_phi_mask(face_img):
    h, w = face_img.shape
    cx, cy = w // 2, h // 2
    radius = min(w, h) // 2
    radius_sq = radius * radius

    phi = np.ones((h, w), dtype=np.float32)

    for y in range(h):
        for x in range(w):
            dist_sq = (x - cx) ** 2 + (y - cy) ** 2
            if dist_sq < radius_sq:
                phi[y, x] = 120 #-1.0
            elif dist_sq == radius_sq:
                phi[y, x] = 0.0
            else:
                phi[y, x] = 255 #1.0
    return phi


def find_face(path, padding=20, input_size=112):
    output_dir = "faces"
    os.makedirs(output_dir, exist_ok=True)
    
    img = cv2.imread(path, cv2.IMREAD_GRAYSCALE)
    if img is None:
        print(f"Error: Could not read image from {path}")
        return None
        
    height, width = img.shape[:2]
    
    face_cascade = cv2.CascadeClassifier(
        cv2.data.haarcascades + 'haarcascade_frontalface_default.xml'
    )
    if face_cascade.empty():
        print("Error: Could not load face classifier")
        return None
    
    faces = face_cascade.detectMultiScale(img, scaleFactor=1.1, minNeighbors=3)
    if len(faces) == 0:
        print("No faces detected")
        return None

    processed_faces = []

    contour_dir = "phis"
    os.makedirs(contour_dir, exist_ok=True)
    for i, (x, y, w, h) in enumerate(faces):
        padding = int(max(w, h) * 0.60)
        x1 = max(x - padding, 0)
        y1 = max(y - padding, 0)
        x2 = min(x + w + padding, width)
        y2 = min(y + h + padding, height)
    
        face_img = img[y1:y2, x1:x2]
    
        #face_resized = cv2.resize(face_img, (input_size, input_size))
        face_resized = face_img

        face_processed = face_resized.astype(np.float32)
        face_processed = (face_processed - 127.5) / 128.0
        face_processed = np.expand_dims(face_processed, axis=0)
        processed_faces.append(face_processed)
    
        save_path = os.path.join(output_dir, f'{os.path.splitext(os.path.basename(path))[0]}_face_{i+1}.pgm')
        cv2.imwrite(save_path, face_resized)

        phi_face = initialize_phi_mask(face_resized)
        phi_path = os.path.join(contour_dir, f'{os.path.splitext(os.path.basename(path))[0]}_phi_face_{i+1}.pgm')
        cv2.imwrite(phi_path, phi_face)


    cv2.imwrite('capture_faces.pgm', img)
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

def get_embeddings(model_path, face_images):
    return [get_embedding(model_path, img) for img in face_images]

def get_next_face_index():
    output_path = "embeddings.json"
    if os.path.exists(output_path):
        with open(output_path, 'r') as f:
            existing_data = json.load(f)
        if existing_data:
            return max(entry['face_index'] for entry in existing_data) + 1
    return 1

def main():
    if len(sys.argv) < 2:
        print("Usage: python face_detector.py <image_path> [padding] [input_size]")
        print("Defaults: padding=20, input_size=112")
        sys.exit(1)
    
    args = {
        'path': sys.argv[1],
        'padding': int(sys.argv[2]) if len(sys.argv) > 2 else 20,
        'input_size': int(sys.argv[3]) if len(sys.argv) > 3 else 112
    }
    
    processed_faces = find_face(**args)
    if not processed_faces:
        print("No faces processed")
        sys.exit(0)
    
    model_path = "/home/pi/project2/mobilefacenet.tflite"
    embeddings = get_embeddings(model_path, processed_faces)
    
    next_index = get_next_face_index()
    embeddings_data = [
        {
            "face_index": next_index + i,
            "embedding": embedding.tolist(),
            "source_image": os.path.basename(args['path'])
        }
        for i, embedding in enumerate(embeddings)
    ]
    
    output_path = "embeddings.json"
    existing_data = []
    if os.path.exists(output_path):
        with open(output_path, 'r') as f:
            existing_data = json.load(f)
    
    existing_data.extend(embeddings_data)
    with open(output_path, 'w') as f:
        json.dump(existing_data, f, indent=4)
    
    print(f"Processed {len(processed_faces)} faces. Next index will be {next_index + len(processed_faces)}")

if __name__ == "__main__":
    main()
