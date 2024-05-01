# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

from awscrt import io, mqtt, auth, http
from awsiot import mqtt_connection_builder
import time as t
import json
data = [
    ["k1o8MeCeVdMeTu7YLCS6", "Manhattan", "", "150", "Convent Ave", 1, "2024-03-22 21:50:25", 40.818550713953165, -73.95156140546665, "crowdsourced", "CCNY Shuttle 1"],
    ["PQwwqv76ALwwd91szMYx", "Manhattan", "", "691", "St Nicholas Ave", 1, "2024-03-22 21:44:55", 40.82357690300404, -73.94518716294994, "crowdsourced", "CCNY Shuttle 1"],
    ["r1HvGHvuGNO4Eoxh8k1Q", "Manhattan", "", "", "W 145th St", 1, "2024-03-22 21:41:35", 40.82469630000001, -73.9462731, "crowdsourced", "CCNY Shuttle 1"],
    ["MuoV1dFHwmjHm9RtvMws", "Manhattan", "", "", "Convent Ave", 1, "2024-03-22 21:32:37", 40.8170297, -73.9524472, "crowdsourced", "CCNY Shuttle 1"],
    ["D7Vxl2T0nef1D8GTzEZK", "Manhattan", "", "285", "St Nicholas Ave", 1, "2024-03-22 21:31:35", 40.810389150020086, -73.9530721499155, "crowdsourced", "CCNY Shuttle 1"],
    ["xfXvscG8dhWSt731wXQy", "Manhattan", "", "283", "St Nicholas Ave", 1, "2024-03-22 21:28:16", 40.8104731, -73.9531392, "crowdsourced", "CCNY Shuttle 1"],
    ["VCt7E7lFfeGiNO4Gafku", "The City College of New York", "Manhattan", "160", "Convent Ave", 1, "2024-03-22 21:21:35", 40.81983768114043, -73.94985901502133, "crowdsourced", "CCNY Shuttle 1"],
    ["QlwHHOF1gAhleaWudKd0", "The City College of New York", "Manhattan", "181", "Convent Ave", 1, "2024-03-22 21:20:31", 40.8197491, -73.9497746, "crowdsourced", "CCNY Shuttle 1"],
    ["Ye0EY5NZNvCmFywpax4N", "Manhattan", "", "270", "Convent Ave", 1, "2024-03-22 21:15:01", 40.82189920000002, -73.9482346, "crowdsourced", "CCNY Shuttle 1"],
    ["hntqBlexNUer8XJCeg59", "145 St", "Manhattan", "394–446", "W 145th St", 1, "2024-03-22 21:12:51", 40.824022, -73.9448658, "crowdsourced", "CCNY Shuttle 1"],
    ["GhGt4C1x3MEsFJIyehox", "The City College of New York", "Manhattan", "221", "Convent Ave", 1, "2024-03-22 21:08:22", 40.8204679, -73.9485402, "crowdsourced", "CCNY Shuttle 1"],
    ["rMPYLO4yQNbGpxCD6Xx0", "Manhattan", "", "168", "Morningside Ave", 1, "2024-03-22 21:07:12", 40.81249360000108, -73.95325623332698, "crowdsourced", "CCNY Shuttle 1"]
]

# Define ENDPOINT, CLIENT_ID, PATH_TO_CERTIFICATE, PATH_TO_PRIVATE_KEY, PATH_TO_AMAZON_ROOT_CA_1, MESSAGE, TOPIC, and RANGE
ENDPOINT = "ay4jya4xw22o7-ats.iot.us-east-1.amazonaws.com"
CLIENT_ID = "testDevice"
PATH_TO_CERTIFICATE = "certificates/test_arduino.cert.pem"
PATH_TO_PRIVATE_KEY = "certificates/test_arduino.private.key"
PATH_TO_AMAZON_ROOT_CA_1 = "certificates/AmazonRootCA1.pem"
MESSAGE = "Hello World"
TOPIC = "test/testing"

# Spin up resources
event_loop_group = io.EventLoopGroup(1)
host_resolver = io.DefaultHostResolver(event_loop_group)
client_bootstrap = io.ClientBootstrap(event_loop_group, host_resolver)

mqtt_connection = mqtt_connection_builder.mtls_from_path(
            endpoint=ENDPOINT,
            cert_filepath=PATH_TO_CERTIFICATE,
            pri_key_filepath=PATH_TO_PRIVATE_KEY,
            client_bootstrap=client_bootstrap,
            ca_filepath=PATH_TO_AMAZON_ROOT_CA_1,
            client_id=CLIENT_ID,
            clean_session=False,
            keep_alive_secs=6
            )
print("Connecting to {} with client ID '{}'...".format(
        ENDPOINT, CLIENT_ID))
# Make the connect() call

connect_future = mqtt_connection.connect()
# Future.result() waits until a result is available
connect_future.result()
print("Connected!")
# Publish message to server desired number of times.
print('Begin Publish')
for i in range (len(data)):
    message = {"message" : data[i]}
    #print("before connect")
    mqtt_connection.publish(topic=TOPIC, payload=json.dumps(data[i]), qos=mqtt.QoS.AT_LEAST_ONCE)
    print("Published: '" + json.dumps(message) + "' to the topic: " + "'test/testing'")
    t.sleep(3)
print('Publish End')
disconnect_future = mqtt_connection.disconnect()
disconnect_future.result()