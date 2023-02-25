import requests
import json
import csv
import time
import os
import datetime

# Get data from weather station
def get_json(url):
    r = requests.get(url)
    if r.status_code == 200:
        return json.loads(r.text)
    else:
        print(f'Error: {r.status_code}')

# Save data to csv file with timestamp
def save_to_csv(data, filename):
    # check if file exists
    file_exists = os.path.isfile(filename)
    with open(filename, 'a') as f:
        writer = csv.writer(f)
        # write header row if file is new
        if not file_exists:
            writer.writerow(['timestamp', 'temperature', 'humidity', 'pressure'])
        row = [time.strftime("%Y-%m-%d %H:%M:%S"), data['Readings']['temperature'], data['Readings']['humidity'], data['Readings']['pressure']]
        writer.writerow(row)

if __name__ == '__main__':
    print(os.getcwd())
    url = 'http://192.168.0.190/json'
    filename = "weather.csv"
    while True:
        now = datetime.datetime.now()
        if now.minute == 0: # wait until next hour
            wait_time = 3600 - now.second # seconds until next hour
        elif now.minute == 30: # wait until next half hour
            wait_time = 1800 - now.second # seconds until next half hour
        else: # wait until next half or full hour
            wait_time = (30 - now.minute % 30) * 60 - now.second # seconds until next half or full hour
        time.sleep(wait_time)
        data = get_json(url)
        save_to_csv(data, filename)
        print(f'Data saved to {filename} at {time.strftime("%H:%M:%S")}')