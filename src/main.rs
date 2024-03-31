use std::{process::Command};
use chrono::{self, DateTime, Local};
use pulsectl::controllers::types::DeviceInfo;
use pulsectl::controllers::{SinkController, DeviceControl};
use std::fs;
use std::io;

const FPS: u32      = 60;
const RES_X: u32    = 2560;
const RES_Y: u32    = 1440;
const OFFSET_X: u32 = 1920;
const OFFSET_Y: u32 = 0;

fn main() {
    let ffmpeg_path = "./bin/bin/ffmpeg";
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

    // Get audio device
    let audio_device: DeviceInfo = get_audio_device_input();

    println!("Using audio device: {}", audio_device.description.as_ref().unwrap());

    let output = Command::new(ffmpeg_path)
        .args(["-video_size", &format!("{RES_X}x{RES_Y}")])
        .args(["-framerate", &format!("{FPS}")])
        .args(["-f", "x11grab"])
        .args(["-i", &format!(":0.0+{OFFSET_X},{OFFSET_Y}")])
        .args(["-t", &format!("{duration}")])
        .args(["-t", &format!("{duration}")])
        .args(["-f", "pulse"])
        .args(["-ac", "2"])
        .args(["-i", &format!("{}", audio_device.index)])
        .arg(output_path)
        .status()
        .expect("Failure");

    println!("Status: {}", output);
}

// List user audio devices and let user pick audio device
fn get_audio_device_input() -> DeviceInfo {
    // create handler that calls functions on playback devices and apps
    let mut handler = SinkController::create().unwrap();

    let devices = handler
        .list_devices()
        .expect("Could not get list of playback devices.");

    println!("Select Playback Device: ");
    for (i, dev) in devices.iter().enumerate() {
        println!(
        "[{}] {}, Volume: {}",
        i+1,
        dev.description.as_ref().unwrap(),
        dev.volume.print());
    }

    let mut input = String::new();
    io::stdin().read_line(&mut input).expect("failure");

    let index: usize = match input.trim().parse::<usize>() {
        Ok(v) => v-1,
        Err(_) => {
            println!("Invalid input, choosing default audio device");
            return handler.get_default_device().expect("Failed to get default device");
        },
    };

    if index >= devices.len() {
        println!("Invalid index, choosing default audio device");
        return handler.get_default_device().expect("Failed to get default device");

    }

    devices[index].clone()
}
