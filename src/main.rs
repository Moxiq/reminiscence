use std::process::Command;
use chrono::{self, DateTime, Local};
use pulsectl::controllers::types::DeviceInfo;
use pulsectl::controllers::{SinkController, DeviceControl};
use std::fs;
use std::io;
use x11rb::rust_connection::RustConnection;
use x11rb::connection::Connection;
use x11rb::errors::ReplyOrIdError;
use x11rb::protocol::xproto::{ConnectionExt, GetGeometryReply, Window, AtomEnum, MapState};

const FPS: u32 = 60;

// Check this for hardware acceleration
// https://docs.nvidia.com/video-technologies/video-codec-sdk/12.0/ffmpeg-with-nvidia-gpu/index.html

// TODO: Some window names does not seem to render correctly, utf-8 stuff? e.g spotify
//      Check this for how xwininfo displays name (line 470)
//      https://github.dev/idunham/xutils/blob/master/xwininfo.c

fn main() {
    let ffmpeg_path = "./bin/bin/ffmpeg";
    let output_dir = "./Recordings";
    let use_hwaccel = true;
    let use_time = false;
    let duration = 15;

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

    let window = get_window().expect("Invalid window");

    println!("Selected window: {},{},{},{},{}", window.name, window.geometry.x, window.geometry.y, window.geometry.width, window.geometry.height);

    // Get audio device
    let audio_device: DeviceInfo = get_audio_device_input();

    println!("Using audio device: {}", audio_device.description.as_ref().unwrap());

    let mut cmd = Command::new(ffmpeg_path);
        cmd.args(["-video_size", &format!("{}x{}", window.geometry.width, window.geometry.height)]);
        cmd.args(["-framerate", &format!("{FPS}")]);
        cmd.args(["-f", "x11grab"]);
        cmd.args(["-i", &format!(":0.0+{},{}", window.geometry.x, window.geometry.y)]);
    if use_time { // Video time
        cmd.args(["-t", &format!("{duration}")]); 
    }
        cmd.args(["-f", "pulse"]);
        cmd.args(["-ac", "2"]);
        cmd.args(["-i", &format!("{}", audio_device.index)]);
    if use_time { // Sound time
        cmd.args(["-t", &format!("{duration}")]); 
    }
    if use_hwaccel { 
        cmd.args(["-c:v", "h264_nvenc"]); 
    }
        cmd.arg(output_path);

    println!("{}", format!("{:?}", cmd).replace("\"", ""));

    let child = match cmd.spawn() {
        Ok(child) => {child},
        Err(err) => {
            eprintln!("Failed to execute command: {}", err);
            return;
        },
    };

    let output = child.wait_with_output().expect("Failure");
    println!("{:?}", output);
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
#[derive(Debug, Clone)]
struct MyWindow {
    id: u32,
    name: String,
    geometry: GetGeometryReply,
}

// Check this
// https://unix.stackexchange.com/questions/494182/what-does-net-mean-on-x11-window-properties
fn get_window() -> Option<MyWindow> {
    // Establish a connection to the X server
    let (conn, screen_num) = x11rb::connect(None).unwrap();
    let screen = &conn.setup().roots[screen_num];

    // Get the root window
    let root_window = screen.root;

    // Query the tree of windows
    let tree = x11rb::protocol::xproto::query_tree(&conn, root_window).unwrap().reply().unwrap();
    let mut windows: Vec<MyWindow> = Vec::new();

    // Add all windows to the list
    for parent in tree.children.iter() {
        let attr = conn.get_window_attributes(*parent).unwrap().reply().unwrap();
        // Only add windows that are viewable
        if attr.map_state != MapState::VIEWABLE {
            continue;
        }

        // Get the recursive children of this viewable window
        for child in get_children_rec(&conn, *parent) {
            match get_window_name(&conn, child) {
                // Only go through windows that has a name
                Some(name) => {
                    // Let the geometry be of the parent window (otherwise the offset will be 0)
                    let geometry = get_window_geometry(&conn, *parent).unwrap();
                    windows.push(MyWindow { id: *parent, name, geometry });
                },
                None => {},
            }
        }
    }

    println!("Select Window: ");
    // Iterate through each window to get its geometry (coordinates)
    for (i, window) in windows.iter().enumerate() {
        println!("[{}] {}, {},{}, {}, {} ({})", i+1, window.name, window.geometry.x, window.geometry.y, window.geometry.width, window.geometry.height, window.id);
    }

    let mut input = String::new();
    io::stdin().read_line(&mut input).expect("failure");

    let index: usize = match input.trim().parse::<usize>() {
        Ok(v) => v-1,
        Err(_) => {
            println!("Invalid input, choosing default audio device");
            return None;
        },
    };

    if index >= windows.len() {
        println!("Invalid index, choosing default audio device");
        return None;

    }

    Some(windows[index].clone())
}

// Helper function to get the geometry of a window
fn get_window_geometry(
    conn: &RustConnection,
    window: Window,
) -> Result<GetGeometryReply, ReplyOrIdError> {
    let geometry = conn.get_geometry(window)?;
    Ok(geometry.reply()?)
}

fn get_window_name(conn: &RustConnection, window: Window) -> Option<String> {
    // Get the property
    let prop_reply = conn
        .get_property(false, window, AtomEnum::WM_NAME, AtomEnum::STRING, 0, u32::MAX)
        .expect("Failed to get property")
        .reply()
        .expect("Failed to get property reply");

    // Check if we have a result
    if prop_reply.value_len == 0 {
        return None;
    }

    Some(String::from_utf8_lossy(&prop_reply.value).into_owned())
}


// Recursively get all children of a window to a vector
fn get_children_rec(conn: &RustConnection, window: Window) -> Vec<Window> {
    // Query the tree structure to get the list of child windows
    let tree_reply = conn.query_tree(window).unwrap().reply().unwrap();
    let children = tree_reply.children;

    // Iterate through each child window and recursively query its children
    let mut result = Vec::new();
    for child in children {
        result.push(child);
        let grandchildren = get_children_rec(conn, child);
        result.extend(grandchildren);
    }

    result
}

