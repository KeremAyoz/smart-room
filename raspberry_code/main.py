import os 
import time
import json
import grovepi
import sys
import json
import camera as camera
import paho.mqtt.client as mqtt
from random import randint
try:
    import RPi.GPIO as GPIO
except RuntimeError :
    print("Error : RPi.GPIO library only work with rasberry pi ")

# Variables needed for connecting to the host
host = '3gyk83.messaging.internetofthings.ibmcloud.com'
clientId = 'd:3gyk83:raspberry:raspberryMain'
username = 'use-token-auth'
password = 'W8YS3X*v@6tK1W3N9c'
topic = 'iot-2/evt/status/fmt/json'

client = mqtt.Client(clientId)
client.username_pw_set(username, password)
client.connect(host, 1883, 60)

# Main loop
while True:
    try:
        # Identify person count and send
        camera.take_photo("/home/pi/Desktop")
        # Image path and visual recognition token given to function
        peopleCount = camera.face_number("/home/pi/Desktop/image.jpg", "CsvGN-b7Hx9N_KZ4eXfAo2C3uS6OTEvHo_UGeGOSvh0l")
        camera_json = "{\"people_count\":" +str(peopleCount) + "}"
        # Publish to the host
        client.publish(topic, camera_json)
        print(camera_json)
        time.sleep(2)
    except IOError:
        print("Error")
        
client.loop()
client.disconnect()