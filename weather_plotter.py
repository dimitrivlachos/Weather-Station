# Path: weather_plot.py
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

# Read data from csv file
def read_csv(filename):
    df = pd.read_csv(filename)
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    return df

# Plot data
def plot_data(df):
    fig, ax = plt.subplots(3, 1, figsize=(10, 8), sharex=True)
    ax[0].plot(df['timestamp'], df['temperature'], 'r')
    ax[0].set_ylabel('Temperature (C)')
    ax[1].plot(df['timestamp'], df['humidity'], 'b')
    ax[1].set_ylabel('Humidity (%)')
    ax[2].plot(df['timestamp'], df['pressure'], 'g')
    ax[2].set_ylabel('Pressure (hPa)')
    # format x-axis ticks as dates
    ax[2].xaxis.set_major_formatter(mdates.DateFormatter('%d/%m'))
    # angle x-axis labels
    plt.setp(ax[2].xaxis.get_majorticklabels(), rotation=45)
    # set x-axis ticks every day
    ax[2].xaxis.set_major_locator(mdates.DayLocator())
    
    # Save plot to file
    plt.savefig('data/weather.png', dpi=300)

if __name__ == '__main__':
    filename = "data/weather.csv"
    df = read_csv(filename)
    plot_data(df)