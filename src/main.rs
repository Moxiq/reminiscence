use std::{process::Command};
use chrono::{self, DateTime, Local};
use std::fs;

const FPS: u32      = 30;
const RES_X: u32    = 2560;
const RES_Y: u32    = 1440;
const OFFSET_X: u32 = 1920;
const OFFSET_Y: u32 = 0;

fn main() {
    let ffmpeg_path = "/usr/local/bin/ffmpeg";
    let output_dir = "./Recordings";
    let duration = 10;

    // Create output directory if it does not exist
    if !fs::metadata(output_dir).is_ok() {
        fs::create_dir(output_dir).unwrap();
    }

    // Get the current date and time in the local timezone
    let local: DateTime<Local> = Local::now();

    // Format the date and time
    let formatted_date = local.format("%m-%d-%H_%M_%S");

    // Full output path
    let output_path = format!("{output_dir}/{formatted_date}.mp4");

    let output = Command::new(ffmpeg_path)
        .args(["-video_size", &format!("{RES_X}x{RES_Y}")])
        .args(["-framerate", &format!("{FPS}")])
        .args(["-f", "x11grab"])
        .args(["-i", &format!(":0.0+{OFFSET_X},{OFFSET_Y}")])
        .args(["-t", &format!("{duration}")])
        .arg(output_path)
        .status()
        .expect("Failure");

    println!("Status: {}", output);
}
