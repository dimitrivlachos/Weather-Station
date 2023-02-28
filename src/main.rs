use serde::{Serialize, Deserialize};
use color_eyre::{self, Result};
use chrono::{DateTime, Utc};
use std::fs::OpenOptions;
use std::io::{Write};
use std::path::Path;
use reqwest;

// Struct to hold the readings from the ESP8266
#[derive(Serialize, Deserialize, Debug)]
struct Readings {
    temperature: f32,
    humidity: f32,
    pressure: f32,
}

// Makes a request to the ESP8266 and returns the readings
async fn make_request() -> Result<Readings> {
    let response = reqwest::Client::new()
    .get("http://192.168.0.190/json")
    .send()
    .await?;

    let json_str = response.text().await?;
    
    #[derive(Deserialize)]
    struct ApiResponse {
        #[serde(rename = "Readings")]
        readings: Readings
    }
    let json: ApiResponse = serde_json::from_str(&json_str)?;
    Ok(json.readings)
}

// Writes the data obtained from the make_request() function to a CSV file
fn write_to_csv(data: &Readings) -> Result<()> {
    let path = Path::new("data/weather.csv");

    // Check if file exists, if not create it and add headers
    let mut file = OpenOptions::new()
        .write(true)
        .create(true)
        .append(true)
        .open(path)?;

    if file.metadata()?.len() == 0 {
        writeln!(file, "timestamp,temperature,humidity,pressure")?;
    }

    // Get the current UTC time
    let timestamp: DateTime<Utc> = Utc::now();

    // Write the data with timestamp to the CSV file
    writeln!(file, "{},{},{},{}", timestamp, data.temperature, data.humidity, data.pressure)?;

    Ok(())
}

#[tokio::main]
async fn main() -> Result<()> {    
    color_eyre::install()?;
    // Make the request and write the data to the CSV file
    let reading = make_request().await?;
    write_to_csv(&reading)?;

    Ok(())
}