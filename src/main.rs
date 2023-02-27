use chrono::{DateTime, Utc};
use csv::{Writer, WriterBuilder};
use reqwest::{Client, Error};
use serde::{Deserialize, Serialize};

#[derive(Deserialize)]
struct Readings {
    temperature: f32,
    humidity: f32,
    pressure: f32,
}

#[derive(Deserialize)]
struct Response {
    Readings: Readings,
}

#[derive(Serialize)]
struct ReadingRecord {
    timestamp: String,
    temperature: f32,
    humidity: f32,
    pressure: f32,
}

async fn get_readings(client: &Client) -> Result<Readings, Error> {
    let response = client.get("http://192.168.0.190/json").send().await?;
    let response_text = response.text().await?;
    let response: Response = serde_json::from_str(&response_text)?;
    Ok(response.Readings)
}

fn save_reading(csv_writer: &mut Writer<&mut std::fs::File>, readings: &Readings, timestamp: DateTime<Utc>) -> Result<(), csv::Error> {
    let record = ReadingRecord {
        timestamp: timestamp.to_rfc3339(),
        temperature: readings.temperature,
        humidity: readings.humidity,
        pressure: readings.pressure,
    };
    csv_writer.serialize(record)?;
    csv_writer.flush()?;
    Ok(())
}

#[tokio::main]
async fn main() -> Result<(), Error> {
    let csv_path = "readings.csv";
    let file = std::fs::OpenOptions::new().write(true).create(true).open(csv_path)?;
    let mut csv_writer = WriterBuilder::new().has_headers(false).from_writer(file);

    let client = Client::new();
    loop {
        let readings = get_readings(&client).await?;
        let timestamp = Utc::now();
        // if there is an error, just print it and continue
        if let Err(e) = save_reading(&mut csv_writer, &readings, timestamp) {
            println!("Error saving reading: {}", e);
        }

        tokio::time::delay_for(std::time::Duration::from_secs(30 * 60)).await;
    }
}