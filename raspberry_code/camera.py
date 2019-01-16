import os 
import time
import json
import grovepi
import sys
import json
import paho.mqtt.client as mqtt
from random import randint

def crop_image(datapath,cropped_path, height, width, left, top):
    photo = io.imread(str(datapath))
    height_im = height + top
    width_im = width + left
    photo = photo[left:height_im,top:width_im]
    im = Image.fromarray(photo)
    cropped_path = cropped_path + 'image.jpg'
    im.save(str(cropped_path))
    return cropped_path


def detect_faces(image_path, api_key, output_path ):
    output_json = output_path +'out_faces.json'
    detect_face_command = 'curl -X POST -u \"apikey:{}\" \
    --form \"images_file=@{}\" \
    \"https://gateway.watsonplatform.net/visual-recognition/api/v3/detect_faces?version=2018-03-19\" > {}'.format(api_key,image_path, output_json)
    os.system(detect_face_command)
    return output_json

def motion_detection(input= 11, output= 3):
    os.system('pip install RPi.GPIO')
    GPIO.setwarnings(False)
    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(input, GPIO.IN)         #Read output from PIR motion sensor
    GPIO.setup(output, GPIO.OUT)         #LED output pin
    return GPIO.input(input)

def take_photo(image_path):
    abs_image_path = image_path + '/image.jpg'
    os.system('fswebcam -r 1280x720 --no-banner {}'.format(abs_image_path))
    return abs_image_path

def read_face_json(path_json): 
    try : 
        with open(path_json) as data_file:    
            data = json.load(data_file)
    except:
        return False
    face_number = len(data['images'][0]['faces'])
    if face_number == 0:
        return {'face_number': 0}
    else:
        height = data['images'][0]['faces'][0]['face_location']['height']
        width =  data['images'][0]['faces'][0]['face_location']['width']
        left = data['images'][0]['faces'][0]['face_location']['left']
        top = data['images'][0]['faces'][0]['face_location']['top']
        feed_dict = { 'face_number': face_number, 'height': height, "width" : width, 'left': left, 'top': top }
        return feed_dict

def face_number(image_path, api_key):
        output_path_json = detect_faces(image_path, api_key, image_path)
        feed_face_dict = read_face_json(output_path_json)
        print(feed_face_dict['face_number'])
        return feed_face_dict['face_number']